/*
 * Generate a state transition diagram from the state tables,
 * we skip error transitions as they just confuse the drawing.
 */
#include <u.h>
#include <libc.h>
#include <bio.h>
#include "state-machine.h"

void
main(void)
{
	int os, s, a, t;

	print("digraph xml_parser {\n");
	print("	rankdir=LR;\n");
	print("	size=\"11,9\"\n");
	print("	node [shape = circle];\n");

	for(t = 0; t < NumToks; t++)
		for(os = 0; os < NumStates; os++){
			s = statab[os][t];
			a = acttab[os][t];
			if(a != Aerr)
				print("	%s -> %s [ label = \"%s / %s\" ];\n",
					stastr[os], stastr[s], tokstr[t], actstr[a]);
		}

	print("}\n");
	exits(nil);
}

