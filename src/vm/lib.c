/******************************************************************************/
/* S A U R U S                                                                */
/* Copyright (c) 2009-2014 Andreas T Jonsson <andreas@saurus.org>             */
/*                                                                            */
/* This software is provided 'as-is', without any express or implied          */
/* warranty. In no event will the authors be held liable for any damages      */
/* arising from the use of this software.                                     */
/*                                                                            */
/* Permission is granted to anyone to use this software for any purpose,      */
/* including commercial applications, and to alter it and redistribute it     */
/* freely, subject to the following restrictions:                             */
/*                                                                            */
/* 1. The origin of this software must not be misrepresented; you must not    */
/*    claim that you wrote the original software. If you use this software    */
/*    in a product, an acknowledgment in the product documentation would be   */
/*    appreciated but is not required.                                        */
/*                                                                            */
/* 2. Altered source versions must be plainly marked as such, and must not be */
/*    misrepresented as being the original software.                          */
/*                                                                            */
/* 3. This notice may not be removed or altered from any source               */
/*    distribution.                                                           */
/******************************************************************************/

#include "saurus.h"
#include "platform.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

typedef struct {
	const char *buffer;
	unsigned size;
} buffer_and_size;

static void print_rec(su_state *s, int idx);

static const void *reader(size_t *size, void *data) {
	buffer_and_size *bas = (buffer_and_size*)data;
	if (!size) return NULL;
	*size = bas->size;
	bas->size = 0;
	return *size ? bas->buffer : NULL;
}

static int load(su_state *s, int narg) {
	buffer_and_size bas;
	su_check_arguments(s, 1, SU_STRING);
	bas.buffer = su_tostring(s, -1, &bas.size);
	su_pushboolean(s, su_load(s, &reader, &bas) ? SU_TRUE : SU_FALSE);
	return 1;
}

static void print_rec(su_state *s, int idx) {
	int i, tmp;
	const char *str;
	FILE *fp = stdout;
	int type = su_type(s, idx);
	
	if (su_type(s, -1) == SU_SEQ) {
		fprintf(fp, "(");
		for (i = 0; su_type(s, -1) != SU_NIL; i++) {
			if (i > 0) fprintf(fp, " ");
			su_first(s, -1);
			print_rec(s, -1);
			su_pop(s, 1);
			su_rest(s, -1);
		}
		fprintf(fp, ")");
		su_pop(s, i);
	} else {
		switch (type) {
			case SU_VECTOR:
				tmp = su_vector_length(s, idx);
				fprintf(fp, "[");
				for (i = 0; i < tmp; i++) {
					if (i > 0) fprintf(fp, " ");
					su_pushinteger(s, i);
					su_vector_index(s, idx - 1);
					print_rec(s, -1);
					su_pop(s, 1);
				}
				fprintf(fp, "]");
				break;
			case SU_MAP:
				fprintf(fp, "{");
				fprintf(fp, "}");
				break;
			case SU_STRING:
				str = su_tostring(s, idx, (unsigned*)&tmp);
				/*
				for (i = 0; i < tmp && i < 32; i++) {
					if (str[i] == ' ') {
						fprintf(fp, "\"%s\"", str);
						return;
					}
				}
				*/
				fprintf(fp, "%s", str);
				break;
			default:
				fprintf(fp, "%s", su_stringify(s, idx));
		}
	}
}

static int print(su_state *s, int narg) {
	for (; narg; narg--) {
		print_rec(s, -narg);
		fputs("\t", su_stdout(s));
	}
	fputs("\n", su_stdout(s));
	return 0;
}

static int type(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_NIL);
	su_pushstring(s, su_type_name(s, -1));
	return 1;
}

static int string(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_NIL);
	su_pushstring(s, su_stringify(s, -1));
	return 1;
}

static int number(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_STRING);
	su_pushnumber(s, atof(su_tostring(s, -1, NULL)));
	return 1;
}

static int unref(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_LOCAL);
	su_unref_local(s, -1);
	return 1;
}

static int ref(su_state *s, int narg) {
	su_check_arguments(s, 2, SU_LOCAL, SU_NIL);
	su_ref_local(s, -1);
	return 1;
}

