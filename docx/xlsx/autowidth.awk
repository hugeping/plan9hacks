
BEGIN{
	nrow = 1;
	ncol = 1;
}

{
	for(c = 1; c <= NF; c++){
		l = length($c);
		if(l > widths[c])
			widths[c] = l;
		if(c > ncol)
			ncol = c;

		tab[nrow,c] = $c;
	}
	nrow++;
}

END {

	for(r = 1; r < nrow; r++){
		for(c = 1; c <= ncol; c++)
			printf("%*.*s ", -widths[c], widths[c], tab[r, c]);
		printf("\n");
	}
}
