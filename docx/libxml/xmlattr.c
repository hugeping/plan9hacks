#include <u.h>
#include <libc.h>
#include "xml.h"

Attr *
xmlattr(Xml *xp, Attr **root, Elem *parent, char *name, char *value)
{
	Attr *ap, *t;

	USED(xp);
	if((ap = xmlcalloc(xp, sizeof(Attr), 1)) == nil)
		sysfatal("no memory - %r\n");
	if(*root == nil){
		*root = ap;
	}
	else{
		for (t = *root; t->next; t = t->next)
			continue;
		t->next = ap;
	}
	ap->parent = parent;

	if(name)
		if((ap->name = xmlstrdup(xp, name, 1)) == nil)
			sysfatal("no memory - %r\n");

	if(value)
		if((ap->value = xmlstrdup(xp, value, 0)) == nil)
			sysfatal("no memory - %r\n");
	return ap;
}
