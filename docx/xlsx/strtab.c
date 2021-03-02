#include <u.h>
#include <libc.h>
#include <bio.h>
#include <xml.h>
#include "xlsx.h"

typedef struct Strtab Strtab;
struct Strtab {
	int idx;
	char *str;
	Strtab *left;
	Strtab *right;
};

static Strtab *Root;

static char *
look(Strtab *st, int idx)
{
	if(st == nil)
		return nil;
	if(st->idx < idx)
		return look(st->left, idx);
	if(st->idx > idx)
		return look(st->right, idx);
	return st->str;
}

char *
looktab(int idx)
{
	char *s;
	static char buf[16];

	if((s = look(Root, idx)) == nil){
		snprint(buf, sizeof(buf), "<%d>", idx);
		return buf;
	}
	return s;
}


static Strtab *
add(Strtab *st, int idx, char *str)
{
	int n;

	if(st == nil){
		st = malloc(sizeof(Strtab));
		if(st == nil)
			sysfatal("no memory for Strtab\n");
		st->idx = idx;
		st->str = strdup(str);
		if(st->str == nil)
			sysfatal("no memory for Strtab\n");
		st->left = st->right = nil;
		return st;
	}

	if(st->idx == idx){
		n = strlen(st->str) + strlen(str) + 1;
		n += 32;
		st->str = realloc(st->str, n);
		if(st->str == nil)
			sysfatal("no memory for Strtab\n");
		strcat(st->str, str);
	}

	if(st->idx < idx)
		st->left = add(st->left, idx, str);
	if(st->idx > idx)
		st->right = add(st->right, idx, str);
	return st;
}


static void
run(Elem *ep, int *idxp)
{
	for(; ep; ep = ep->next)
		if(strcmp(ep->name, "t") == 0 && ep->pcdata)
			Root = add(Root, (*idxp)++, ep->pcdata);
}

void
stringindex(Elem *ep, int *idxp)
{
	for(; ep; ep = ep->next){
		if(strcmp(ep->name, "r") == 0 && ep->child)
			run(ep->child, idxp);
		if(strcmp(ep->name, "t") == 0 && ep->pcdata)
			Root = add(Root, (*idxp)++, ep->pcdata);
	}
}
	
void
mktab(Elem *ep)
{
	int idx;

	idx = 0;
	for(; ep; ep = ep->next)
		if(strcmp(ep->name, "si") == 0 && ep->child)
			stringindex(ep->child, &idx);
}

