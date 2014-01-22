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

static int seq(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_NIL);
	su_seq(s, -1);
	return 1;
}

static int list(su_state *s, int narg) {
	if (narg > 0) {
		su_list(s, narg);
		return 1;
	} else {
		return 0;
	}
}

static int cons(su_state *s, int narg) {
	su_check_arguments(s, 2, SU_NIL, SU_NIL);
	su_cons(s, -2);
	return 1;
}

static int first(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_SEQ);
	su_first(s, -1);
	return 1;
}

static int rest(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_SEQ);
	su_rest(s, -1);
	return 1;
}

static int vector(su_state *s, int narg) {
	su_vector(s, narg);
	return 1;
}

static int vector_length(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_VECTOR);
	su_vector_length(s, -1);
	return 1;
}

static int vector_index(su_state *s, int narg) {
	su_check_arguments(s, 2, SU_VECTOR, SU_NUMBER);
	su_copy(s, -1);
	su_vector_index(s, -2);
	return 1;
}

static int vector_set(su_state *s, int narg) {
	su_check_arguments(s, 3, SU_VECTOR, SU_NUMBER, SU_NIL);
	su_copy(s, -2);
	su_copy(s, -2);
	su_vector_set(s, -3);
	return 1;
}

static int vector_push(su_state *s, int narg) {
	su_check_arguments(s, -1, SU_VECTOR);
	su_vector_push(s, -narg, narg - 1);
	return 1;
}

static int vector_pop(su_state *s, int narg) {
	su_check_arguments(s, 2, SU_VECTOR, SU_NUMBER);
	su_vector_pop(s, -2, -1);
	return 1;
}

static int map(su_state *s, int narg) {
	su_map(s, narg);
	return 1;
}

static int map_length(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_MAP);
	su_pushinteger(s, su_map_length(s, -1));
	return 1;
}

static int map_get(su_state *s, int narg) {
	su_check_arguments(s, 2, SU_MAP, SU_NIL);
	return su_map_get(s, -2);
}

static int map_set(su_state *s, int narg) {
	su_check_arguments(s, 3, SU_MAP, SU_NIL, SU_NIL);
	su_map_insert(s, -3);
	return 1;
}

static int map_insert(su_state *s, int narg) {
	su_check_arguments(s, 3, SU_MAP, SU_NIL, SU_NIL);
	su_assert(s, !su_map_has(s, -3), "Duplicated key in map!");
	su_map_insert(s, -3);
	return 1;
}

static int map_remove(su_state *s, int narg) {
	su_check_arguments(s, 2, SU_MAP, SU_NIL);
	su_assert(s, su_map_has(s, -3), "Key does not exist in map!");
	su_map_remove(s, -1);
	return 1;
}

static int map_has(su_state *s, int narg) {
	su_check_arguments(s, 2, SU_MAP, SU_NIL);
	su_pushboolean(s, su_map_has(s, -1));
	return 1;
}

void libseq(su_state *s) {
	su_pushfunction(s, &seq);
	su_setglobal(s, 1, "seq");
	su_pushfunction(s, &list);
	su_setglobal(s, 1, "list");
	su_pushfunction(s, &cons);
	su_setglobal(s, 1, "cons");
	su_pushfunction(s, &first);
	su_setglobal(s, 1, "first");
	su_pushfunction(s, &rest);
	su_setglobal(s, 1, "rest");
	
	su_pushfunction(s, &vector);
	su_setglobal(s, 1, "vector");
	su_pushfunction(s, &vector_length);
	su_setglobal(s, 1, "vector-length");
	su_pushfunction(s, &vector_index);
	su_setglobal(s, 1, "vector-index");
	su_pushfunction(s, &vector_set);
	su_setglobal(s, 1, "vector-set");
	su_pushfunction(s, &vector_push);
	su_setglobal(s, 1, "vector-push");
	su_pushfunction(s, &vector_pop);
	su_setglobal(s, 1, "vector-pop");
	
	su_pushfunction(s, &map);
	su_setglobal(s, 1, "map");
	su_pushfunction(s, &map_length);
	su_setglobal(s, 1, "map-length");
	su_pushfunction(s, &map_get);
	su_setglobal(s, 1, "map-get");
	su_pushfunction(s, &map_set);
	su_setglobal(s, 1, "map-set");
	su_pushfunction(s, &map_insert);
	su_setglobal(s, 1, "map-insert");
	su_pushfunction(s, &map_remove);
	su_setglobal(s, 1, "map-remove");
	su_pushfunction(s, &map_has);
	su_setglobal(s, 1, "map-has");
}
