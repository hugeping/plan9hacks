#include <u.h>
#include <libc.h>
#include <bio.h>
#include <xml.h>
#include "xlsx.h"

typedef struct Numfmt Numfmt;
struct Numfmt {
	int style;
	int id;
	Numfmt *left;
	Numfmt *right;
};

typedef struct Fmtstr Fmtstr;
struct Fmtstr {
	int id;
	char *fmt;
	Fmtstr *left;
	Fmtstr *right;
};

static Numfmt *Numroot;
static Fmtstr *Fmtroot;

static char *
lookfmtstr(Fmtstr *fs, int id)
{
	if(fs == nil)
		return nil;
	if(fs->id < id)
		return lookfmtstr(fs->left, id);
	if(fs->id > id)
		return lookfmtstr(fs->right, id);
	return fs->fmt;
}

static int
looknumfmt(Numfmt *nf, int style)
{
	if(nf == nil)
		return -1;
	if(nf->style < style)
		return looknumfmt(nf->left, style);
	if(nf->style > style)
		return looknumfmt(nf->right, style);
	return nf->id;
}

int
style2numid(int style)
{
	return looknumfmt(Numroot, style);
}

char *
numid2fmtstr(int id)
{
	return lookfmtstr(Fmtroot, id);
}



static Numfmt *
addnum(Numfmt *nf, int style, int id)
{
	if(nf == nil){
		nf = malloc(sizeof(Numfmt));
		if(nf == nil)
			sysfatal("No memory for Numfmt\n");
		nf->style = style;
		nf->id = id;
		nf->left = nf->right = nil;
		return nf;
	}

	if(nf->style < style)
		nf->left = addnum(nf->left, style, id);
	if(nf->style > style)
		nf->right = addnum(nf->right, style, id);
	return nf;
}

static Fmtstr *
addfmt(Fmtstr *fs, int id, char *fmt)
{

	if(fs == nil){
		fs = malloc(sizeof(Fmtstr));
		if(fs == nil)
			sysfatal("No memory for Fmtstr\n");
		fs->id = id;
		fs->fmt = strdup(fmt);
		if(fs->fmt == nil)
			sysfatal("No memory for fmt\n");
		fs->left = fs->right = nil;
		return fs;
	}
	if(fs->id < id)
		fs->left = addfmt(fs->left, id, fmt);
	else
	if(fs->id > id)
		fs->right = addfmt(fs->right, id, fmt);
	return fs;
}

static void
dumpsty(Numfmt *nf)
{
	char *fmt;

	if(nf == nil)
		return;
	dumpsty(nf->right);
	if((fmt = lookfmtstr(Fmtroot, nf->id)) == nil)
		fmt = "<none>";
	fprint(2, "%-6d %-6d %q\n", nf->style, nf->id, fmt);
	dumpsty(nf->left);
}

void
dumpstyles(void)
{
	fprint(2, "%-6s %-6s %q\n", "styleid", "numid", "fmtstr");
	dumpsty(Numroot);
}

static void
numfmts(Elem *ep)
{
	int id;
	char *fmt, *v;

	id = -1;
	fmt = nil;
	for(; ep; ep = ep->next){
		if(strcmp(ep->name, "numFmt") == 0){
			if((v = xmlvalue(ep, "numFmtId")) != nil)
				id = atoi(v);
			if((v = xmlvalue(ep, "formatCode")) != nil)
				fmt = v;
		}
		if(id < 164)	/* seems these are builtin to excel */
			continue;

		if(id != -1 && fmt != nil)
			Fmtroot = addfmt(Fmtroot, id, fmt);
		id = -1;
		fmt = nil;
	}
}

static void
cellxfs(Elem *ep)
{
	int enab, style;
	char *v;

	style = 0;
	for(; ep; ep = ep->next)
		if(strcmp(ep->name, "xf") == 0){
			enab = 0;
			if((v = xmlvalue(ep, "applyNumberFormat")) != nil)
				enab = atoi(v);
			if(enab && (v = xmlvalue(ep, "numFmtId")) != nil)
				Numroot = addnum(Numroot, style, atoi(v));
			style++;
		}
}

void
rd_styles(Elem *base)
{
	Elem *ep;

	for(ep = base; ep; ep = ep->next)
		if(strcmp(ep->name, "cellXfs") == 0 && ep->child)
			cellxfs(ep->child);
	for(ep = base; ep; ep = ep->next){
		if(strcmp(ep->name, "numFmts") == 0 && ep->child)
			numfmts(ep->child);
	}
}

