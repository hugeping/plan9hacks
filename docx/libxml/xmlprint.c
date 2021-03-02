#include <u.h>
#include <libc.h>
#include <bio.h>
#include <ctype.h>
#include "xml.h"

static void
prval(Biobuf *bp, char *s)
{
	char *p;
	Rune r;

	p = s;
	while(*p){
		p += chartorune(&r, p);
		switch(r){
		case L'&': Bprint(bp, "&amp;"); break;
		case L'<': Bprint(bp, "&lt;"); break;
		case L'>': Bprint(bp, "&gt;"); break;
		case L'"': Bprint(bp, "&quot;"); break;
		case L'\'': Bprint(bp, "&apos;"); break;
		default: 
			if(r >= L' ')
				Bprint(bp, "%C", r);
			else
				Bprint(bp, "&#x%04x;", r);
			break;
		}
	}
}

static void
_xmlprint(Biobuf *bp, Elem *ep, int in)
{
	Attr *ap;
	enum {indent = 4};

	for(; ep; ep = ep->next){
		Bprint(bp, "%*s<%s", in, "", ep->name);
	
		for (ap = ep->attrs; ap; ap = ap->next){
			Bprint(bp, " %s=\'", ap->name);
			prval(bp, ap->value);
			Bprint(bp, "\'");
		}

		if(ep->child){
			if(ep->pcdata){
				Bprint(bp, ">\n%*s\n", in+indent, "");
				prval(bp, ep->pcdata);
			}
			else
				Bprint(bp, ">\n");
			_xmlprint(bp, ep->child, in+indent);
			Bprint(bp, "%*s</%s>\n", in, "", ep->name);
		}
		else{
			if(ep->pcdata){
				Bprint(bp, ">\n%*s", in+indent, "");
				prval(bp, ep->pcdata);
				Bprint(bp, "\n%*s</%s>\n", in, "", ep->name);
			}
			else
				Bprint(bp, "/>\n");
		}
	}
}

void
xmlprint(Xml *xp, int fd)
{
	Biobuf bout;

	Binit(&bout, fd, OWRITE);
	if(xp->doctype){
		Bprint(&bout, "<?xml version='1.0' encoding='utf-8'?>\n");
		Bprint(&bout, "<!DOCTYPE %s>\n", xp->doctype);
	}
	else
		Bprint(&bout, "<?xml version='1.0' encoding='utf-8' standalone='yes'?>\n");
	_xmlprint(&bout, xp->root, 0);
	Bterm(&bout);
}
