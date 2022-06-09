#include <curses.h>

#include <math.h>

#include <sys/param.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

struct line_seg_given {
	double x1, y1;
	double x2, y2;
};

struct line_seg_gen {
	double a, b, c;
	double from, to;
	int what; /* 1 - x, 2 - y */
};

int frame_number = 0;

enum {
	side_rays = 50,
	max_walls = 100000,
	cell_side = 1,

	k_up = ',',
	k_down = 'o',
	k_left = 'a',
	k_right = 'e',

	k_rotate_left = 'h',
	k_rotate_right = 't'
};

const int mx = 61;
const int my = 61;

const char *map =
	#include "maze.inc"
;


const double side_view = M_PI / 3;

int wallcount = 0;
struct line_seg_gen walls_gen[max_walls];

struct player {
	double x, y;
	double alpha;	/* 0 to 2*pi */
};

struct pair {
	double x, y;
};

void sig_alarm_handler(int n)
{
	frame_number = 0;
	alarm(1);
}

int deq(double a, double b) {
	return fabs(a - b) < 10E-4;
}


void convert_wall(struct line_seg_gen *p, struct line_seg_given *g)
{
		double dx = g->x2 - g->x1;
		double dy = g->y2 - g->y1;

		p->what = 1;

		if (deq(dy, 0)) {
			p->a = 0;
			p->b = 1;
			p->c = - g->y1;
		} else if (deq(dx, 0)) {
			p->what = 2;
			p->a = 1;
			p->b = 0;
			p->c = - g->x1;
		} else {
			p->a = 1 / dx;
			p->b = -1 / dy;
			p->c = - g->x1 / dx + g->y1 / dy;
		}

		if (p->what == 1) {
			p->from = MIN(g->x1, g->x2);
			p->to = MAX(g->x1, g->x2);
		} else {
			p->from = MIN(g->y1, g->y2);
			p->to = MAX(g->y1, g->y2);
		}

		/* printf("%.2lf, %.2lf, %.2lf: F:%d, %lf, %lf\n", p->a, p->b, p->c, p->what, p->from, p->to); */
}

/*
   0
  1 3
   2
 */

int get_next(int x, int y, int dir)
{
	switch(dir) {
	case 0:
		return y == my ? '.' : map[(y+1) * mx + x];
	case 1:
		return x == 0 ? '.' : map[y * mx + x - 1];
	case 2:
		return y + 1 == 0 ? '.' : map[(y-1) * mx + x];
	case 3:
		return x + 1 == mx ? '.' : map[y * mx + x + 1];
	}
	return -1;
}

void convert_walls()
{
	int x, y;
	for (y = 0; y < my; ++y) {
		for (x = 0; x < mx; ++x) {
			double h = (double)cell_side / 2;
			double cx = x * cell_side + h;
			double cy = y * cell_side + h;

			struct line_seg_given up = { cx, cy, cx, cy + h };
			struct line_seg_given down = { cx, cy, cx, cy - h };
			struct line_seg_given left = { cx, cy, cx - h, cy };
			struct line_seg_given right = { cx, cy, cx + h, cy };

			if (map[mx * y + x] != '#')
				continue;

			if (get_next(x, y, 0) == '#') {
				convert_wall(walls_gen + wallcount, &up);
				++wallcount;
			}
			if (get_next(x, y, 1) == '#') {
				convert_wall(walls_gen + wallcount, &left);
				++wallcount;
			}
			if (get_next(x, y, 2) == '#') {
				convert_wall(walls_gen + wallcount, &down);
				++wallcount;
			}
			if (get_next(x, y, 3) == '#') {
				convert_wall(walls_gen + wallcount, &right);
				++wallcount;
			}
		}
	}
}

int check(struct line_seg_gen *l, double x, double y)
{
	if (l->what == 1)
		return x >= l->from && x <= l->to;
	else
		return y >= l->from && y <= l->to;
}

double calc_dist(struct pair *start, struct pair *dir, struct line_seg_gen *l)
{
	double free_part = l->a * start->x + l->b * start->y + l->c;
	double mul = l->a * dir->x + l->b * dir->y;
	double t, dist;

	if (deq(mul, 0))
		return 10E7; /* Parallel */

	t = -free_part / mul;
	dist = t;
	if (t >= 0 && check(l, start->x + dir->x * t, start->y + dir->y * t))
		return dist;
	else
		return 10E7;
}

double calc_closest_dist(struct pair *start, struct pair *dir)
{
	int i;
	double min = 10E7;
	for (i = 0; i < wallcount; ++i) {
		double res = calc_dist(start, dir, walls_gen + i);
		if (res < min)
			min = res;
	}
	return min;
}

void throw_rays(struct player *p, double *dist, double *dist_to_sides)
{
	int i;
	for (i = 0; i < 2*side_rays + 1; ++i) {
		double alpha = (double)(side_rays - i) / side_rays * side_view;
		double angle = p->alpha + alpha;
		struct pair start = { p->x, p->y };
		struct pair dir = { cos(angle), sin(angle) };
		dist[i] = cos(alpha) * calc_closest_dist(&start, &dir);
		/* printf("%d, %lf, %lf\n", i, angle, dist[i]); */
	}
	dist_to_sides[0] = dist[side_rays];
	for (i = 1; i < 4; ++i) {
		double angle = p->alpha + M_PI / 2 * i;
		struct pair start = { p->x, p->y };
		struct pair dir = { cos(angle), sin(angle) };
		dist_to_sides[i] = calc_closest_dist(&start, &dir);
	}
}

