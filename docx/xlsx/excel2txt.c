#include <u.h>
#include <libc.h>
#include <bio.h>
#include <xml.h>
#include <ctype.h>
#include "xlsx.h"

enum { Widefield = 40 };		/* wrap fields longer than this in tbl mode */

char *Currency = "£";			/* currency symbol */
int Epoch1904 = 1;				/* disable "as broken as Lotus-123" mode (yes really) */

static char *Strtype[] = { "numeric", "inline", "shared", "boolean", "string", "error", "date" };

static int Ncols;				/* number of columns in sheet */
static int *Colwidth;			/* width of each column */
static int Defwidth;			/* default column width if not set in aobve */

static char *Delim;				/* intra cell delimiter - implies fields are not padded */
static int Blanklines;			/* allow blank lines in output */
static int Tbl;					/* try to format for tbl(1) */
static int Trunc;				/* crop long fields */
static int Doquote;				/* quote fields using %q */
static char *Colrange = nil;	/* range of collums requested */

static void
prnt(Biobuf *bp, char *str, int *remainp)
{
	int l;

	l = strlen(str);
	if(Trunc && l > *remainp -1){
		if(Doquote)
			Bprint(bp, "%.*q…", *remainp -2, str);
		else
			Bprint(bp, "%.*s…", *remainp -2, str);
	}
	else{
		if(Doquote)
			Bprint(bp, "%q", str);
		else
			Bprint(bp, "%s", str);
	}

	if(l > *remainp)
		*remainp = 1;
	else
		*remainp -= l;
}

static void
rd_text(Biobuf *bp, Elem *ep, int *widthp)
{
	prnt(bp, ep->pcdata, widthp);
}

static void
rd_inlinestr(Biobuf *bp, Elem *ep, int *widthp)
{
	for(; ep; ep = ep->next)
		if(strcmp(ep->name, "t") == 0 && ep->child)
			rd_text(bp, ep->child, widthp);
			
}

int
skip(char *range, int here)
{
	int n, s;
	char *p;

	if (! range)
		return 0;

	s = -1;
	p = range;
	while(1){
		n = strtol(p, &p, 10);
		switch(*p){
		case 0:
			if(n == here)
				return 0;
			if(s != -1 && here > s && here < n)
				return 0;
			return 1;
		case ',':
			if(n == here)
				return 0;
			if(s != -1 && here > s && here < n)
				return 0;
			s = -1;
			p++;
			break;
		case '-':
			if(n == here)
				return 0;
			s = n;
			p++;
			break;
		default:
			sysfatal("%s malformed range spec", range);
			break;
		}
	}
	/* NOTREACHED */
}

static int
inline_width(Elem *ep)
{
	int n;
	
	n = 0;
	for(; ep; ep = ep->next)
		if(strcmp(ep->name, "t") == 0 && ep->child && ep->child->pcdata)
			n += strlen(ep->child->pcdata);
	return n;
			
}

static int
cellwidth(Elem *ep, int type, int style)
{
	int width, first, id;
	char *fmt, buf[1024];

	width = 0;
	first = 1;
	for(; ep; ep = ep->next){
		if(! first)
			width++;
		first = 0;

		if(strcmp(ep->name, "is") == 0 && type == Inline && ep->child)
			width += inline_width(ep->child);

		if(strcmp(ep->name, "v") == 0 && ep->pcdata)
			switch(type){
			case Shared:
				width += strlen(lookstring(atoi(ep->pcdata)));
				break;
			case Numeric:
			case Date:
				id = style2numid(style);
				if(fmtnum(buf, sizeof(buf), id, ep->pcdata, type) == -1){
					fmt = numid2fmtstr(id);
					fprint(2, "%s: %d '%s' numfmt unknown\n", argv0, id, fmt);
					strcpy(buf, "unknon format");
				}
				width += strlen(buf);
				break;
			case String:
				width += strlen(ep->pcdata);
				break;
			case Bool:
				if(atoi(ep->pcdata) == 0)
					width += strlen("false");
				else
					width += strlen("true");
				break;
			case Error:
				width += strlen(ep->pcdata);
				break;
			}
	}
	return width;
}


static int
colwidth(int col)
{
	if(col < 0 || col >= Ncols)
		return Defwidth;			/* default width */
	return Colwidth[col -1];		/* -1 as column indices start at 1 */
}

