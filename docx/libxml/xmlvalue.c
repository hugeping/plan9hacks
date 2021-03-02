#include <u.h>
#include <libc.h>
#include "xml.h"

char *
xmlvalue(Elem *ep, char *name)
{
	Attr *ap;

	/*
	 * This enables the common idiom: xmlvalue(xmllook(), name);
	 */
	if (ep == nil)
		return nil;
	for(ap = ep->attrs; ap; ap = ap->next)
		if(strcmp(ap->name, name) == 0)
			return ap->value;
	return nil;
}
