#include <u.h>
#include <libc.h>
#include "xml.h"

#define Roundup(x, g)	(((x) + (unsigned)(g-1)) & ~((unsigned)(g-1)))

struct Xtree {
	Xtree *left;
	Xtree *right;
	char *str;
	int hits;
};

struct Xblock {
	Xblock *next;
	char *free;
	char *end;
};

static int Strdups, Commons, Unique, Memblocks;

static void *
getmem(Xml *xp, int len)
{
	int sz;
	Xblock *b;
	char *ret;

	len = Roundup(len, sizeof(long long));

	sz = xp->alloc.blksiz;		/* shorthand */
	b = xp->alloc.active;

	if(len > sz)
		sysfatal("store: object too big (%d > %d)\n", len, sz);

	if(xp->alloc.active == nil || b->free + len >= b->end){
		Memblocks++;
		b = mallocz(sizeof(Xblock) + sz, 0);
		b->free = (char *)&b[1];
		b->end = (char *)&b->free[sz];

		b->next = xp->alloc.active;
		xp->alloc.active = b;
	}

	ret = b->free;
	b->free += len;
	return ret;
}

static Xtree *
lookadd(Xml *xp, Xtree *t, char *str, Xtree **match)
{
	int n;

	if(t == nil){
		Unique++;
		t = getmem(xp, sizeof(Xtree) + strlen(str)+1);
		t->left = nil;
		t->right = nil;
		t->str = (char *)&t[1];
		strcpy(t->str, str);
		*match = t;
		t->hits = 1;
		return t;
	}

	if((n = strcmp(str, t->str)) == 0){
		*match = t;
		t->hits++;
	}
	if(n < 0)
		t->left = lookadd(xp, t->left, str, match);
	if(n > 0)
		t->right = lookadd(xp, t->right, str, match);
	return t;
}

char *
xmlstrdup(Xml *xp, char *str, int iscommon)
{
	char *s;
	Xtree *t;

	Strdups++;
	if(iscommon){
		Commons++;
		xp->alloc.root = lookadd(xp, xp->alloc.root, str, &t);
		return t->str;
	}

	s = getmem(xp, strlen(str)+1);
	return strcpy(s, str);
}

void *
xmlcalloc(Xml *xp, int n, int m)
{
	void *v;

	v = getmem(xp, n * m);
	memset(v, 0, n * m);
	return v;
}

void *
xmlmalloc(Xml *xp, int n)
{
	return getmem(xp, n);
}


void
_Xheapstats(void)
{
	fprint(2, "total=%d common=%d -> unique=%d rare=%d memblocks=%d\n",
		Strdups, Commons, Unique, Strdups - Commons, Memblocks);
}

void
_Xheapfree(Xml *xp)
{
	Xblock *b, *n;

	for(b = xp->alloc.active; b; b = n){
		n = b->next;
		if(xmldebug)
			memset(b, 0x7e, xp->alloc.blksiz);
		free(b);
	}

}