static int set(su_state *s, int narg) {
	su_check_arguments(s, 2, SU_LOCAL, SU_NIL);
	su_copy(s, -1);
	su_set_local(s, -3);
	return 1;
}

static int input(su_state *s, int narg) {
	FILE *fp;
	su_check_arguments(s, 1, SU_NIL);
	if (su_type(s, -1) == SU_NIL) {
		if (su_stdin(s) != stdin)
			fclose(su_stdin(s));
		su_set_stdin(s, stdin);
	} else if (su_type(s, -1) == SU_STRING) {
		fp = fopen(su_tostring(s, -1, NULL), "rb");
		su_assert(s, fp != NULL, "Error: %d (%s)", errno, strerror(errno));
		if (su_stdin(s) != stdin)
			fclose(su_stdin(s));
		su_set_stdin(s, fp);
	} else {
		su_error(s, "Unexpected argument!");
	}
	return 0;
}

static int output(su_state *s, int narg) {
	FILE *fp;
	su_check_arguments(s, 1, SU_NIL);
	if (su_type(s, -1) == SU_NIL) {
		if (su_stdout(s) != stdout)
			fclose(su_stdout(s));
		su_set_stdout(s, stdout);
	} else if (su_type(s, -1) == SU_STRING) {
		fp = fopen(su_tostring(s, -1, NULL), "wb");
		su_assert(s, fp != NULL, "IO error: %d (%s)", errno, strerror(errno));
		if (su_stdout(s) != stdout)
			fclose(su_stdout(s));
		su_set_stdout(s, fp);
	} else {
		su_error(s, "Unexpected argument!");
	}
	return 0;
}

static int write(su_state *s, int narg) {
	unsigned size;
	const char *str;
	su_check_arguments(s, 1, SU_STRING);
	str = su_tostring(s, -1, &size);
	su_pushinteger(s, (int)fwrite(str, 1, size - 1, su_stdout(s)));
	return 1;
}

static int read(su_state *s, int narg) {
	int size;
	void *mem;
	size_t res;
	char buffer[4096];
	su_check_arguments(s, 1, SU_NUMBER);
	size = su_tointeger(s, -1);
	if (size > 0) {
		mem = su_allocate(s, NULL, size);
		res = fread(mem, 1, size, su_stdin(s));
		if (!res) {
			su_allocate(s, mem, 0);
			return 0;
		}
		su_pushbytes(s, mem, size);
		su_allocate(s, mem, 0);
	} else if (size < 0) {
		if (su_stdin(s) == stdin) {
			su_pushstring(s, gets(buffer));
			return 1;
		}
		fseek(su_stdin(s), 0, SEEK_END);
		size = (int)ftell(su_stdin(s));
		fseek(su_stdin(s), 0, SEEK_CUR);
		mem = su_allocate(s, NULL, size);
		su_assert(s, size == (int)fread(mem, 1, size, su_stdin(s)), "IO error: %d (%s)", errno, strerror(errno));
		su_pushbytes(s, mem, size);
		su_allocate(s, mem, 0);
	} else {
		su_pushstring(s, "");
	}
	return 1;
}

extern void libseq(su_state *s);

void su_libinit(su_state *s) {
	su_pushfunction(s, &print);
	su_setglobal(s, 1, "print");
	su_pushfunction(s, &type);
	su_setglobal(s, 1, "type?");
	su_pushfunction(s, &string);
	su_setglobal(s, 1, "string!");
	su_pushfunction(s, &number);
	su_setglobal(s, 1, "number!");
	
	su_pushfunction(s, &unref);
	su_setglobal(s, 1, "unref");
	su_pushfunction(s, &ref);
	su_setglobal(s, 1, "ref");
	su_pushfunction(s, &set);
	su_setglobal(s, 1, "set");
	
	su_pushfunction(s, &input);
	su_setglobal(s, 1, "input");
	su_pushfunction(s, &output);
	su_setglobal(s, 1, "output");
	su_pushfunction(s, &read);
	su_setglobal(s, 1, "read");
	su_pushfunction(s, &write);
	su_setglobal(s, 1, "write");
	
	libseq(s);
}
