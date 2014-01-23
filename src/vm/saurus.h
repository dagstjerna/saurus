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

#ifndef _SAURUS_H_
#define _SAURUS_H_

#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>

typedef struct state su_state;

enum {
	SU_FALSE,
	SU_TRUE
};

enum su_object_type {
	SU_INV, SU_NIL, SU_BOOLEAN, SU_STRING, SU_NUMBER,
	SU_SEQ, SU_FUNCTION, SU_NATIVEFUNC, SU_VECTOR, SU_MAP,
	SU_LOCAL, SU_NATIVEPTR, SU_NATIVEDATA, SU_NUM_OBJECT_TYPES
};

typedef enum su_object_type su_object_type_t;

typedef int (*su_nativefunc)(su_state*,int);
typedef const void* (*su_reader)(size_t*,void*);
typedef void* (*su_alloc)(void*,size_t);

su_state *su_init(su_alloc alloc);
void su_close(su_state *s);
void su_libinit(su_state *s);
const char *su_version(int *major, int *minor, int *patch);
void *su_allocate(su_state *s, void *p, size_t n);

void su_seterror(su_state *s, jmp_buf jmp, int flag);
void su_error(su_state *s, const char *fmt, ...);
void su_assert(su_state *s, int cond, const char *fmt, ...);
void su_check_type(su_state *s, int idx, su_object_type_t t);
void su_check_arguments(su_state *s, int num, ...);

const char *su_stringify(su_state *s, int idx);
const char *su_type_name(su_state *s, int idx);
su_object_type_t su_type(su_state *s, int idx);

void su_pushnil(su_state *s);
void su_pushfunction(su_state *s, su_nativefunc f);
su_nativefunc su_tofunction(su_state *s, int idx);
void su_pushnumber(su_state *s, double n);
double su_tonumber(su_state *s, int idx);
void su_pushboolean(su_state *s, int b);
int su_toboolean(su_state *s, int idx);
void su_pushinteger(su_state *s, int i);
int su_tointeger(su_state *s, int idx);
void su_pushbytes(su_state *s, const char *ptr, unsigned size);
void su_pushstring(su_state *s, const char *str);
const char *su_tostring(su_state *s, int idx, unsigned *size);
void su_pushpointer(su_state *s, void *ptr);
void *su_topointer(su_state *s, int idx);

void su_ref_local(su_state *s, int idx);
void su_unref_local(su_state *s, int idx);
void su_set_local(su_state *s, int idx);

void su_seq(su_state *s, int idx);
void su_list(su_state *s, int num);
void su_first(su_state *s, int idx);
void su_rest(su_state *s, int idx);
void su_cons(su_state *s, int idx);

void su_vector(su_state *s, int num);
int su_vector_length(su_state *s, int idx);
void su_vector_index(su_state *s, int idx);
void su_vector_set(su_state *s, int idx);
void su_vector_push(su_state *s, int idx, int num);
void su_vector_pop(su_state *s, int idx, int num);

void su_map(su_state *s, int num);
int su_map_length(su_state *s, int idx);
int su_map_get(su_state *s, int idx);
void su_map_insert(su_state *s, int idx);
void su_map_remove(su_state *s, int idx);
int su_map_has(su_state *s, int idx);

int su_getglobal(su_state *s, const char *name);
void su_setglobal(su_state *s, int replace, const char *name);

int su_load(su_state *s, su_reader reader, void *data);
void su_call(su_state *s, int narg, int nret);
void su_pop(su_state *s, int n);
void su_copy(su_state *s, int idx);
void su_copy_range(su_state *s, int idx, int num);
void su_top(su_state *s);

void su_gc(su_state *s);

FILE *su_stdout(su_state *s);
FILE *su_stdin(su_state *s);
FILE *su_stderr(su_state *s);
void su_set_stdout(su_state *s, FILE *fp);
void su_set_stdin(su_state *s, FILE *fp);
void su_set_stderr(su_state *s, FILE *fp);

#endif
