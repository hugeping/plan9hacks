#include <u.h>
#include <libc.h>
#include "xml.h"

/*
 * search for element, starting at ep.
 */

Elem *
xmlfind(Xml *xp, Elem *ep, char *path)
{
	char *p;
	Elem *t;

	USED(xp);

	if (path == nil)
		return nil;
	if (*path == '/')
		path++;
	if ((p = strchr(path, '/')) == nil)
		if((p = strchr(path, 0)) == nil)
			return nil;		// shut up lint !

	for(; ep; ep = ep->next)
		if (strncmp(ep->name, path, p-path) == 0){
			if (*p == 0)
				return ep;
			if (! ep->child)
				continue;
			if ((t = xmlfind(xp, ep->child, p)) != nil)
				return t;
		}
	return nil;
}
