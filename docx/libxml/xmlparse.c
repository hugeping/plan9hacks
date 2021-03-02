#include <u.h>
#include <libc.h>
#include <ctype.h>
#include <bio.h>
#include "xml.h"
#include "state-machine.h"

int xmldebug = 0;

enum {
	Grain = 16
};

#define isname1(c)	(isalpha((c)) || c == '_')	/* FIXME: not enforced yet */
#define isnameN(r)	(isalpharune((r)) || isdigitrune((r)) || r == L'_' || r == L'-' || r == L'.' || r == L':')
#define Roundup(x, g)	(((x) + (unsigned)(g-1)) & ~((unsigned)(g-1)))

enum {
	Ntext = 1024,	/* longest name or atribute value possible */
	Nref = 32	/* longest entity reference name */
};


typedef struct {
	int line;	/* Line number (for errors) */
	Biobuf *bp;	/* input stream */
	int flags;	/* misc flags, see xml.h */
	Xml *xml;
	int failed;
} State;

typedef struct {
	char *buf;
	int sz;
} Lexbuf;


static int 
trimwhite(char *s)
{
	char *p;

	for(p = s; *p; p++)
		if(! isspace(*p))
			return 0;

	/* trim any whitespace into a single space */
	s[0] = ' ';
	s[1] = 0;
	return 1;
}

static void
growstr(State *st, Lexbuf *lb, char *str)
{
	int b, s, sz;

	if(str == nil || *str == 0)
		return;
	if((st->flags & Fcrushwhite) && trimwhite(str) &&
	   (lb->buf == nil || lb->buf[0] == 0))
		return;
	b = 0;
	if(lb->buf)
		b = strlen(lb->buf);
	s = strlen(str);

	sz = Roundup(b+s+1, Grain);
	if(sz >= lb->sz){
		lb->buf = realloc(lb->buf, sz);
		if(lb->buf == nil)
			sysfatal("No memory, wanted %d bytes\n", sz);
		lb->sz = sz;
	}
	strcpy(lb->buf+b, str);
}

static void
growrune(State *st, Lexbuf *lb, Rune r)
{
	int n;
	char str[UTFmax+1];

	n = runetochar(str, &r);
	str[n] = 0;
	growstr(st, lb, str);
}

static void
stripns(char *str)
{
	char *p;
	
	if((p = strrchr(str, ':')) == nil)
		return;
	strcpy(str, p+1);
}

static void
failed(State *st, char *fmt, ...)
{
	int n;
	va_list arg;
	char err[ERRMAX];

	st->failed = 1;
	va_start(arg, fmt);
	n = snprint(err, sizeof(err), "%d ", st->line);
	vsnprint(err+n, sizeof(err)-n, fmt, arg);
	va_end(arg);
	werrstr("%s", err);
	
}

static void
unget(State *st, int c)
{
	if(c == '\n')
		st->line--;
	Bungetrune(st->bp);
}

static long
get(State *st)
{
	long r;

	r = Bgetrune(st->bp);
	if(r == Runeerror){
		failed(st, "bad UTF-8 sequence");
		r = L' ';
	}

	if(r == L'\n')
		st->line++;

	if(xmldebug){
		if(xmldebug == 3)
			fprint(2, "%C", (Rune)r);
		if(xmldebug == 1 && r == -1)
			fprint(2, "EOF\n");
	}
	return r;
}

static struct {
	char *name;
	Rune rune;
} Entities[] = {
	{ "amp",	L'&' },
	{ "lt",		L'<' },
	{ "gt",		L'>' },
	{ "apos",	L'\'' },
	{ "quot",	L'"' },
	{ "nbsp",	0xa0 },		/* no-break space */
};

static long
entityref(State *st)
{
	int n, i, l;
	long r;
	Rune x;
	char *p, buf[Nref];

	l = 0;
	p = buf;
	while((r = get(st)) != -1 && r != L';' && ! isspacerune(r))
		if(l < (sizeof(buf) - UTFmax)){
			x = r;
			n = runetochar(p, &x);
			p += n;
			l += n;
		}
	*p = 0;
	if(r == -1)
		return -1;

	/* false positive */
	if(r != L';'){
		fprint(2, "%d: unquoted '&' - ignored\n", st->line);
		for(i = --l; i >= 0; i--)
			unget(st, buf[i]);
		return L'&';
	}

	if(buf[0] == '#'){
		if(buf[1] == 'x' || buf[1] == 'X')
			return strtol(buf+2, 0, 16);
		return strtol(buf+1, 0, 10);
	}

	for(i = 0; i < nelem(Entities); i++)
		if(memcmp(Entities[i].name, buf, l) == 0)
			return Entities[i].rune;

	fprint(2, "%d: '&%s;' unknown/unsupported entity reference\n", st->line, buf);
	return L'?';
}

