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

#include "../vm/saurus.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 512

extern const char compiler_code[];
char buffer[BUFFER_SIZE];
char buffer2[BUFFER_SIZE];
char buffer3[BUFFER_SIZE];

const char *repl_help_text = "guru meditation...";

int luaopen_writebin(lua_State *L);

static const void *reader(size_t *size, void *data) {
	if (!size) return NULL;
	*size = fread(buffer, 1, sizeof(buffer), (FILE*)data);
	return (*size > 0) ? buffer : NULL;
}

int main(int argc, char *argv[]) {
	int ret, i;
	int compile;
	FILE *fp;
	jmp_buf err;
	su_state *s;
	lua_State *L;
	char *tmp, *tmp2;
	const char *input, *output;
	int print_help = argc > 1 && strstr("-h -v --help --version", argv[1]) != NULL;
	int pipe = argc > 1 && !strcmp("--", argv[1]);
	
	if (!pipe && (argc <= 1 || print_help)) {
		printf("S A U R U S\nCopyright (c) 2009-2014 Andreas T Jonsson <andreas@saurus.org>\nVersion: %s\n\n", su_version(NULL, NULL, NULL));
		if (print_help) {
			puts("Usage: saurus <options> <input.su> <output.suc>\n\tOptions:\n\t\t'-c' Compile source file to binary file.\n\t\t'--' read from STDIN.");
			return 0;
		}
	}
	
	s = su_init(NULL);
	su_libinit(s);
	
	L = lua_open();
	luaL_openlibs(L);
	luaopen_writebin(L);
	
	if (luaL_loadstring(L, compiler_code)) {
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_close(L);
		su_close(s);
		return -1;
	}
	
	if (lua_pcall(L, 0, 0, 0)) {
		lua_getglobal(L, "saurus_error");
		if (lua_isnil(L, -1))
			lua_pop(L, 1);
		fprintf(stderr, "%s\n", lua_tostring(L, -1));
		lua_close(L);
		su_close(s);
		return -1;
	}
	
	ret = 0;
	if (argc < 2 || pipe) {
		if (!pipe)
			puts("Type -h for help or 'q to quit.\n");
		su_pushstring(s, repl_help_text);
		su_setglobal(s, 0, "-h");
		
		ret = setjmp(err);
		if (!pipe)
			su_seterror(s, err, ret);
		
		jump: for (;;) {
			if (!pipe)
				printf("> ");
			fgets(buffer3, BUFFER_SIZE, stdin);
			tmp2 = tmpnam(buffer2);
			
			fp = fopen(tmp2, "w");
			if (!fp) {
				fprintf(stderr, "Could not open: %s\n", tmp2);
				remove(tmp2);
				su_close(s);
				return -1;
			}
			fputs(buffer3, fp);
			fclose(fp);
			
			tmp = tmpnam(buffer3);
			lua_getglobal(L, "entry");
			lua_pushstring(L, tmp2);
			lua_pushstring(L, tmp);
			if (lua_pcall(L, 2, 0, 0)) {
				remove(tmp);
				remove(tmp2);
				lua_getglobal(L, "saurus_error");
				if (lua_isnil(L, -1)) {
					lua_pop(L, 1);
					puts(lua_tostring(L, -1));
				} else {
					puts(lua_tostring(L, -1));
					lua_pop(L, 1);
				}
				lua_pop(L, 1);
				goto jump;
			}
				
			remove(tmp2);
			fp = fopen(tmp, "rb");
			if (!fp) {
				fprintf(stderr, "Could not open: %s\n", argv[1]);
				remove(tmp);
				su_close(s);
				lua_close(L);
				return -1;
			}
			
			su_assert(s, su_getglobal(s, "print"), "Could not retrieve 'print' function.");
			
			if (su_load(s, &reader, fp)) {
				su_close(s);
				lua_close(L);
				fclose(fp);
				remove(tmp);
				fprintf(stderr, "Could not load: %s\n", argv[1]);
				return -1;
			}
			
			fclose(fp);
			remove(tmp);
			
			su_vector(s, 0); /* ... */
			su_call(s, 1, 1);
			if (su_type(s, -1) == SU_STRING && !strcmp(su_tostring(s, -1, NULL), "q")) {
				ret = 0;
				break;
			}
			su_call(s, 1, 0);
		}
	} else {
		compile = !strcmp(argv[1], "-c");
		input = compile ? argv[2] : argv[1];
			
		if (compile) {
			if (argc < 4) {
				fputs("Expected input and output file!", stderr);
				lua_close(L);
				su_close(s);
				return -1;
			}
		} else {
			fp = fopen(input, "rb");
			if (!fp) {
				fprintf(stderr, "Could not open: %s\n", input);
				lua_close(L);
				su_close(s);
				return -1;
			}
		
			if (fread(buffer, 1, 1, fp) == 1) {
				if (*buffer == '\x1b') {
					rewind(fp);
					if (su_load(s, &reader, fp)) {
						su_close(s);
						lua_close(L);
						fclose(fp);
						fprintf(stderr, "Could not load: %s\n", input);
						return -1;
					}
				
					su_pushstring(s, input);
					for (i = 2; i < argc; i++)
						su_pushstring(s, argv[i]);
				
					su_call(s, argc - 1, 1);
					if (su_type(s, -1) == SU_NUMBER)
						ret = (int)su_tonumber(s, -1);
				
					su_close(s);
					lua_close(L);
					fclose(fp);
					return ret;
				}
			}
			fclose(fp);
		}
		
		tmp = tmpnam(buffer);
		output = compile ? argv[3] : tmp;
		
		lua_getglobal(L, "entry");
		lua_pushstring(L, input);
		lua_pushstring(L, output);
		if (lua_pcall(L, 2, 0, 0)) {
			lua_getglobal(L, "saurus_error");
			if (lua_isnil(L, -1))
				lua_pop(L, 1);
			fprintf(stderr, "%s\n", lua_tostring(L, -1));
			lua_close(L);
			su_close(s);
			return -1;
		}
		
		if (!compile) {
			fp = fopen(output, "rb");
			if (!fp) {
				fprintf(stderr, "Could not open: %s\n", output);
				lua_close(L);
				su_close(s);
				return -1;
			}
	
			if (su_load(s, &reader, fp)) {
				su_close(s);
				lua_close(L);
				fclose(fp);
				remove(tmp);
				fprintf(stderr, "Could not load: %s\n", output);
				return -1;
			}
	
			fclose(fp);
			remove(tmp);
		
			su_pushstring(s, argv[1]);
			for (i = 2; i < argc; i++)
				su_pushstring(s, argv[i]);
	
			su_call(s, argc - 1, 1);
			if (su_type(s, -1) == SU_NUMBER)
				ret = (int)su_tonumber(s, -1);
		}
	}
	
	lua_close(L);
	su_close(s);
	return ret;
}