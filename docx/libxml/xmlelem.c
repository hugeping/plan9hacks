#include <u.h>
#include <libc.h>
#include "xml.h"

Elem *
xmlelem(Xml *xp, Elem **root, Elem *parent, char *name)
{
	Elem *ep, *t;

	USED(xp);
	if((ep = xmlcalloc(xp, sizeof(Elem), 1)) == nil)
		sysfatal("no memory - %r\n");
	if(! *root){
		*root = ep;
	}
	else{
		for (t = *root; t->next; t = t->next)
			continue;
		t->next = ep;
	}
	ep->parent = parent;
	if(name)
		if((ep->name = xmlstrdup(xp, name, 1)) == nil)
			sysfatal("no memory - %r\n");
	return ep;
}

