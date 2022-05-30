/*
 * Maze generator v1
 * Generatas n*n maze in two forms: with thin walls and without.
 * 
 */

#include <time.h>
#include <stdlib.h>
#include <stdio.h>

enum {
	n = N,

	wall = '#',
	wallfree = ' ',
	grand_tries = 1000,
	start_dir = 0
};

/*
 *	 0     1
 *	1 3   2 8
 *	 2     4
 */
struct cell {
	short x, y;
	char flags;
	struct cell* next[4];
};

struct pair {
	int x, y;
};

struct lc {
	struct pair p;
	struct lc *next;
};

struct lc_list {
	struct lc *first, *last;
	struct lc *prevlast;
};

void lc_list_init(struct lc_list *l);
void lc_list_add(struct lc_list *l, struct pair *p);
struct lc **lc_list_is(struct lc_list *l, struct pair *p);
void lc_list_rm(struct lc_list *l, struct lc **p);
void lc_list_print(struct lc_list *l);

void sym_maze_init(unsigned char (*arr)[2*n+1]);
void maze_init(struct cell (*m)[n]);
void maze_form(struct cell (*m)[n]);
void maze_print(struct cell (*m)[n]);

void maze_transform(struct cell (*ini)[n], unsigned char (*res)[2*n+1]);
void smaze_print(unsigned char (*res)[2*n+1]);

int main()
{
	struct cell maze[n][n];
	unsigned char res[2*n+1][2*n+1];

	srandom(time(NULL));
	sym_maze_init(res);
	maze_init(maze);
	maze_form(maze);
	maze_print(maze);
	
	maze_transform(maze, res);
	smaze_print(res);
	return 0;
}

void sym_maze_init(unsigned char (*arr)[2*n+1])
{
	int i, j;
	for (i = 0; i < 2*n+1; ++i) {
		for (j = 0; j < 2*n+1; ++j) {
			arr[i][j] = wall;
		}
	}
}

void form_row(struct cell (*m)[n], int row);
void form_col(struct cell (*m)[n], int col);

void maze_init(struct cell (*m)[n])
{
	int i;
	for (i = 0; i < n; ++i)
		form_row(m, i);
	for (i = 0; i < n; ++i)
		form_col(m, i);
}

void form_row(struct cell (*m)[n], int row)
{
	int i;
	for (i = 0; i < n; ++i)
		m[row][i].next[0] = row != 0 ? &m[row-1][i] : NULL;

	for (i = 0; i < n; ++i)
		m[row][i].next[2] = row != n-1 ? &m[row+1][i] : NULL;

	for (i = 0; i < n * n; ++i) {
		m[i / n][i % n].y = i / n;
		m[i / n][i % n].x = i % n;
		m[i / n][i % n].flags = 0;
	}
}

void form_col(struct cell (*m)[n], int col)
{
	int i;
	for (i = 0; i < n; ++i)
		m[i][col].next[1] = col != 0 ? &m[i][col-1] : NULL;

	for (i = 0; i < n; ++i)
		m[i][col].next[3] = col != n-1 ? &m[i][col+1] : NULL;
}

int get_random(struct cell (*m)[n], struct pair *p);
void gen_path(struct cell (*m)[n], struct pair *p);

void maze_form(struct cell (*m)[n])
{
	struct pair p;
	m[n-1][n-1].flags = 0xF0;	/* initial cell */
	while(get_random(m, &p))
		gen_path(m, &p);
}

int get_random(struct cell (*m)[n], struct pair *p)
{
	int i;
	for (i = 0; i < grand_tries; ++i) {
		p->x = random() % n;
		p->y = random() % n;
		if (m[p->y][p->x].flags == 0)
			return 1;
	}
	return 0;
}

void form_path(struct cell (*m)[n], struct pair *p, struct lc_list *l);
void adjust_path(struct cell (*m)[n], struct lc_list *l);

void gen_path(struct cell (*m)[n], struct pair *p)
{
	struct lc_list l;
	lc_list_init(&l);
	lc_list_add(&l, p);
	form_path(m, p, &l);
	adjust_path(m, &l);
}

void get_rand_next(struct cell *c, struct lc_list *l, struct pair *p);

void form_path(struct cell (*m)[n], struct pair *p, struct lc_list *l)
{
	while (m[l->last->p.y][l->last->p.x].flags == 0) {
		struct pair p;
		struct lc **lcp;
		/* lc_list_print(l); */
		get_rand_next(&m[l->last->p.y][l->last->p.x], l, &p);
		if ((lcp = lc_list_is(l, &p))) {
			/* printf("Loop caused by: %d, %d\n", p.y, p.x); */
			lc_list_rm(l, lcp);
		} else {
			lc_list_add(l, &p);
		}
	}
	/* lc_list_print(l); */
}

int get_next_free(struct cell *c, struct lc_list *l, int dir);

void get_rand_next(struct cell *c, struct lc_list *l, struct pair *p)
{
	int dir = start_dir;
	int iter = random() % 3;
	for (; iter > 0; --iter) {
		dir = get_next_free(c, l, dir);
		dir = (dir + 1) % 4;
	}
	dir = get_next_free(c, l, dir);

	p->x = c->next[dir]->x;
	p->y = c->next[dir]->y;
}

