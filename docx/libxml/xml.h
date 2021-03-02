
#pragma lib "libxml.a"

typedef struct Xml Xml;
typedef struct Attr Attr;
typedef struct Elem Elem;

typedef struct Xtree Xtree;
typedef struct Xblock Xblock;

#pragma incomplete Xtree
#pragma incomplete Xblock

enum {
	Fcrushwhite = 1,
	Fstripnamespace = 2,
};

struct Xml {
	Elem	*root;			/* root of tree */
	char	*doctype;		/* DOCTYPE structured comment, or nil */
	struct {
		Xtree	*root;
		Xblock	*active;
		int	blksiz;
	} alloc;
};

struct Elem {
	Elem	*next;			/* next element at this hierarchy level */
	Elem	*child;		/* first child of this node */
	Elem	*parent;		/* parent of this node */
	Attr	*attrs;		/* linked list of atributes */
	char	*name;			/* element name */
	char	*pcdata;		/* pcdata following this element */
	int	line;			/* Line number (for errors) */
};

struct Attr {
	Attr	*next;			/* next atribute */
	Elem	*parent;		/* parent element */
	char	*name;			/* atributes name (nil for coments) */
	char	*value;		/* atributes value */
};

extern int xmldebug;

Attr*	xmlattr(Xml *, Attr **, Elem *, char *, char *);
Elem*	xmlelem(Xml *, Elem **, Elem *, char *);
Elem*	xmlfind(Xml *, Elem *, char *);
void	xmlfree(Xml *);
char*	xmlstrdup(Xml*, char *, int);
void*	xmlcalloc(Xml *, int, int);
void*	xmlmalloc(Xml *, int);
void	_Xheapstats(void);
void	_Xheapfree(Xml *);
Elem*	xmllook(Elem *, char *, char *, char *);
Xml*	xmlnew(int);
Xml*	xmlparse(int, int, int);
void	xmlprint(Xml *, int);
char*	xmlvalue(Elem *, char *);
