#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "instead/instead.h"
#define WIDTH 70

static int log_opt = 0;
static int tiny_init(void)
{
	int rc;
	rc = instead_loadfile(STEAD_PATH"/tiny.lua");
	if (rc)
		return rc;
	return 0;
}
static struct instead_ext ext = {
	.init = tiny_init,
};

#define utf_cont(p) ((*(p) & 0xc0) == 0x80)

static int utf_ff(const char *s, const char *e)
{
	int l = 0;
	if (!s || !e)
		return 0;
	if (s > e)
		return 0;
	if ((*s & 0x80) == 0) /* ascii */
		return 1;
	l = 1;
	while (s < e && utf_cont(s + 1)) {
		s ++;
		l ++;
	}
	return l;
}

static void fmt(const char *str, int width)
{
	int l;
	const char *ptr = str;
	const char *eptr = ptr + strlen(str);
	const char *s = str;
	const char *last = NULL;
	char c;
	int w = 0;
	while (l = utf_ff(ptr, eptr)) {
		c = *ptr;
		ptr += l;
		w ++;
		if (c == ' ' || c == '\t' || c == '\n')
			last = ptr;
		if ((width > 0 && w > width && last) || c == '\n') {
			while(s != last)
				putc(*s++, stdout);
			if (c != '\n')
				putc('\n', stdout);
			w = 0;
			last = s;
			continue;
		}
	}
	while(s != eptr)
		putc(*s++, stdout);
	putc('\n', stdout);
}

int main(int argc, const char **argv)
{
	int rc; char *str; const char *game = NULL;
	int opt_debug = 0;
	int opt_width = WIDTH;
	int i = 0;
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-d"))
			opt_debug = 1;
		else if (!strncmp(argv[i], "-w", 2))
			opt_width = atoi(argv[i] + 2);
		else if (!game)
			game = argv[i];
	}
	if (!game) {
		fprintf(stderr, "Usage: %s [-d] [-w<width>] <game>\n", argv[0]);
		exit(1);
	}
	if (instead_extension(&ext)) {
		fprintf(stderr, "Can't register tiny extension\n");
		exit(1);
	}
	instead_set_debug(opt_debug);

	if (instead_init(game)) {
		fprintf(stderr, "Can not init game: %s\n", game);
		exit(1);
	}
	if (instead_load(NULL)) {
		fprintf(stderr, "Can not load game: %s\n", instead_err());
		exit(1);
	}
#if 0 /* no autoload */
	str = instead_cmd("load autosave", &rc);
#else
	str = instead_cmd("look", &rc);
#endif
	if (!rc) {
		fmt(str, opt_width); fflush(stdout);
	}
	free(str);

	while (1) {
		char input[256], *p, cmd[256 + 64];
		printf("> "); fflush(stdout);
		p = fgets(input, sizeof(input), stdin);
		if (!p)
			break;
		p[strcspn(p, "\n\r")] = 0;
		if (!strcmp(p, "quit"))
			break;
		if (!strcmp(p, "log")) {
			log_opt = 1;
			continue;
		}
		if (!strncmp(p, "load ", 5) || !strncmp(p, "save ", 5))
			snprintf(cmd, sizeof(cmd), "%s", p);
		else
			snprintf(cmd, sizeof(cmd), "@metaparser \"%s\"", p);
		str = instead_cmd(cmd, &rc);
		if (rc && !str)
			str = instead_cmd(p, NULL);
		if (str) {
			char *eptr = str + strlen(str);
			while ((*eptr == '\n' || *eptr == 0) && eptr != str) *eptr-- = 0;
			fmt(str, opt_width); fflush(stdout);
		}
		free(str);
		if (log_opt) fprintf(stderr, "%s\n", p);
	}
	instead_cmd("save autosave", NULL);
	instead_done();
	return(0);
}
