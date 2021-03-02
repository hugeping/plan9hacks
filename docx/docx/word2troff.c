/* OpenXML (docx) to text conversion.
 * Steve Simon,   Oct 2012
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include <xml.h>
#include <ctype.h>

/*
 * Try to do continuious underline/strikethrou.
 *
 * This looks much better than just underlining the
 * text but because troff adds padding between words
 * to meet its justification targets, and these have
 * no underscore/strikethru we still get some gaps which
 * looks worse to my eyes. Try it and see what you think.
 */
enum { UnderlineSpaces = 0 };


enum {
	Plain,			/* Run.style */
	Bold,
	Italic,
	Underline,
	Strikeout,
	Smallcaps,
	Allcaps,

	Fullsize = 0,	/* Run.shift */
	Superscript,
	Subscript,

	Lefthand = 0,	/* Para.style */
	List,
	Heading,
	Boxed,
	Quoted,
	Caption,
	Title,
	Code,
	Emphasis,

	Left = 0,		/* Para.just */
	Fill,
	Right,
	Center,

	Dotted = 1,		/* Tab.style */

	Ntabs = 32,		/* max number of tab stops we support */
	Wrap = 90,		/* wordwrap at this column */
	Widefield = 32,	/* table fields wider than this get wrapped */
};

typedef struct Fonts Fonts;
typedef struct Para Para;
typedef struct Run Run;
typedef struct Tab Tab;

struct Fonts {
	char *xml;
	char *norm;
	char *bold;
	char *italic;
};

struct Run {
	double size;		/* point size or zero if default */
	int style;			/* bold italic etc */
	int shift;			/* sub/super-script */
	int font;		/* index into fontmap table */
	int parastyle;		/* inherited paragraph style */
};

struct Tab {
	double pos;
	int style;
	int just;
};

struct Para {
	int just;			/* justification */
	int style;			/* Centered, title, lefthand etc */
	int level;			/* bullet/numbering level */
	double left;		/* left margin or zero if default */
	double right;		/* right margin or zero if default */
	int tabseq;			/* sequence number of tab definitions */
	int ntab;			/* number of tabs defined */
	Tab tabs[Ntabs]; 	/* tab specs */
};

static struct {			/* map Word's paragraph styles to troff's */
	char *name;
	int style;
} Stylemap[] = {
	{ "ListParagraph",	Lefthand },
	{ "code",			Code },
	{ "Title",			Title },
	{ "Quote",			Quoted },
	{ "Caption",		Caption },
	{ "Heading1",		Heading },
	{ "Heading2",		Heading },
	{ "Heading3",		Heading },
	{ "Heading4",		Heading },
	{ "Heading5",		Heading },
	{ "Heading6",		Heading },
	{ "Heading7",		Heading },
	{ "Heading8",		Heading },
	{ "Emphasis",		Emphasis },
};

enum {
	Deffont,
	Codefont
};

Fonts Fontmap[] = {					/* map Word's font names ot troff's */
	[Deffont] 	{ "Helvetica",			"H",	"HB",	"HI" },
	[Codefont]	{ "Courier New",		"CW",	"CW",	"CW" },
				{ "Arial",				"H",	"HB",	"HI" },
				{ "Times New Roman",	"R",	"B",	"I"  },
};


static int Col = 1;						/* column for wordwrap */
static double Pagewidth = 6.5;			/* Word's page width */
static double Defsize = 20;				/* Word's initial font size in ½ points */
static double Codesize = 14;			/* code font size in ½ points */
static double Defleft = 0.625;			/* Word's left gutter */
static double Defright = 0.625;			/* Word's right gutter */
static double Scalepoints = 0.5;		/* Word's units to points */
static double Scaleinches = (1.0 / 1440); /* scale Word's units to inches */

/*
 * scale Word's units to inches and then adjust for the different
 * page layout engines. This was done by eye and seems to work.
 */
static double Scalewidth = (1.0 / 1440) * 0.874;

static Para Curpara;					/* current para settings */
static Run Currun;						/* current run settings */

static void para(Biobuf *bp, Elem *ep, Para *pp, int intable);
static void setpara(Biobuf *bp, Para *pp, int newpara, int intable);

static Elem *Footnotes;

static void
markpos(Biobuf *bp, Run *rp)
{
	if(rp == nil)
		return;
	if(rp->style != Strikeout && rp->style != Underline)
		return;
	Col += Bprint(bp, "\\kx");
}