static int
rd_c(Biobuf *bp, Elem *ep, int type, int style, int col)
{
	int id, first, remain, strwid, colwid;
	char *fmt, buf[1024];

	colwid = colwidth(col) -1;			/* -1 to ensures there a space between columns */
	remain = colwid;

	strwid = colwid;
	if(Tbl && !Trunc)
		strwid = cellwidth(ep, type, style);

	if(Tbl && strwid > Widefield)
		Bprint(bp, "T{\n");


	first = 1;
	for(; ep; ep = ep->next){
		if(! first)
			Bprint(bp, " ");
		first = 0;

		if(strcmp(ep->name, "is") == 0 && type == Inline && ep->child)
			rd_inlinestr(bp, ep->child, &remain);

		if(strcmp(ep->name, "v") == 0 && ep->pcdata)
			switch(type){
			case Shared:
				prnt(bp, lookstring(atoi(ep->pcdata)), &remain);
				break;
			case Numeric:
			case Date:
				id = style2numid(style);
				if(fmtnum(buf, sizeof(buf), id, ep->pcdata, type) < 0){
					fmt = numid2fmtstr(id);
					fprint(2, "%s: %d '%s' numfmt unknown\n", argv0, id, fmt);
					strcpy(buf, "unknon format");
				}
				prnt(bp, buf, &remain);
				break;
			case String:
				prnt(bp, ep->pcdata, &remain);
				break;
			case Bool:
				if(atoi(ep->pcdata) == 0)
					prnt(bp, "FALSE", &remain);
				else
					prnt(bp, "TRUE", &remain);
				break;
			case Error:
				prnt(bp, ep->pcdata, &remain);
				break;
			default:
				fprint(2, "type=%s - known but unsupported cell type (%s)\n", Strtype[type], ep->pcdata);
			}
	}

	if(Tbl && strwid > Widefield)
		Bprint(bp, "\nT}");
	return remain +1;				/* +1 to ensures there a space between columns */
}


/* Only works for ASCII, but column labels are always A1 B2 etc. */
static int
addr2col(char *s)
{
	if(isalpha(s[1]))
		return (toupper(s[0]) - 'A') * 26 + (toupper(s[1]) - 'A');
	return toupper(s[0]) - 'A';
}

static void
rd_row(Biobuf *bp, Elem *ep)
{
	char *v;
	int col, c, style, type, first, remain, notblank;

	col = 1;
	remain = 0;
	first = 1;
	notblank = 0;
	for(; ep;  ep = ep->next)
		if(strcmp(ep->name, "c") == 0 && ep->child){

			/* padding between columns (if there are more) */
			if(! (first || skip(Colrange, col))){
				if(Delim)
					Bprint(bp, "%s", Delim);
				else
					Bprint(bp, "%*.s", remain, "");
			}
			remain = 0;
			first = 0;

			type = Numeric;		/* default if no type set */
			if((v = xmlvalue(ep, "t")) != nil){
				if(strcmp(v, "inlineStr") == 0)
					type = Inline;
				else if(strcmp(v, "str") == 0)
					type = String;
				else if(strcmp(v, "s") == 0)
					type = Shared;
				else if(strcmp(v, "b") == 0)
					type = Bool;
				else if(strcmp(v, "d") == 0)
					type = Date;
				else if(strcmp(v, "e") == 0)
					type = Error;
				else if(strcmp(v, "n") == 0)
					type = Numeric;
				else
					sysfatal("rd_row: type=%s unknown type\n", v);
			}

			style = 0;
			if((v = xmlvalue(ep, "s")) != nil)
				style = atoi(v);

			if((v = xmlvalue(ep, "r")) != nil)
				c = addr2col(v);
			else
				c = col;

			/* padding for missing colums */
			while(col++ < c){
				if(skip(Colrange, col))
					continue;
				if(Delim)
					Bprint(bp, "%s", Delim);
				else
					Bprint(bp, "%*.s", colwidth(col), "");
				notblank++;
			}

			if(skip(Colrange, col))
				continue;
			remain = rd_c(bp, ep->child, type, style, col);
			notblank++;
		}
	if(Blanklines || notblank)
		Bprint(bp, "\n");
}

static void
rd_sheetdata(Biobuf *bp, Elem *ep)
{
	char *v;
	int row, r;

	row = 1;
	for(; ep; ep = ep->next)
		if(strcmp(ep->name, "row") == 0 && ep->child){
			if((v = xmlvalue(ep, "r")) != nil){
				r = atoi(v);
				if(Blanklines)
					for(; row < r; row++)
						Bprint(bp, "\n");
			}
			rd_row(bp, ep->child);
		}
}

static void
rd_cols(Elem *base)
{
	char *v;
	Elem *ep;
	int min, max, i;

	Ncols = 0;
	for(ep = base; ep; ep = ep->next)
			if((v = xmlvalue(ep, "max")) != nil){
				i = atoi(v);
				if(i > Ncols)
					Ncols = i;
			}
	
	Colwidth = mallocz(Ncols * sizeof(int), 0);
	if(Colwidth == nil)
		sysfatal("No memory for column widths\n");

	min = max = -1;
	for(ep = base; ep; ep = ep->next){
		if(strcmp(ep->name, "col") == 0){
			if((v = xmlvalue(ep, "min")) != nil)
				min = atoi(v);

			if((v = xmlvalue(ep, "max")) != nil)
				max = atoi(v);

			if((v = xmlvalue(ep, "width")) != nil){
				if(min == -1 || max == -1)
					sysfatal("badly formatted column widths\n");
				Colwidth[min -1] = ceil(atof(v));			/* -1 as cols start at 1 */

				for(i = min+1; i <= max; i++)
					Colwidth[i-1] = Colwidth[min-1];		/* -1 as cols start at 1 */
				min = max = -1;
			}
		}
	}
}

