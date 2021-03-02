#include <u.h>
#include <libc.h>
#include "xml.h"

void
xmlfree(Xml *xp)
{
	_Xheapfree(xp);
	free(xp);
}
