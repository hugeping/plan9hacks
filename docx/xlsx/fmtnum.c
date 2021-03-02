#include <u.h>
#include <libc.h>
#include <bio.h>
#include <xml.h>
#include "xlsx.h"

/* for number format syntax see http://www.ozgrid.com/Excel/CustomFormats.htm */


static char *Months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

/*
 *		Robert J. Craig
 * 		AT&T Bell Laboratories
 *		1200 East Naperville Road
 *		Naperville, IL 60566-7045
 *
 * Given a number, v, this function outputs two integers,
 * d and n, such that
 *
 *	v = n / d
 *
 * to accuracy
 *	epsilon = | (v - n/d) / v | <= error
 *
 * input:  v = decimal number you want replaced by fraction.
 *	  error = accuracy to which the fraction should
 *		  represent the number v.
 *
 * output:  n = numerator of representing fraction.
 *	   d = denominator of representing fraction.
 *
 * return value:  -1.0 if(v < MIN || v > MAX || error < 0.0)
 *		 | (v - n/d) / v | otherwise.
 *
 * Note:  This program only works for positive numbers, v.
 *
 * reference:  Jerome Spanier and Keith B. Oldham, "An Atlas
 *	of Functions," Springer-Verlag, 1987, pp. 665-7.
 */

static double
fract(double v, int *n, int *d, double error)
{

	int D, N, t;
	double epsilon, r, m;

	double MIN = 4.0e+10;
	double MAX = 3.0e-10;

	if(v < MIN || v > MAX || error < 0.0)
		return -1.0;
	*d = D = 1;
	*n = v;
	N = *n+1;

	do{
		r = 0.0;
		if(v*(*d) != (double)*n){
			r = (N - v*D)/(v * *d - *n);
			if(r <= 1.0){
				t = N;
				N = *n;
				*n = t;
				t = D;
				D = *d;
				*d = t;
			}
		}
		epsilon = fabs(1.0 - *n/(v * *d));
		if(epsilon > error){
			m = 1.0;
			do{
				m *= 10.0;
			}while (m*epsilon < 1.0);
			epsilon = 1.0/m * (int)(0.5 + m*epsilon);
		}
	
//		print("%6d/%-6d   ε %g\n", *n, *d, epsilon);
		if(epsilon <= error)
			break;

		if(r <= 1.0)
			r = 1.0/r;

		N += *n *(int)r;
		D += *d *(int)r;
		(*n) += N;
		(*d) += D;

	}while(r != 0.0);
	return epsilon;
}

static Tm *
isotime(char *str)				/* parse ISO 8601 date format */
{
	static Tm tm;
	char tmp[128], *a[8];

	/* e.g. 2010-04-06T11:39:46Z */

	snprint(tmp, sizeof(tmp), "%s", str);
	if(getfields(tmp, a, nelem(a), 0, "-T:") != 6){
		fprint(2, "isotime: '%s' bad ISO 8601 date\n", str);
		return nil;
	}

	memset(&tm, 0, sizeof(tm));
	tm.year = strtol(a[0], nil, 10) - 1900;
	tm.mon = strtol(a[1], nil, 10) -1;
	tm.mday = strtol(a[2], nil, 10);
	tm.hour = strtol(a[3], nil, 10);
	tm.min = strtol(a[4], nil, 10);
	tm.sec = strtol(a[5], nil, 10);
	return &tm;
}

static struct Tm *
exceltime(double t)
{

	/* Beware - These epochs are wrong, this
	 * is due to Excel still remaining compatible
	 * with Lotus-123, which incorrectly believed 1900
	 * was a leap year
	 */
	if(Epoch1904)
		t -= 24107;		/* epoch = 1/1/1904 */
	else
		t -= 25569;		/* epoch = 1/1/1900 */
	t *= 60*60*24;		/* days to secconds */
	return localtime((long)t);
}

