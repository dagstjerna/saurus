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
#include "intern.h"
#include "ref.h"

value_t ref_local(su_state *s, value_t *val) {
	value_t v;
	v.type = SU_LOCAL;
	v.obj.loc = (local_t*)allocate(s, NULL, sizeof(local_t));
	v.obj.loc->v = *val;
	gc_insert_object(s, &v.obj.loc->gc, SU_LOCAL);
	return v;
}

void su_ref_local(su_state *s, int idx) {
	value_t v = ref_local(s, STK(idx));
	push_value(s, &v);
}

void su_unref_local(su_state *s, int idx) {
	push_value(s, &STK(idx)->obj.loc->v);
}

void su_set_local(su_state *s, int idx) {
	local_t *loc = STK(idx)->obj.loc;
	loc->v = *STK(-1);
	su_pop(s, 1);
}
