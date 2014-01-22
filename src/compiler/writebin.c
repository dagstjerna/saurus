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

int su_load_call_wrp(lua_State *L) {
	return 0;
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