int
fmtnum(char *buf, int len, int id, char *str, int type)
{
	Tm *tm;
	double num, err;
	int i, n, d, h, m, s;

	num = atof(str);
	if(type == Date)
		tm = isotime(str);
	else
		tm = exceltime(num);

	switch(id){
	case 0:	 	// General
		snprint(buf, len, "%g", num);
		break;
	case 1:		// 0
		snprint(buf, len, "%.0f", num);
		break;
	case 2:		// 0.00
		snprint(buf, len, "%4.2f", num);
		break;
	case 3:	 	// #,##0
		snprint(buf, len, "%.0f", num);
		break;
	case 4:	 	// #,##0.00
		snprint(buf, len, "%.2f", num);
		break;
	case 9:	 	// 0%
		snprint(buf, len, "%.0f%%", num * 100.0);
		break;
	case 10:	 // 0.00%
		snprint(buf, len, "%6.2f%%", num * 100.0);
		break;
	case 11:	// 0.00E+00
		snprint(buf, len, "%.2e", num);
		break;
	case 12:	// # ?/?
		i = (int)num; num -= i;
		err = fract(num, &n, &d, 1e-10);
		if(err != 0)
			snprint(buf, len, "%d %d/%d (%+g)", i, n, d, err);
		else
			snprint(buf, len, "%d %d/%d", i, n, d);
		break;
	case 13:	// # ??/??
		i = (int)num; num -= i;
		err = fract(num, &n, &d, 1e-10);
		if(err != 0)
			snprint(buf, len, "%d %02d/%02d (%+g)", i, n, d, err);
		else
			snprint(buf, len, "%d %02d/%02d", i, n, d);
		break;
	case 14:	// mm-dd-yy
		snprint(buf, len, "%d-%s-%02d", tm->mday, Months[tm->mon], tm->year % 100);
		/*
		 * We don't use this form as it is Locale specific and we want
		 * to sidestep that horror
		 *		snprint(buf, len, "%d-%d-%02d", tm->mon +1, tm->mday, tm->year % 100);
		 */
		break;
	case 15:	// d-mmm-yy
		snprint(buf, len, "%d-%s-%02d", tm->mday, Months[tm->mon], tm->year % 100);
		break;
	case 16:	// d-mmm
		snprint(buf, len, "%d-%s", tm->mday, Months[tm->mon]);
		break;
	case 17:	// mmm-yy
		snprint(buf, len, "%s-%02d", Months[tm->mon], tm->year % 100);
		break;
	case 18:	// h:mm AM/PM
		snprint(buf, len, "%d:%02d %s", tm->hour % 12, tm->min, (tm->hour / 12)? "AM": "PM");
		break;
	case 19:	// h:mm:ss AM/PM
		snprint(buf, len, "%d:%02d:%02d %s", tm->hour %12, tm->min, tm->sec, (tm->hour / 12)? "AM": "PM");
		break;
	case 20:	// h:mm
		snprint(buf, len, "%d:%02d", tm->hour, tm->min);
		break;
	case 21:	// h:mm:ss
		snprint(buf, len, "%d:%02d:%02d", tm->hour, tm->min, tm->sec);
		break;
	case 22:	// m/d/yy h:mm
		snprint(buf, len, "%d/%s/%02d %d:%02d", tm->mday, Months[tm->mon], tm->year % 100, tm->hour, tm->min);
		/*
		 * We don't use the "proper" form as it is Locale specific and we want to sidestep that horror.
		 *		snprint(buf, len, "%d/%d/%02d %d:%02d", tm->mon+1, tm->mday, tm->year % 100, tm->hour, tm->min);
		 */
		break;
	case 37:	// #,##0 ;(#,##0)
		if(num == 0)
			snprint(buf, len, "0");
		else if (num < 0)
			snprint(buf, len, "(%.0f)", num);
		else
			snprint(buf, len, "%.0f", num);
		break;
	case 38:	// #,##0 ;[Red](#,##0)
		if(num == 0)
			snprint(buf, len, "0");
		else if (num < 0)
			snprint(buf, len, "(%.0f)", num);
		else
			snprint(buf, len, "%.0f", num);
		break;
	case 39:	// #,##0.00;(#,##0.00)
		if(num == 0)
			snprint(buf, len, "0");
		else if (num < 0)
			snprint(buf, len, "(%.2f)", num);
		else
			snprint(buf, len, "%.2f", num);
		break;
	case 40:	// #,##0.00;[Red](#,#)
		if(num == 0)
			snprint(buf, len, "0");
		else if (num < 0)
			snprint(buf, len, "(%.2f)", num);
		else
			snprint(buf, len, "%.2f", num);
		break;
	case 44:	// _("$"* #,##0.00_);_("$"* \(#,##0.00\);_("$"* "-"??_);_(@_)  <Not published>
		if(num == 0)
			snprint(buf, len, "%s 0", Currency);
		else if (num < 0)
			snprint(buf, len, "(%s %.2f)", Currency, num);
		else
			snprint(buf, len, "%s %.2f", Currency, num);
		break;

	case 45:	// mm:ss
		snprint(buf, len, "%02d:%02d", tm->min, tm->sec);
		break;
	case 46:	// [h]:mm:ss
		h = num / 60; num -= h * 60;
		m = num / 60; num -= m * 60;
		s = num / 60;
		if(h)
			snprint(buf, len, "%d:%02d:%02d", h, m, s);
		else
			snprint(buf, len, ":%02d:%02d", m, s);
		break;
	case 47:	// mmss.0
		h = num / 60; num -= h * 60;
		m = num / 60; num -= m * 60;
		s = num / 60;
			snprint(buf, len, "%02d:%02d.0", m, s);
		break;
	case 48:	// ##0.0
		snprint(buf, len, "%.1f", num);
		break;
	case 49:	// @
		snprint(buf, len, "%s", str);
		break;

#ifdef Coraid
	/*
	 * special custom numfmts for coraid's Spreadsheets,
	 * these will almost definitely be very, very wrong
	 * for anyone else, you have been warned.
	 */
	case 164:	// "$"#,##0.00
		snprint(buf, len, "$ %.2f", num);
		break;
	case 165:	// "$"#,##0.0000
		snprint(buf, len, "$ %.4f", num);
		break;
	case 166:	// "$"#,##0.0000_);[Red]\("$"#,##0.0000\)
		if(num >= 0)
			snprint(buf, len, "$ %.4f", num);
		else
			snprint(buf, len, "($ %.4f)", num);
		break;
#endif

	default:
		return -1;
	}
	return 0;
}
