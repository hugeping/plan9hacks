#include <u.h>
#include <libc.h>
#include "xml.h"

Xml *
xmlnew(int blksize)
{
	Xml *xp;

	xp = mallocz(sizeof(Xml), 1);
	xp->alloc.blksiz = blksize;
	if(xp == nil)
		return nil;
	return xp;
}