/*
 * This ugly underline and strikethru troff code lifted 
 * from Paul DuBois's rtf2troff; he disliked it too.
 */
static void
ostrike(Biobuf *bp, Run *rp)
{
	if(rp == nil)
		return;
	if(rp->style == Underline)
		Col += Bprint(bp, "\\l'|\\nxu\\(ru'");
	if(rp->style == Strikeout)
		Col += Bprint(bp, "\\l'|\\nxu\\(em'");
}

static int
cookrune(Biobuf *bp, Rune r)
{
	switch(r){
	case 0x2019:		// right single quotation mark
		return Bprint(bp, "'");
	case 0x2018:		// left single quotation mark
		return Bprint(bp, "`");
	case 0xa0:			// non-breaking space
		return Bprint(bp, "\\ ");
	case '\\':			// backslash
		return Bprint(bp, "\\(bs");
	default:
		break;
	}
	return Bputrune(bp, r);
}

/*
 * Because the way we implement underlines and strikethru
 * uses horizontal motion we have to only draw under/thru
 * objects which will never be broken i.e. means words,
 * (but remember to turn off hypenation)
 */
static void
text(Biobuf *bp, Run *rp, char *str)
{
	Rune r;
	char *p, *s;

	markpos(bp, rp);

	p = str;
	while(*p){
		p += chartorune(&r, p);

		if(Col == 1)
			markpos(bp, rp);

		/* escape troff whitespace and control chars if they appear at start of line */
		if(Col == 1 && (r == L'.' || r == L'\'' || isspacerune(r)))
			Bprint(bp, "\\&");

		if(isspacerune(r)){
			ostrike(bp, rp);

			/* word wrap to make the troff code easier to edit */
			if((s = strchr(p+1, ' ')) != nil)
				if(Col + s-(p+1) > Wrap){
					Bprint(bp, "\n");
					Col = 1;
					continue;
				}
		}

		if(UnderlineSpaces)
			if(isspacerune(r))
				markpos(bp, rp);

		if(rp && (rp->style == Allcaps || rp->style == Smallcaps))
			r = toupperrune(r);

		Col += cookrune(bp, r);

		if(! UnderlineSpaces)
			if(isspacerune(r))
				markpos(bp, rp);

		if(r == L'\n')
			Col = 1;
	}

	ostrike(bp, rp);
}

