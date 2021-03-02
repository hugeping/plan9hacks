#include <u.h>
#include <libc.h>
#include <bio.h>
#include "xml.h"

/*
 * search for element, starting at ep.
 * if attr!=nil the elem must have an attribute attr
 * if value!=nil then the elem must have attr=value
 */

Elem *
xmllook(Elem *ep, char *path, char *attr, char *value)
{
	char *p;
	Elem *t;
	Attr *ap;

	if (path == nil)
		return nil;
	if (*path == '/')
		path++;
	if ((p = strchr(path, '/')) == nil)
		if((p = strchr(path, 0)) == nil)
			return nil;			// shut up lint !

	for(; ep; ep = ep->next)
		if (strncmp(ep->name, path, p-path) == 0){
			if (*p == '/'){
				if (ep->child)
					if ((t = xmllook(ep->child, p, attr, value)) != nil)
						return t;
				continue;
			}
			if (attr == nil)
				return ep;
			for (ap = ep->attrs; ap; ap = ap->next)
				if (strcmp(ap->name, attr) == 0){
					if (value == nil)
						return ep;
					if (strcmp(ap->value, value) == 0)
						return ep;
				}
			if (ep->child)
				if ((t = xmllook(ep->child, p, attr, value)) != nil)
					return t;
		}
	return nil;
}
