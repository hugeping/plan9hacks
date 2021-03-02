extern int Epoch1904;		/* disable "as broken as Lotus-123" mode (yes really) */
extern char *Currency;		/* Currency symbol */

enum {
	Numeric,				/* type of field */
	Inline,
	Shared,
	Bool,
	String,
	Error,
	Date,					/* ISO 8601 dates,  >= Excel 2010 only */
};

/* fmtnum.c */
int fmtnum(char *buf, int len, int id, char *str, int type);

/* strings.c */
char *lookstring(int idx);
void stringindex(Elem *ep, int *idxp);
void rd_strings(Elem *ep);

/* strtab.c */
char *looktab(int idx);
void stringindex(Elem *ep, int *idxp);
void mktab(Elem *ep);
void dumpstrings(void);

/* styles.c */
int style2numid(int style);
char *numid2fmtstr(int id);
void dumpstyles(void);
void rd_styles(Elem *base);