static void
ctrl(Biobuf *bp, char *fmt, ...)
{
	Rune r;
	char *p;
	va_list ap;
	char buf[1024];

	va_start(ap, fmt);
	vsnprint(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	p = buf;

	/* strip leading newline if we are already in column 1 */
	if(*p == '\n' && Col == 1)
		p++;

	while(*p){
		p += chartorune(&r, p);
		Bputrune(bp, r);
		Col++;
		if(r == L'\n')
			Col = 1;
	}
}

static void
object(Biobuf *bp, Elem *ep)
{
	char *v, *p;

	for(; ep; ep = ep->next)
		if(strcmp(ep->name, "o:OLEObject") == 0)
			if((v = xmlvalue(ep, "ProgID")) != nil){
				/* prettyness */
				if((p = strrchr(v, '.')) != nil && atoi(p+1) != 0)
					*p = 0;
				ctrl(bp, "%s", v);
			}

}

static void
setfont(Biobuf *bp, char *name)
{
	switch(strlen(name)){
	case 2:
		ctrl(bp, "\\f(%s", name);
		break;
	case 1:
		ctrl(bp, "\\f%s", name);
		break;
	default:
		sysfatal("'%s' bad font name length (not 1 or 2)\n", name);
	}
}

static void
setrun(Biobuf *bp, Run *rp, int intable)
{
	int ospace, nspace;

	if(intable){
		if(rp->parastyle == Currun.parastyle)
			return;
		rp->style = Plain;
	}

	if(rp->parastyle != Currun.parastyle){
		if(rp->parastyle == Code){
			rp->size = Codesize;
			rp->font = Codefont;

		}
		else{
			rp->size = Defsize;
			rp->font = Deffont;
		}

		if(rp->parastyle == Emphasis)
			rp->style = Italic;

		Currun.parastyle = rp->parastyle;
	}

	
	if(rp->size != Currun.size){

		/*
		 * This is pure guesswork, though the results seem to look about right.
		 * the line spacing needs to be a bout a 10th of the pointsize
		 */
		ospace = (int)(Scalepoints * Currun.size / 10);
		if(ospace < 1)
			ospace = 1;
		nspace = (int)(Scalepoints * rp->size / 10);
		if(nspace < 1)
			nspace = 1;

		if(nspace != ospace)
			ctrl(bp, "\n.ls %d\n", nspace);
		ctrl(bp, "\\s(%02d", (int)(Scalepoints * rp->size));

		Currun.size = rp->size;
	}

	if(rp->style != Currun.style){
		switch(Currun.style){
		case Smallcaps:
			ctrl(bp, "\\s+2");
			break;
		case Underline:
		case Strikeout:
			ctrl(bp, "\n.hy 14\n");
			break;
		}

		switch(rp->style){
		case Bold:
			setfont(bp, Fontmap[rp->font].bold);
			break;
		case Italic:
			setfont(bp, Fontmap[rp->font].italic);
			break;
		case Smallcaps:
			ctrl(bp, "\\s-2");
			setfont(bp, Fontmap[rp->font].norm);
			break;
		case Underline:
		case Strikeout:
			ctrl(bp, "\n.hy 0\n");
			break;
		default:
			setfont(bp, Fontmap[rp->font].norm);
			break;
		}
		Currun.style = rp->style;
	}

	if(rp->shift != Currun.shift){
		switch(Currun.shift){
		case Superscript:
			ctrl(bp, "\\s+3\\v'+%.2fp'", (Scalepoints * rp->size) * 0.3);
			break;
		case Subscript:
			ctrl(bp, "\\s+3\\v'-%.2fp'", (Scalepoints * rp->size) * 0.2);
			break;
		}

		switch(rp->shift){
		case Superscript:
			ctrl(bp, "\\v'-%.2fp'\\s-3", (Scalepoints * rp->size) * 0.3);
			break;
		case Subscript:
			ctrl(bp, "\\v'+%.2fp'\\s-3", (Scalepoints * rp->size) * 0.2);
			break;
		}
		Currun.shift = rp->shift;
	}

}

static void
getstyle(Elem *ep, Run *rp, int flag)
{
	char *v;

	if((v = xmlvalue(ep, "w:val")) != nil)
		if(atoi(v) == 0)
			return;
	rp->style = flag;
}

static void
runprops(Biobuf *bp, Elem *ep, Run *rp)
{
	int i;
	char *v;

	USED(bp);
	for(; ep; ep = ep->next){
		if(strcmp(ep->name, "w:b") == 0)
			getstyle(ep, rp, Bold);
		if(strcmp(ep->name, "w:i") == 0)
			getstyle(ep, rp, Italic);
		if(strcmp(ep->name, "w:u") == 0)
			getstyle(ep, rp, Underline);
		if(strcmp(ep->name, "w:strike") == 0)
			getstyle(ep, rp, Strikeout);
		if(strcmp(ep->name, "w:caps") == 0)
			getstyle(ep, rp, Allcaps);
		if(strcmp(ep->name, "w:smallCaps") == 0)
			getstyle(ep, rp, Smallcaps);
		if(strcmp(ep->name, "w:sz") == 0)
			if((v = xmlvalue(ep, "w:val")) != nil)
				rp->size = atoi(v);
		if(strcmp(ep->name, "w:vertAlign") == 0)
			if((v = xmlvalue(ep, "w:val")) != nil){
				if(strcmp(v, "subscript") == 0)
					rp->shift = Subscript;	
				if(strcmp(v, "superscript") == 0)
					rp->shift = Superscript;
			}
		if(strcmp(ep->name, "w:rFonts") == 0)
			if((v = xmlvalue(ep, "w:ascii")) != nil)
				for(i = 0; i < nelem(Fontmap); i++)
					if(strcmp(v, Fontmap[i].xml) == 0){
						rp->font = i;
						break;
					}
		}

}

static void
footbody(Biobuf *bp, Elem *ep)
{
	Para p;

	memset(&p, 0, sizeof(Para));
	for(; ep; ep = ep->next)
		if(strcmp(ep->name, "w:p") == 0 && ep->child)
			para(bp, ep->child, &p, 1);
}

static void
footnote(Biobuf *bp, char *id)
{
	Para p;
	Elem *ep;
	char *v;

	ctrl(bp, "\\s-3\\v'-2.5p'%s\\v'+2.5p'\\s+3", id);		// TODO: should move a fraction of the font size

	memset(&p, 0, sizeof(Para));
	for(ep = Footnotes; ep; ep = ep->next){
		if(strcmp(ep->name, "w:footnote") == 0 && ep->child)
			if((v = xmlvalue(ep, "w:id")) != nil)
				if(strcmp(v, id) == 0){
					ctrl(bp, "\n.FS\n");
					ctrl(bp, "%s", id);
					footbody(bp, ep->child);
					ctrl(bp, "\n.FE\n");
					break;
				}
	}
	if(ep == nil)
		ctrl(bp, "\n[footnote not found]\n");
}

static int
run(Biobuf *bp, Para *pp, Elem *ep, int isleader, int intable)
{
	Run r;
	char *v, *brk;
	int tabstop;

	memset(&r, 0, sizeof(r));
	r.size = Defsize;
	r.font = Deffont;
	r.parastyle = pp->style;

	tabstop = 0;
	for(; ep; ep = ep->next){
		if(strcmp(ep->name, "w:footnoteReference") == 0)
			if((v = xmlvalue(ep, "w:id")) != nil)
				footnote(bp, v);

	if(strcmp(ep->name, "w:drawing") == 0)
			ctrl(bp, "\n.ce 1\n[drawing]\n");

		if(strcmp(ep->name, "w:pict") == 0)
			ctrl(bp, "\n.ce 1\n[picture]\n");

		if(strcmp(ep->name, "w:object") == 0 && ep->child){
			ctrl(bp, "\n.ce 1\n[object: ");
			object(bp, ep->child);
			ctrl(bp, "]\n");
		}

		if(strcmp(ep->name, "w:cr") == 0)
			ctrl(bp, "\n.br\n");

		if(strcmp(ep->name, "w:br") == 0){
			brk = ".br";
			if((v = xmlvalue(ep, "w:type")) != nil)
				if(strcmp(v, "page") == 0)
					brk = ".bp";
			ctrl(bp, "\n%s\n", brk);
		}

		if(strcmp(ep->name, "w:rPr") == 0 && ep->child)
			runprops(bp, ep->child, &r);

		if(strcmp(ep->name, "w:t") == 0 && ep->pcdata){
			setrun(bp, &r, intable);
			text(bp, &r, ep->pcdata);
		}

		if(strcmp(ep->name, "w:tab") == 0){
			if(isleader)
				Bprint(bp, "%c", 1);
			else
				Bprint(bp, "\t");
			tabstop++;
		}

	}
	return tabstop;
}

static void
cell(Biobuf *bp, Elem *ep, Para *pp)
{
	Para p;
	int first;

	p = *pp;

	first = 1;
	for(; ep; ep = ep->next)
		if(strcmp(ep->name, "w:p") == 0 && ep->child){
			if(! first)
				text(bp, nil, " ");
			first = 0;
			para(bp, ep->child, &p, 1);
		}		
}

static void
row(Biobuf *bp, Elem *ep, Para *pp)
{
	int first;

	first = 1;
	for(; ep; ep = ep->next)
		if(strcmp(ep->name, "w:tc") == 0){
			if(! first)
				ctrl(bp, "\t");
			first = 0;

			ctrl(bp, "T{\n");
			cell(bp, ep->child, pp);
			ctrl(bp, "\nT}");
		}
	ctrl(bp, "\n");
}

static void
grid(Biobuf *bp, Elem *base)
{
	Elem *ep;
	char *v;
	double width;

	for(ep = base; ep; ep = ep->next)
		if(strcmp(ep->name, "w:gridCol") == 0){
			if((v = xmlvalue(ep, "w:w")) != nil){
				width = Scalewidth * atof(v);
				if(width < 0.01)				/* spurious fields (why are they here?) */
					continue;
				ctrl(bp, "lw(%1.2fi) ", width);
			}
			else
				ctrl(bp, "l ");
		}
	ctrl(bp, ".\n");

}

static void
borders(Elem *ep, int *nbox, int *allbox)
{
	char *v;

	for(; ep; ep = ep->next){
		if(strcmp(ep->name, "w:top") == 0){
			if((v = xmlvalue(ep, "w:val")) != nil)
				if(strcmp(v, "single") == 0)
					*nbox = 1;
				if(strcmp(v, "double") == 0)
					*nbox = 2;
		}

		if(strcmp(ep->name, "w:insideH") == 0){
			if((v = xmlvalue(ep, "w:val")) != nil)
				if(strcmp(v, "nil") != 0)
					*allbox = 1;
		}
	}
}

static void
tableprops(Biobuf *bp, Elem *base)
{
	char *v;
	Elem *ep;
	int expand, center, allbox, nbox;

	nbox = 0;
	expand = 0;
	center = 0;
	allbox = 0;

	for(ep = base; ep; ep = ep->next){
		if(strcmp(ep->name, "w:tblW") == 0)
			if((v = xmlvalue(ep, "w:type")) != nil)
				if(strcmp(v, "auto") == 0)
					expand = 1;

		if(strcmp(ep->name, "w:jc") == 0)
			if((v = xmlvalue(ep, "w:val")) != nil)
				if(strcmp(v, "center") == 0)
					center = 1;

  		if(strcmp(ep->name, "w:tblStyle") == 0)
			if((v = xmlvalue(ep, "w:val")) != nil)
				if(strcmp(v, "TableGrid") == 0){
					allbox = 1;
				}

		if(strcmp(ep->name, "w:tblBorders") == 0 && ep->child)
			borders(ep->child, &nbox, &allbox);
	}

	ctrl(bp, "\n");
	if(nbox == 1)
		ctrl(bp, "box ");
	if(nbox == 2)
		ctrl(bp, "doublebox ");
	if(center)
		ctrl(bp, "center ");
	else					/* expand and center are mulually exclusive */
	if(expand)
		ctrl(bp, "expand ");
	if(allbox)
		ctrl(bp, "allbox ");
	ctrl(bp, ";\n");
}


static void
table(Biobuf *bp, Elem *base)
{
	int first;
	Elem *ep;
	Run r;
	Para p;

	/*
	 * ensure we have reset to default 
	 * environment before we enter the table
	 */
	memset(&p, 0, sizeof(p));
	setpara(bp, &p, 0, 0);
	memset(&r, 0, sizeof(r));
	r.font = Deffont;
	r.size = Defsize;
	setrun(bp, &r, 0);

	first = 1;
	ctrl(bp, "\n.TS H\n");
	for(ep = base; ep; ep = ep->next){
		if(strcmp(ep->name, "w:tr") == 0 && ep->child){
			row(bp, ep->child, &p);

			/*
			 * we _guess_ that the table heading is only one row tall
			 * (for multi page tables)
			 */ 
			if(first)
				ctrl(bp, ".TH\n");
			first = 0;
		}

		if(strcmp(ep->name, "w:tblPr") == 0 && ep->child)
			tableprops(bp, ep->child);

		if(strcmp(ep->name, "w:tblGrid") == 0 && ep->child)
			grid(bp, ep->child);

	}
	ctrl(bp, "\n.TE\n");

}


static void
tabs(Elem *ep, Para *pp)
{
	char *v;
	Tab *tp;

	for(; ep; ep = ep->next){
		if(strcmp(ep->name, "w:tab") == 0){
			tp = &pp->tabs[pp->ntab++];

			if((v = xmlvalue(ep, "w:val")) != nil)
				if(strcmp(v, "right") == 0)
					tp->just = Right;
				if(strcmp(v, "left") == 0)
					tp->just = Left;
				if(strcmp(v, "center") == 0)
					tp->just = Center;

			if((v = xmlvalue(ep, "w:leader")) != nil)
				if(strcmp(v, "dot") == 0)
					tp->style = Dotted;

			if((v = xmlvalue(ep, "w:pos")) != nil)
				tp->pos = atoi(v);
		}
	}

}

static int
tabcmp(int na, Tab *a, int nb, Tab *b)
{
	int i;

	if(na != nb)
		return -1;
	for(i = 0; i < na; i++){
		if(a[i].style != b[i].style)
			return -1;
		if(a[i].just != b[i].just)
			return -1;
		if(a[i].pos != b[i].pos)
			return -1;
	}
	return 0;
}

static void
listprops(Elem *ep, Para *pp)
{
	char *v;

	for(; ep; ep = ep->next){
		if(cistrcmp(ep->name, "w:ilvl") == 0)
			if((v = xmlvalue(ep, "w:val")) != nil)
				pp->level = atoi(v);
	}
			
}

static void
paraprops(Elem *ep, Para *pp)
{
	int i;
	char *v;

	for(; ep; ep = ep->next){
		if(strcmp(ep->name, "w:tabs") == 0 && ep->child)
			tabs(ep->child, pp);

		if(strcmp(ep->name, "w:jc") == 0)	
			if((v = xmlvalue(ep, "w:val")) != nil){
				if(strcmp(v, "right") == 0)
					pp->just = Right;
				if(strcmp(v, "center") == 0)
					pp->just = Center;
				if(strcmp(v, "both") == 0)
					pp->just = Fill;
			}

		if(strcmp(ep->name, "w:ind") == 0){
			if((v = xmlvalue(ep, "w:left")) != nil)
				pp->left = atoi(v);
			if((v = xmlvalue(ep, "w:right")) != nil)
				pp->right = atoi(v);
		}

		if(strcmp(ep->name, "w:numPr") == 0 && ep->child){
			pp->style = List;
			listprops(ep->child, pp);
		}

		if(strcmp(ep->name, "w:pBdr") == 0){
			pp->style = Boxed;
		}

		if(strcmp(ep->name, "w:pStyle") == 0)
			if((v = xmlvalue(ep, "w:val")) != nil){
				for(i = 0; i < nelem(Stylemap); i++)
					if(strcmp(v, Stylemap[i].name) == 0){
						pp->style = Stylemap[i].style;
						break;
					}
			}
	}
}

static void
setpara(Biobuf *bp, Para *pp, int newpara, int intable)
{
	int i, ispara;
	double n;
	static char *bullets[] = {
		"\\(bu", "\\(ci", "\\(bx", "\\(sq",
		"\\(bu\\(bu", "\\(ci\\(ci", "\\(bx\\(bx", "\\(sq\\(sq"
	};

	/* right margin */
	if(pp->right != Curpara.right){
		n = Pagewidth - (Scalewidth * pp->right + Defright);
		if(n > Pagewidth)
			n = Pagewidth;
		ctrl(bp, "\n.nr LL %gi\n", n);
		ctrl(bp, "\n.ll \\n(LLu\n");
		Curpara.right = pp->right;
	}

	/* left margin */
	if(pp->left != Curpara.left){
		n = Scalewidth * pp->left + Defleft;
		if(n < 0)
			n = 0;
		ctrl(bp, "\n.in %gi\n", n);
		Curpara.left = pp->left; 
	}

	/*
	 * tab stops and justification are reset by paragraph breaks in
	 * MS macros so we have this flag to remind us to reload them at
	 * the start of the next paragraph
	 */
	ispara = 0;
	
	/* paragraph style */
	if(!intable && (newpara || pp->style != Curpara.style)){

	 	switch(Curpara.style){
		case Boxed:
			ctrl(bp, "\n.B2\n");
			break;
		case Quoted:
			ctrl(bp, "\n.QE\n");
			break;
		case Caption:
			ctrl(bp, "\n.ce 0\n");
			break;
		case Title:
			ctrl(bp, "\n.B2\n.in +0.3i\n");
		case Code:
			ctrl(bp, "\n.fi\n.ju\n");
			break;
		}

		switch(pp->style){
		case Lefthand:
			ctrl(bp, "\n.LP\n");
			ispara = 1;
			break;
		case List:
			if(pp->level < nelem(bullets))
				ctrl(bp, "\n.IP %s %.2fi\n", bullets[pp->level],
					pp->level * 0.15);
			else
				ctrl(bp, "\n.IP %d %.2fi\n", pp->level,
					pp->level * 0.15);
			ispara = 1;
			break;
		case Caption:
			ctrl(bp, "\n.SH\n.ce 9999\n");
			ispara = 1;
			break;
		case Title:
			ctrl(bp, "\n.SH\n.in -0.3i\n.B1\n");
			ispara = 1;
			break;
		case Heading:
			ctrl(bp, "\n.SH\n");
			ispara = 1;
			break;
		case Boxed:
			ctrl(bp, "\n.B1\n");
			ispara = 1;
			break;
		case Quoted:
			ctrl(bp, "\n.QP\n");
			ispara = 1;
			break;
		case Code:
			ctrl(bp, "\n.nj\n.nf\n");
			break;
		}

		Curpara.style = pp->style;
	}

	/* justification */
	if(! intable && (ispara || pp->just != Curpara.just)){
		switch(Curpara.just){
		case Fill:
		case Right:
			ctrl(bp, "\n.ad l\n");
			break;
		case Center:
			ctrl(bp, "\n.ce 0\n");
			break;
		}

		switch(pp->just){
		case Fill:
			ctrl(bp, "\n.ad b\n");
			break;
		case Center:
			ctrl(bp, "\n.ce 1000\n");
			break;
		case Right:
			ctrl(bp, "\n.ad r\n");
			break;
		}
		Curpara.just = pp->just;
	}

	/* tab stops */
	if(ispara || tabcmp(pp->ntab, pp->tabs, Curpara.ntab, Curpara.tabs) != 0){
		ctrl(bp, "\n.ta");
		for(i = 0; i < pp->ntab; i++)
			switch(pp->tabs[i].just){
			case Left:		/* NB: left is default */
				ctrl(bp, " %.2fiL", Scalewidth * pp->tabs[i].pos);
				break;
			case Center:
				ctrl(bp, " %.2fiC", Scalewidth * pp->tabs[i].pos);
				break;
			case Right:
				ctrl(bp, " %.2fiR", Scalewidth * pp->tabs[i].pos);
				break;
			}

		ctrl(bp, "\n");

		Curpara.ntab = pp->ntab;
		for(i = 0; i < pp->ntab; i++)
			Curpara.tabs[i] = pp->tabs[i];
	}
}


static void
para(Biobuf *bp, Elem *ep, Para *pp, int intable)
{
	Para p;
	int first, tabstop, isleader;

	p = *pp;
	first = 1;			/* force a paragraph break */
	tabstop = 0;		/* start with empty list of tabstops */

	for(; ep; ep = ep->next){
		if(strcmp(ep->name, "w:pPr") == 0 && ep->child)
			paraprops(ep->child, &p);

		if(strcmp(ep->name, "w:r") == 0 && ep->child){
			setpara(bp, &p, first, intable);
			first = 0;

			if(tabstop < p.ntab && p.tabs[tabstop].style == Dotted)
				isleader = 1;
			else
				isleader = 0;
			tabstop += run(bp, &p, ep->child, isleader, intable);
		}

		if(strcmp(ep->name, "w:hyperlink") == 0 && ep->child)
			para(bp, ep->child, &p, intable);

		/* autogenerated fields, e.g. auto numbered Figures */
		if(strcmp(ep->name, "w:fldSimple") == 0 && ep->child)
			para(bp, ep->child, &p, intable);
	}
}

static void
body(Biobuf *bp, Elem *ep)
{
	Para p;

	memset(&p, 0, sizeof(Para));
	for(; ep; ep = ep->next){
		if(strcmp(ep->name, "w:p") == 0 && ep->child)
			para(bp, ep->child, &p, 0);
		if(strcmp(ep->name, "w:tbl") == 0 && ep->child)
			table(bp, ep->child);
	}
}

static Xml *
parsefile(char *mnt, char *path)
{
	char *s;
	Xml *xp;
	int fd;

	s = smprint("%s/%s", mnt, path);
	if((fd = open(s, OREAD)) == -1)
		return nil;
	if((xp = xmlparse(fd, 8192, 0)) == nil)
		return nil;
	close(fd);
	free(s);
	return xp;
}


static void
usage(void)
{
	fprint(2, "usage: %s ziproot\n", argv0);
	exits("usage");
}

void
main(int argc, char *argv[])
{
	Elem *ep;
	Biobuf bout;
	Xml *xpDoc, *xpNote;

	ARGBEGIN{
	case 'd':
		xmldebug++;
		break;
	default:
		usage();
	}ARGEND;

	if(argc == 0)
		usage();

	Binit(&bout, 1, OWRITE);
	if((xpNote = parsefile(argv[0], "word/footnotes.xml")) != nil){
		if((ep = xmllook(xpNote->root, "/w:footnotes/w:footnote", nil, nil)) == nil || ep->child == nil)
			sysfatal("bad footnotes file format");
		Footnotes = ep;
		xmlfree(xpNote);
	}

	if((xpDoc = parsefile(argv[0], "word/document.xml")) == nil)
		sysfatal("cannot read document.xml %r");

	if((ep = xmllook(xpDoc->root, "/w:document/w:body", nil, nil)) == nil || ep->child == nil)
		sysfatal("bad document file format");

	ctrl(&bout, "\n.LP\n");		/* initialise MS macros */
	ctrl(&bout, "\n.FP helvetica\n");

	body(&bout, ep->child);

	Bprint(&bout, "\n");		/* just to be sure */
	Bterm(&bout);

	xmlfree(xpDoc);
	exits(nil);
}