static int
match(State *st, Rune *s)
{
	long r;
	Rune *p;

	r = -1;
	for(p = s; *p; p++)
		if((r = get(st)) != *p)
			break;
	if(r == -1)
		return -1;	/* EOF */
	if(*p == 0)
		return 0;	/* match */
	unget(st, r);
	for(p--; p >= s; p--)
		unget(st, *p);
	return 1;		/* no match */
}

static int
comment(State *st)
{
	long r;
	int startline;

	startline = st->line;
	do{
		if(get(st) == -1)
			break;
	}while(match(st, L"--") == 1);

	r = get(st);
	if(r == -1){
		failed(st, "EOF in comment (re: line %d)", startline);
		return -1;
	}
	if(r != L'>'){
		failed(st, "'--' illegal in a comment (re: line %d)", startline);
		return Twhite;
	}
	return Twhite;
}

static int
doctype(State *st, Lexbuf *lb)
{
	long r;
	char *p;
	int startline;

	startline = st->line;
	
	/* trim leading whitespace */
	while((r = get(st)) != -1 && isspacerune(r))
		continue;
	unget(st, r);

	if(lb->buf)
		lb->buf[0] = 0;

	while((r = get(st)) != -1 && r != L'>')
		growrune(st, lb, r);

	if(r == -1){
		failed(st, "EOF in DOCTYPE (re: line %d)", startline);
		return -1;
	}
	/* trim trailing whitespace */
	p = strrchr(lb->buf, 0);
	for(p--; p >= lb->buf && isspace(*p); p--)
		*p = 0;

	st->xml->doctype = xmlstrdup(st->xml, lb->buf, 0);
	return Twhite;
}

static int
cdata(State *st, Lexbuf *lb)
{
	long r;
	int startline;

	startline = st->line;
	do{
		if((r = get(st)) == -1)
			break;
		if(r == L'&')
			if((r = entityref(st)) == -1)
				break;
		growrune(st, lb, r);
	}while(match(st, L"]]>") == 1);

	if(r == -1){
		failed(st, "EOF in CDATA (re: line %d)", startline);
		return -1;
	}
	return Tname;
}

/*
 * byte order mark.
 * This is pointless for utf8 which has defined byte order,
 * and we don't support utf16 or utf32 but some xml seems to
 * prepend them to utf8 so we need to find them and skip them
 */
static int
bom(State *st)
{
	long r;

	if((r = get(st)) == -1)
		return -1;

	if(r != 0xbb){
		unget(st, 0xef);
		unget(st, r);
		return -1;
	}
	if((r = get(st)) == -1){
		unget(st, 0xef);
		unget(st, 0xbb);
		return -1;
	}
	if(r != 0xbf){
		unget(st, 0xef);
		unget(st, 0xbb);
		unget(st, r);
	}
	return 0;
}

static int
xlex(State *st, Lexbuf *lb, int s)
{
	long r;
	Rune q;

	while((r = get(st)) != -1){
		if(r == 0xef)
			if(bom(st) == 0)
				continue;
		if(r == L'<'){
			r = get(st);
			switch(r){
			case L'?':
				while((r = get(st)) != -1 && r != L'>')
					continue;
				if(r == -1)
					return -1;
				return Twhite;
			case L'!':
				if(match(st, L"--") == 0)
					return comment(st);
				if(match(st, L"DOCTYPE ") == 0)
					return doctype(st, lb);
				if(match(st, L"[CDATA[") == 0)
					return cdata(st, lb);
				failed(st, "<!name not known");
			case L'/':
				return Tendblk;
			case L' ':
			case L'\t':
			case L'\f':
			case L'\n':
			case L'\r':
				failed(st, "whitespace following '<'");
				break;
			default:
				unget(st, r);
				return Topen;
			}
			continue;
		}

		if(s != Slost){
			switch(r){
			case '=':
				return Tequal;
			case '>':
				return Tclose;
			case '/':
				r = get(st);
				if(r == '>')
					return Tnulblk;
				unget(st, r);
				continue;
			case '\'':
			case '"':		/* attribute value */
				q = r;
				while((r = get(st)) != -1 && r != q){
					if(r == L'&')
						if((r = entityref(st)) == -1)
							break;
					growrune(st, lb, r);
				}
				if(r == -1)
					return -1;
				return Tname;
			case '\n':
			case '\r':
			case ' ':
			case '\v':
			case '\f':
			case '\t':
				do
					growrune(st, lb, r);
				while((r = get(st)) != -1 && isspacerune(r));
				if(r == -1)
					return -1;
				unget(st, r);
				return Twhite;
			default:		/* attribute name */
				do
					growrune(st, lb, r);
				while((r = get(st)) != -1 && isnameN(r));
				if(r == -1)
					return -1;
				unget(st, r);
				return Tname;
			}
		}

		do{
			if(r == L'&')
				if((r = entityref(st)) == -1)
					break;
			growrune(st, lb, r);
		}while((r = get(st)) != -1 && r != '<');
		if(r == -1)
			return -1;
		unget(st, r);
		return Tname;
	}
	return -1;
}

