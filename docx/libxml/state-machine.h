enum {			/* Lexer Tokens */
	Twhite = 0,
	Topen,
	Tname,
	Tclose,
	Tequal,
	Tendblk,
	Tnulblk,
	NumToks
};

enum {			/* Parser states */
	Slost	= 0,
	Sopened	= 1,
	Snamed	= 2,
	Sattred	= 3,
	Sequed	= 4,
	Sendblk	= 5,
	Sclosed	= 6,
	NumStates

};

enum {			/* Parser Actions */
	Aerr	= 0,
	Anop	= 1,
	Aelem	= 2,
	Apcdata	= 3,
	Aattr	= 4,
	Avalue	= 5,
	Aup	= 6,
	Adown	= 7,
	Acheck	= 8,
	NumActions
};

static char *
tokstr[] = {	/* lexer token names for debug */
	[Twhite]	"white",	[Topen]		"open",
	[Tname]		"name",		[Tclose]	"close",
	[Tequal]	"equal",	[Tendblk]	"endblk",
	[Tnulblk]	"nulblk"
};

static char *
stastr[] = {	/* parser state names for debug */
	[Slost]		"lost",		[Sopened]	"opened",
	[Snamed]	"named",	[Sattred]	"attred",
	[Sequed]	"equed",	[Sendblk]	"endblk",
	[Sclosed]	"closed",
};

static char *
actstr[] = {	/* parser action names for debug */
	[Aerr]		"error",	[Anop]		"nop",
	[Apcdata]	"pcdata",	[Aattr]		"attr",
	[Avalue]	"value",	[Aelem]		"elem",
	[Aup]		"up",		[Adown]		"down",
	[Acheck]	"check"
};


static int statab[7][7] = {	/* Parser state transition table */
/* 			Twhite	Topen	Tname	Tclose	Tequal	Tendblk	Tnulblk */ 
	[Slost]     {	Slost, 	Sopened,Slost, 	Slost, 	Slost, 	Slost,	Slost },
	[Sopened]   {	0, 	0, 	Snamed,	0, 	0, 	0, 	0 },
	[Snamed]    {	Snamed, 0, 	Sattred,Sendblk,0, 	Slost,	Slost },            
	[Sattred]   {	Sattred, 0, 	0, 	0, 	Sequed, 0, 	0 },
	[Sequed]    {	Sequed, 0, 	Snamed,	0, 	0, 	0, 	0 },
	[Sendblk]   {	0, 	0, 	Sclosed,0, 	0, 	0, 	0 },
	[Sclosed]   {	0,	0,	0,	Slost,	0,	0,	0 },          
};

static int acttab[7][7] = {	/* Parser action table */
/* 			Twhite	Topen	Tname	Tclose	Tequal	Tendblk	Tnulblk */         
	[Slost]     {	Apcdata, Anop, 	Apcdata, Apcdata, Apcdata, Aup,	Apcdata },
	[Sopened]   {	0, 	0, 	Aelem,	0, 	0, 	0, 	0 },
	[Snamed]    {	Anop,	0, 	Aattr,	Adown,	0, 	Anop,	Anop },            
	[Sattred]   {	Anop, 	0, 	0, 	0, 	Anop,	0, 	0 },
	[Sequed]    {	Anop, 	0, 	Avalue,	0, 	0, 	0, 	0 },
	[Sendblk]   {	0, 	0, 	Acheck, 0, 	0, 	0, 	0 },
	[Sclosed]   {	0,	0,	0,	Anop,	0,	0,	0 },          
};