int get_next_free(struct cell *c, struct lc_list *l, int dir)
{
	for (;;) {
		int is_backward = c->next[dir] && l->prevlast &&
			(c->next[dir]->x == l->prevlast->p.x &&
			 c->next[dir]->y == l->prevlast->p.y);

		if (c->next[dir] != NULL && !is_backward)
			return dir;
		else
			dir = (dir + 1) % 4;
	}
}

int get_relation(struct pair *p1, struct pair *p2);

void adjust_path(struct cell (*m)[n], struct lc_list *l)
{
	struct lc *tmp = l->first; 
	for (; tmp->next; tmp = tmp->next) {
		int rel = get_relation(&tmp->p, &tmp->next->p);
		m[tmp->p.y][tmp->p.x].flags |= 1 << rel;
		m[tmp->next->p.y][tmp->next->p.x].flags |= 1 << (rel + 2) % 4;
		/*
		printf("[%d, %d]:[%d, %d]: %d, %d\n", tmp->p.y, tmp->p.x,
				tmp->next->p.y, tmp->next->p.x, rel, (rel + 2) % 4);
		*/
	}
}

int get_relation(struct pair *p1, struct pair *p2)
{
	if (p1->x + 1 == p2->x)
		return 3;
	if (p1->x - 1 == p2->x)
		return 1;
	if (p1->y + 1 == p2->y)
		return 2;
	if (p1->y - 1 == p2->y)
		return 0;
	return -1;
}

void maze_print(struct cell (*m)[n])
{
	int i, j;
	for (i = 0; i < n; ++i) {
		for (j = 0; j < n; ++j) {
			printf("[%c%c%c%c]",
					m[i][j].flags & 1<<0 ? 'U' : ' ',
					m[i][j].flags & 1<<1 ? 'L' : ' ',
					m[i][j].flags & 1<<2 ? 'D' : ' ',
					m[i][j].flags & 1<<3 ? 'R' : ' ');
		}
		printf("\n");
	}
}

void fill_towers(struct cell (*ini)[n], unsigned char (*res)[2*n+1]);
void fill_relations0(struct cell (*ini)[n], unsigned char (*res)[2*n+1]);
void fill_relations3(struct cell (*ini)[n], unsigned char (*res)[2*n+1]);

void maze_transform(struct cell (*ini)[n], unsigned char (*res)[2*n+1]) 
{
	fill_towers(ini, res);
	fill_relations0(ini, res);
	fill_relations3(ini, res);
}

void fill_towers(struct cell (*ini)[n], unsigned char (*res)[2*n+1])
{
	int i, j;
	for (i = 0; i < n; ++i) {
		for (j = 0; j < n; ++j) {
			ini[i][j].flags ? res[2*i+1][2*j+1] = wallfree : i;
		}
	}
}

void fill_relations0(struct cell (*ini)[n], unsigned char (*res)[2*n+1])
{
	int i, j;
	for (i = 1; i < n; ++i) {
		for (j = 0; j < n; ++j) {
			if(res[2*i+1][2*j+1] == wallfree &&
					res[2*i-1][2*j+1] == wallfree &&
					ini[i][j].flags & 1 << 0)
				res[2*i][2*j+1] = wallfree;
		}
	}
}

void fill_relations3(struct cell (*ini)[n], unsigned char (*res)[2*n+1])
{
	int i, j;
	for (i = 0; i < n; ++i) {
		for (j = 0; j < n - 1; ++j) {
			if(res[2*i+1][2*j+1] == wallfree &&
					res[2*i+1][2*j+3] == wallfree &&
					ini[i][j].flags & 1 << 3)
				res[2*i+1][2*j+2] = wallfree;
		}
	}
}

void smaze_print(unsigned char (*res)[2*n+1])
{
	int i, j;
	for (i = 0; i < 2*n+1; ++i) {
		for (j = 0; j < 2*n+1; ++j)
			printf("%c", res[i][j]);
		printf("\n");
	}
}

void lc_list_init(struct lc_list *l)
{
	l->first = l->last = NULL;
	l->prevlast = NULL;
}

void lc_list_add(struct lc_list *l, struct pair *p)
{
	if (l->first == NULL) {
		l->first = l->last = malloc(sizeof(struct lc));
	} else {
		l->prevlast = l->last;
		l->last = l->last->next = malloc(sizeof(struct lc));
	}
	l->last->next = NULL;
	l->last->p = *p;
}

struct lc **lc_list_is(struct lc_list *l, struct pair *p)
{
	struct lc **tmp = &l->first;
	for (; *tmp; tmp = &(*tmp)->next) {
		if ((*tmp)->p.x  == p->x && (*tmp)->p.y == p->y)
			return tmp;
	}
	return NULL;
}

void lc_list_rm(struct lc_list *l, struct lc **p)
{
	struct lc *tmp = (*p)->next;
	l->last = *p;

	while (tmp) {
		struct lc *tmp2 = tmp;
		tmp = tmp->next;
		free(tmp2);
	}
	l->last->next = NULL;
}

void lc_list_print(struct lc_list *l)
{
	struct lc *tmp = l->first;
	for (; tmp; tmp = tmp->next)
		printf("[%d, %d]", tmp->p.y, tmp->p.x);
	printf("\n");
}