static Elem *
_xmlparse(State *st, Elem *parent, int depth)
{
	Attr *ap;
	Lexbuf lexbuf, *lb;
	Lexbuf pcdata, *pc;
	Elem *root, *ep;
	int os, s, t, a;

	ap = nil;
	ep = nil;
	s = Slost;
	root = nil;
	lb = &lexbuf;
	memset(lb, 0, sizeof(Lexbuf));
	pc = &pcdata;
	memset(pc, 0, sizeof(Lexbuf));
	while((t = xlex(st, lb, s)) != -1){
		os = s;
		s = statab[os][t];
		a = acttab[os][t];
		if(xmldebug == 2)
			fprint(2, "depth=%d token=%s action=%s state=%s->%s str='%s'\n",
				depth, tokstr[t], actstr[a], stastr[os], stastr[s], lb->buf);
		switch(a){
		case Aelem:
			if(xmldebug == 1)
				fprint(2, "%-3d %*.selem name='%s'\n", st->line, depth, "", lb->buf);
			if(!isname1(lb->buf[0]))
				failed(st, "'%s' is an illegal element name", lb->buf);
			if(st->flags & Fstripnamespace)
				stripns(lb->buf);
			assert((ep = xmlelem(st->xml, &root, parent, lb->buf)) != nil);
			ep->line = st->line;
			break;
		case Apcdata:
			if(parent)
				growstr(st, pc, lb->buf);
			break;
		case Aattr:
			assert(ep != nil);
			if(xmldebug == 1)
				fprint(2, "%-3d %*.sattr name='%s'\n", st->line, depth, "", lb->buf);
			if(!isname1(lb->buf[0]))
				failed(st, "'%s' is an illegal attribute name", lb->buf);
			if(st->flags & Fstripnamespace)
				stripns(lb->buf);
			assert((ap = xmlattr(st->xml, &(ep->attrs), ep, lb->buf, nil)) != nil);
			break;
		case Avalue:
			assert(ep != nil);
			assert(ap != nil);
			ap->value = xmlstrdup(st->xml, lb->buf, 0);
			ap = nil;
			if(xmldebug == 1)
				fprint(2, "%*.sattr value=%s\n", depth, "", lb->buf);
			break;
		case Adown:
			assert(ep != nil);
			if(xmldebug == 1)
				fprint(2, "%*.sdown name=%s\n", depth, "", ep->name);
			ep->child = _xmlparse(st, ep, depth+1);
			if(xmldebug == 1 && ep->pcdata)
				fprint(2, "%*.s     name=%s pcdata len=%ld\n", 
					depth, "",
					ep->name,
					(ep->pcdata)? strlen(ep->pcdata): 0L);
			break;
		case Aup:
			if(pc->buf){
				parent->pcdata = xmlstrdup(st->xml, pc->buf, 0);
				free(pc->buf);
			}
			free(lb->buf);
			return root;
			/* NOTREACHED */
			break;
		case Acheck:
			assert(ep != nil);
			if(st->flags & Fstripnamespace)
				stripns(lb->buf);
			if(ep->name && strcmp(lb->buf, ep->name) != 0)
				failed(st, "</%s> found, expecting match for <%s> (re: line %d) - nesting error",
					lb->buf, ep->name, ep->line);
			break;
		case Anop:
			break;
		case Aerr:
			failed(st, "%s syntax error", lb->buf);
			break;
		default:
			sysfatal("xmlparse: %d - internal error, unknown action\n", a);
			break;
		}
		if(lb->buf)
			lb->buf[0] = 0;
	}
	if(t == -1 && depth != 0)
		failed(st, "unexpected EOF (depth=%d)", depth);

	if(pc->buf){
		parent->pcdata = xmlstrdup(st->xml, pc->buf, 0);
		free(pc->buf);
	}
	free(lb->buf);
	return root;
}

Xml *
xmlparse(int fd, int blksize, int flags)
{
	State s;
	Biobuf bio;
	Xml *x;

	memset(&s, 0, sizeof(s));
	s.line = 1;
	Binit(&bio, fd, OREAD);
	s.bp = &bio;
	s.flags = flags;

	x = xmlnew(blksize);
	s.xml = x;

	x->root = _xmlparse(&s, nil, 0);
	if(s.failed){
		if(x)
			xmlfree(x);
		x = nil;
	}
	Bterm(&bio);
	return x;
}