int _gcd(int a, int b)
{
	return a == 0 ? b : _gcd(b % a, a);
}

int _lcm(int a, int b)
{
	return a * b / _gcd(a, b);
}

void extend(double *values, double *dist, int ini, int new)
{
	int lcm = _lcm(ini, new);

	double *buffer = malloc(sizeof(double) * lcm);
	int i;
	for (i = 0; i < lcm; ++i) {
		buffer[i] = dist[i / (lcm / ini)];
	}

	for (i = 0; i < new; ++i) {
		int j;
		double sum = 0;
		for (j = 0; j < lcm/new; ++j)
			sum += buffer[lcm/new * i + j];
		values[i] = sum / (double)(lcm / new);
	}
	free(buffer);
}

int draw_col(int col, int rows, int v) {
	int d2 = (rows - v) / 2;
	int should = (rows - v) % 2;
	int i;
	int c;

	if (v > 30)
		c = '@';
	else if (v > 20)
		c = '#';
	else if (v > 10)
		c = '%';
	else if (v > 5)
		c = '*';
	else
		c = '.';

	for (i = 0; i < d2; ++i) {
		move(i, col);
		addch(' ');
	}
	for (i = d2; i < d2 + v; ++i) {
		move(i, col);
		/*
		if ((i == d2 || i + 1 == d2 + v))
			addch('@');
		else
		*/
		addch(c);
	}
	for (i = d2 + v; i < d2 + v + d2 + should; ++i) {
		move(i, col);
		addch(' ');
	}
	return v == 0;
}

void clear_scr(int rows, int cols)
{
	int i = 0;
	move(0, 0);
	for (i = 0; i < rows * cols; ++i)
		addch(' ');
}

void draw(double *dist, int rows, int cols)
{
	double values[cols];
	int i;

	extend(values, dist, 2*side_rays + 1, cols);
	for (i = 0; i < cols; ++i) {
		values[i] = (double)100 / pow(values[i], 1.005);
		values[i] = MIN(values[i], rows);
	}
	clear_scr(rows, cols);
	for (i = 0; i < cols; ++i) {
		move(0, i);
		int nv = floor(values[i]);
		draw_col(i, rows, nv);
	}
}

void draw_stats(struct player *p)
{
	move(0, 0);
	printw("%4d %6.2lf %6.2lf %6.2lf", frame_number, p->x, p->y, p->alpha);
}

void signal_set()
{
	struct sigaction sa;
	sigset_t set;
	sigemptyset(&set);
	sa.sa_handler = sig_alarm_handler;
	sa.sa_mask = set;
	sa.sa_flags = 0;
	sigaction(SIGALRM, &sa, NULL);
	alarm(1);
}

int main()
{
	struct player p = { 2 * cell_side, 2 * cell_side, M_PI / 2 };
	double dist[2*side_rays + 1];
	double dist_to_sides[4];
	int c;
	int rows, cols;

	convert_walls();

	initscr();
	cbreak();
	nodelay(stdscr, 0);
	keypad(stdscr, 1);
	curs_set(0);
	getmaxyx(stdscr, rows, cols);
	noecho();

	signal_set();

	for (;;) {
		throw_rays(&p, dist, dist_to_sides);
		draw(dist, rows, cols);
		++frame_number;
		draw_stats(&p);
		refresh();

		c = getch();

		if (c == KEY_F(2))
			break;

		if (c == KEY_RESIZE)
			getmaxyx(stdscr, rows, cols);

		if ((c == k_up || c == KEY_UP) && dist_to_sides[0] > 1) {
			p.x += cos(p.alpha) * 1;
			p.y += sin(p.alpha) * 1;
		}
		if ((c == k_down || c == KEY_DOWN) && dist_to_sides[2] > 1) {
			p.x -= cos(p.alpha) * 1;
			p.y -= sin(p.alpha) * 1;
		}

		if ((c == k_left || c == KEY_LEFT) && dist_to_sides[1] > 1) {
			p.x += cos(p.alpha + M_PI / 2) * 1;
			p.y += sin(p.alpha + M_PI / 2) * 1;
		}

		if ((c == k_right || c == KEY_RIGHT) && dist_to_sides[3] > 1) {
			p.x += cos(p.alpha - M_PI / 2) * 1;
			p.y += sin(p.alpha - M_PI / 2) * 1;
		}

		if (c == k_rotate_left || c == '<')
			p.alpha += M_PI / 20;
		if (c == k_rotate_right || c == '>')
			p.alpha -= M_PI / 20;

		if (p.alpha < 0)
			p.alpha += M_PI * 2;

		if (p.alpha >= M_PI * 2)
			p.alpha -= M_PI * 2;
	}

	endwin();
	return 0;
}
