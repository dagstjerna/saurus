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

#include <stdio.h>

static int open(su_state *s, int narg) {
	FILE *fp;
	su_check_arguments(s, 2, SU_STRING, SU_STRING);
	fp = fopen(su_tostring(s, -2, NULL), su_tostring(s, -1, NULL));
	su_pushpointer(s, fp ? fp : NULL);
	return 1;
}

static int close(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_NATIVEPTR);
	fclose(su_topointer(s, -1));
	return 0;
}

static int not_implemented(su_state *s, int narg) {
	su_error(s, "Not implemented!");
	return 0;
}

static int write(su_state *s, int narg) {
	unsigned size;
	const char *str;
	su_check_arguments(s, 2, SU_NATIVEPTR, SU_NUMBER);
	str = su_tostring(s, -1, &size);
	su_pushnumber(s, (double)fwrite(str, 1, size - 1, su_topointer(s, -2)));
	return 1;
}

static int read_file(su_state *s, int narg) {
	unsigned count, size;
	char *buffer;
	int ret;
	FILE *fp;
	
	su_check_arguments(s, 1, SU_NATIVEPTR);
	fp = fopen(su_tostring(s, -1, NULL), "rb");
	if (!fp)
		return 0;

	if (fseek(fp, 0, SEEK_END)) {
		fclose(fp);
		return 0;
	}
	
	size = (unsigned)ftell(fp);
	if (size == (unsigned)-1) {
		fclose(fp);
		return 0;
	}
	
	rewind(fp);
	count = 0;
	buffer = (char*)malloc(size);
	if (!buffer) {
		fclose(fp);
		return 0;
	}
	
	do {
		ret = fread(buffer + count, 1, size - count, fp);
		if (ret < 0) {
			fclose(fp);
			free(buffer);
			return 0;
		}
		count += ret;
	} while (count < size);
	
	su_pushbytes(s, buffer, size);
	fclose(fp);
	free(buffer);
	return 1;
}

static int write_file(su_state *s, int narg) {
	unsigned size, count;
	const char *str;
	int ret;
	FILE *fp;
	
	su_check_arguments(s, 2, SU_STRING, SU_STRING);
	fp = fopen(su_tostring(s, -2, NULL), "wb");
	if (!fp)
		return 0;

	str = su_tostring(s, -1, &size);
	count = --size;
	do {
		ret = fwrite(str, 1, count, fp);
		if (ret < 0) {
			fclose(fp);
			return 0;
		}
		count -= ret;
	} while (count);
	
	fclose(fp);
	su_pushnumber(s, (double)size);
	return 1;
}

static int size(su_state *s, int narg) {
	unsigned size;
	FILE *fp;
	
	su_check_arguments(s, 1, SU_NATIVEPTR);
	fp = fopen(su_tostring(s, -1, NULL), "rb");
	if (!fp)
		return 0;
	
	if (fseek(fp, 0, SEEK_END)) {
		fclose(fp);
		return 0;
	}
	
	size = (unsigned)ftell(fp);
	if (size == (unsigned)-1) {
		fclose(fp);
		return 0;
	}
	
	fclose(fp);
	su_pushnumber(s, (double)size);
	return 1;
}

static int error(su_state *s, int narg) {
	int err;
	FILE *fp;
	su_check_arguments(s, 1, SU_NATIVEPTR);
	fp = su_topointer(s, -1);
	err = ferror(fp);
	if (err) {
		su_pushinteger(s, ferror(fp));
		return 1;
	}
	return 0;
}

void libio(su_state *s) {
	su_pushfunction(s, &not_implemented);
	su_setglobal(s, 1, "io-file?");
	su_pushfunction(s, &open);
	su_setglobal(s, 1, "io-open");
	su_pushfunction(s, &close);
	su_setglobal(s, 1, "io-close");
	su_pushfunction(s, &not_implemented);
	su_setglobal(s, 1, "io-read");
	su_pushfunction(s, &write);
	su_setglobal(s, 1, "io-write");
	su_pushfunction(s, &read_file);
	su_setglobal(s, 1, "io-read-file");
	su_pushfunction(s, &write_file);
	su_setglobal(s, 1, "io-write-file");
	su_pushfunction(s, &size);
	su_setglobal(s, 1, "io-size");
	su_pushfunction(s, &error);
	su_setglobal(s, 1, "io-error");
	su_pushfunction(s, &not_implemented);
	su_setglobal(s, 1, "io-flush");
	su_pushfunction(s, &not_implemented);
	su_setglobal(s, 1, "io-seek");
	su_pushfunction(s, &not_implemented);
	su_setglobal(s, 1, "io-set-buffering");
	su_pushfunction(s, &not_implemented);
	su_setglobal(s, 1, "io-temp-file");
	su_pushfunction(s, &not_implemented);
	su_setglobal(s, 1, "io-popen");
	su_pushfunction(s, &not_implemented);
	su_setglobal(s, 1, "io-input");
	su_pushfunction(s, &not_implemented);
	su_setglobal(s, 1, "io-output");
	su_pushfunction(s, &not_implemented);
	su_setglobal(s, 1, "io-line-seq");
	
	su_pushpointer(s, su_stdin(s));
	su_setglobal(s, 1, "io-stdin");
	su_pushpointer(s, su_stdout(s));
	su_setglobal(s, 1, "io-stdout");
	su_pushpointer(s, su_stderr(s));
	su_setglobal(s, 1, "io-stderr");
}
