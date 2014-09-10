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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#include "../vm/saurus.h"
#include "../vm/intern.h"
#include "../vm/seq.h"

typedef struct {
	char sign[4];
	unsigned char maj;
	unsigned char min;
	unsigned short flags;
} header;

char buffer[512];

static int encode_int8(lua_State *L) {
	char n = (char)lua_tointeger(L, -1);
	lua_pushlstring(L, &n, 1);
	return 1;
}

static int encode_int16(lua_State *L) {
	short n = (short)lua_tointeger(L, -1);
	lua_pushlstring(L, (char*)&n, 2);
	return 1;
}

static int encode_int32(lua_State *L) {
	int n = (int)lua_tointeger(L, -1);
	lua_pushlstring(L, (char*)&n, 4);
	return 1;
}

static int encode_uint8(lua_State *L) {
	unsigned char n = (unsigned char)lua_tointeger(L, -1);
	lua_pushlstring(L, (char*)&n, 1);
	return 1;
}

static int encode_uint16(lua_State *L) {
	unsigned short n = (unsigned short)lua_tointeger(L, -1);
	lua_pushlstring(L, (char*)&n, 2);
	return 1;
}

static int encode_uint32(lua_State *L) {
	unsigned n = (unsigned)lua_tointeger(L, -1);
	lua_pushlstring(L, (char*)&n, 4);
	return 1;
}

static int encode_number(lua_State *L) {
	double n = (double)lua_tonumber(L, -1);
	lua_pushlstring(L, (char*)&n, 8);
	return 1;
}

static int encode_header(lua_State *L) {
	header h;
	memcpy(h.sign, "\x1bsuc", 4);
	h.maj = (unsigned char)lua_tointeger(L, -2);
	h.min = (unsigned char)lua_tointeger(L, -1);
	h.flags = 0;
	
	assert(sizeof(header) == 8);
	lua_pushlstring(L, (char*)&h, 8);
	return 1;
}

static int encode_string(lua_State *L) {
	size_t len;
	char *mem;
	const char *str;
	unsigned size;
	
	if (lua_isnil(L, -1)) {
		lua_pushinteger(L, 0);
		encode_uint32(L);
		lua_remove(L, -2);
		return 1;
	}
	
	str = lua_tostring(L, -1);
	len = strlen(str) + 1;
	size = len + sizeof(unsigned);
	mem = (char*)malloc(size);
	
	if (!mem) {
		lua_pushstring(L, "Out of memory!");
		lua_error(L);
	}
	
	memcpy(mem, &len, sizeof(unsigned));
	memcpy(mem + sizeof(unsigned), str, len);
	lua_pushlstring(L, mem, size);
	
	free(mem);
	return 1;
}

int su_open_wrp(lua_State *L) {
	su_state *s = su_init(NULL);
	su_libinit(s);
	lua_pushlightuserdata(L, (void*)s);
	return 1;
}

int su_close_wrp(lua_State *L) {
	su_state *s = (su_state*)lua_touserdata(L, -1);
	su_close(s);
	return 0;
}

static int push_sexp(lua_State *L, su_state *s, value_t v);

static int push_sexp(lua_State *L, su_state *s, value_t v) {
	int ret = 0;
	int top = lua_gettop(L);
	switch (v.type) {
		case SU_NIL:
			lua_getglobal(L, "nil_substitute");
			return ret;
		case SU_NUMBER:
			lua_pushnumber(L, v.obj.num);
			return ret;
		case SU_BOOLEAN:
			lua_pushboolean(L, v.obj.b);
			return ret;
		case SU_STRING:
			lua_pushlstring(L, v.obj.str->str, v.obj.str->size - 1);
			return ret;
		case SU_SEQ:
		case IT_SEQ:
		case CELL_SEQ:
			lua_newtable(L);
			lua_pushinteger(L, 1);
			ret = push_sexp(L, s, seq_first(s, v.obj.q));
			if (ret) goto err;
			lua_settable(L, -3);
			lua_pushinteger(L, 2);
			ret = push_sexp(L, s, seq_rest(s, v.obj.q));
			if (ret) goto err;
			lua_settable(L, -3);
			return ret;
		default:
			return (int)v.type;
	}
err:
	lua_settop(L, top);
	return ret;
}

const char *code = NULL;

static const void *reader(size_t *size, void *data) {
	const char *tmp = code;
	if (!size) return NULL;
	if (!code) {
		*size = 0;
		return NULL;
	}
	code = NULL;
	*size = *(int*)data;
	return (*size > 0) ? tmp : NULL;
}

int su_load_call_wrp(lua_State *L) {
	su_state *s = (su_state*)lua_touserdata(L, -3);
	int len = lua_tointeger(L, -2);
	code = lua_tostring(L, -1);
	
	len = su_load(s, reader, &len);
	if (!len) {
		su_pushnil(s);
		su_call(s, 1, 1);
		push_sexp(L, s, s->stack[s->stack_top - 1]);
		su_pop(s, 1);
	} else {
		lua_pushnil(L);
	}
	return 1;
}

const luaL_Reg functions[] = {
	{"su_open", su_open_wrp},
	{"su_close", su_close_wrp},
	{"su_load_call", su_load_call_wrp},
	
	{"header", encode_header},
	{"string", encode_string},
	{"number", encode_number},
	{"int8", encode_int8},
	{"int16", encode_int16},
	{"int32", encode_int32},
	{"uint8", encode_uint8},
	{"uint16", encode_uint16},
	{"uint32", encode_uint32},
	{NULL, NULL}
};

int luaopen_writebin(lua_State *L) {
	luaL_register(L, "writebin", functions);
	return 1;
}