static Xml *
parsefile(char *fmt, ...)
{
	char *s;
	Xml *xp;
	int fd;
	va_list ap;

	va_start(ap, fmt);
	s = vsmprint(fmt, ap);
	va_end(ap);

	if((fd = open(s, OREAD)) == -1)
		return nil;
	if((xp = xmlparse(fd, 8192, Fcrushwhite)) == nil)
		return nil;
	close(fd);
	free(s);
	return xp;
}

static void
usage(void)
{
	fprint(2, "usage: %s [-b] [-c range] [-d str]  [-q] [-s n] [-t] [-T] ziproot\n", argv0);
	fprint(2, "  -b         allow blank rows in output\n");
	fprint(2, "  -c range   output only columns in range\n");
	fprint(2, "     ranges contain a comma seperated list fo fields, or\n");
	fprint(2, "     first and last field numbers seperated by a minus\n");
	fprint(2, "  -d str   set field delimiter, disables field padding\n");
	fprint(2, "  -q         quote cell text\n");
	fprint(2, "  -s n       select sheet number to print\n");
	fprint(2, "  -t         truncate long cells to column width\n");
	fprint(2, "  -T         generate tbl(1) input\n");
	fprint(2, "  -C x       set currency symbol to x\n");
	exits("usage");
}

void
main(int argc, char *argv[])
{
	int i, sheet, dmpstr, dmpsty;
	Elem *ep;
	Biobuf bout;
	char *s, *v;
	Xml *xp;

	dmpsty = 0;
	dmpstr = 0;
	sheet = 1;
	ARGBEGIN{
	case 'b':
		Blanklines = 1;
		break;
	case 'c':
		Colrange = EARGF(usage());
		break;
	case 'D':
		s = EARGF(usage());
		if(strcmp(s, "xml") == 0)
			xmldebug++;
		if(strcmp(s, "str") == 0)
			dmpstr++;
		if(strcmp(s, "sty") == 0)
			dmpsty++;
		break;
	case 'd':
		Delim = EARGF(usage());
		break;
	case 's':
		sheet = atoi(EARGF(usage()));
		break;
	case 'q':
		Doquote = 0;
		break;
	case 't':
		Trunc = 1;
		break;
	case 'T':
		Tbl = 1;
		Delim = "\t";
		break;
	case 'C':
		Currency = EARGF(usage());
		break;
	default:
		usage();
	}ARGEND;

	if(argc == 0)
		usage();

	quotefmtinstall();

	Binit(&bout, 1, OWRITE);
	if((xp = parsefile("%s/xl/sharedstrings.xml", argv[0])) != nil){
		if((ep = xmllook(xp->root, "/sst/si", nil, nil)) != nil)
			rd_strings(ep);
		xmlfree(xp);
		if(dmpstr)
			dumpstrings();
	}

	if((xp = parsefile("%s/xl/styles.xml", argv[0])) != nil){
		if((ep = xmllook(xp->root, "/styleSheet", nil, nil)) != nil && ep->child != nil)
			rd_styles(ep->child);
		xmlfree(xp);
		if(dmpsty)
			dumpstyles();
	}
	if((xp = parsefile("%s/xl/workbook.xml", argv[0])) != nil){
		if((ep = xmllook(xp->root, "/workbook/workbookPr", nil, nil)) != nil)
			if((v = xmlvalue(ep, "date1904")) != nil)
				Epoch1904 = atoi(v);
		xmlfree(xp);
	}

	if((xp = parsefile("%s/xl/worksheets/sheet%d.xml", argv[0], sheet)) == nil)
		sysfatal("sheet %d - cannot read %r", sheet);

	if((ep = xmllook(xp->root, "/worksheet/cols", nil, nil)) != nil && ep->child != nil)
		rd_cols(ep->child);

	Defwidth = 10;
	if((ep = xmllook(xp->root, "/worksheet/sheetFormatPr", nil, nil)) != nil)
		if((v = xmlvalue(ep, "defaultColWidth")) != nil)
			Defwidth = atoi(v);

	if(Tbl){
		Bprint(&bout, ".LP\n");			/* bootstrap MS macros */
		Bprint(&bout, "\\s(08\\fH\n");
		Bprint(&bout, ".ps 8\n");
		Bprint(&bout, ".TS H\n");
		Bprint(&bout, "allbox  center ;\n");
		for(i = 1; i <= Ncols; i++)
			if(! skip(Colrange, i))
				Bprint(&bout, "l ");
		Bprint(&bout, ".\n");
	}

	if((ep = xmllook(xp->root, "/worksheet/sheetData", nil, nil)) == nil || ep->child == nil)
		sysfatal("/worksheet/sheetData not found in worksheet");
	rd_sheetdata(&bout, ep->child);

	if(Tbl)
		Bprint(&bout, ".TE\n");

	Bterm(&bout);
	xmlfree(xp);
	exits(nil);
}

