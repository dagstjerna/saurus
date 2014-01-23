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
#include "seq.h"
#include "ref.h"
#include "gc.h"

#include <assert.h>

static void free_prot(su_state *s, prototype_t *prot);

static void add_to_gray(su_state *s, gc_t *obj) {
	if (obj->flags != GC_FLAG_WHITE)
		return;
	assert(s->gc_gray_size <= GC_GRAY_SIZE);
	obj->flags = GC_FLAG_GRAY;
	s->gc_gray[s->gc_gray_size++] = obj;
}

static void add_to_black(gc_t *obj) {
	obj->flags = GC_FLAG_BLACK;
}

static gc_t *get_gc_object(value_t *v) {
	switch (v->type) {
		case SU_INV:
		case SU_NIL:
		case SU_BOOLEAN:
		case SU_NUMBER:
		case SU_NATIVEFUNC:
		case SU_NATIVEPTR:
			return NULL;
	}
	return v->obj.gc_object;
}

static void gray_vector(su_state *s, gc_t *obj) {
	vector_t *v = (vector_t*)obj;
	add_to_gray(s, (gc_t*)v->root);
	add_to_gray(s, (gc_t*)v->tail);
}

static void gray_vector_node(su_state *s, gc_t *obj) {
	int i;
	gc_t *child;
	vector_node_t *node = (vector_node_t*)obj;
	for (i = 0; i < (int)node->len; i++) {
		child = get_gc_object(&node->data[i]);
		if (child) add_to_gray(s, child);
	}
}

static void gray_function(su_state *s, gc_t *obj) {
	int i;
	gc_t *child;
	function_t *func = (function_t*)obj;
	if (func->prot->gc.type != SU_INV)
		add_to_gray(s, &func->prot->gc);
	for (i = 0; i < (int)func->num_const; i++) {
		child = get_gc_object(&func->constants[i]);
		if (child) add_to_gray(s, child);
	}
	for (i = 0; i < (int)func->num_ups; i++) {
		child = get_gc_object(&func->upvalues[i]);
		if (child) add_to_gray(s, child);
	}
}

static void free_prot(su_state *s, prototype_t *prot) {
	int i;
	su_allocate(s, prot->inst, 0);
	su_allocate(s, prot->lineinf, 0);
	su_allocate(s, prot->upvalues, 0);
	
	for (i = 0; i < prot->num_const; i++) {
		if (prot->constants[i].id == CSTRING)
			su_allocate(s, prot->constants[i].obj.str, 0);
	}
	su_allocate(s, prot->constants, 0);
	
	for (i = 0; i < prot->num_prot; i++)
		free_prot(s, &prot->prot[i]);
	su_allocate(s, prot->prot, 0);
}

static void free_object(su_state *s, gc_t *obj) {
	function_t *func;
	if (obj->type == SU_FUNCTION) {
		func = (function_t*)obj;
		su_allocate(s, func->constants, 0);
		su_allocate(s, func->upvalues, 0);
	} else if (obj->type == PROTOTYPE) {
		free_prot(s, (prototype_t*)obj);
	}
	su_allocate(s, obj, 0);
}

static void begin(su_state *s) {
	int i;
	gc_t *gcv;
	s->gc_gray_size = 0;
	for (i = 0; i < s->stack_top; i++) {
		gcv = get_gc_object(&s->stack[i]);
		if (gcv) add_to_gray(s, gcv);
	}
	if (s->globals.type != SU_NIL) {
		add_to_gray(s, s->globals.obj.gc_object);
		add_to_gray(s, s->strings.obj.gc_object);
	}
}

static void scan(su_state *s) {
	gc_t *obj;
	gc_t *child;
	while (s->gc_gray_size) {
		obj = s->gc_gray[--s->gc_gray_size];
		if(obj->flags == GC_FLAG_BLACK)
			continue;
			
		add_to_black(obj);
		switch (obj->type) {
			case SU_LOCAL:
				child = get_gc_object(&((local_t*)obj)->v);
				if (child) add_to_gray(s, child);
				break;
			case SU_VECTOR:
				gray_vector(s, obj);
				break;
			case VECTOR_NODE:
				gray_vector_node(s, obj);
				break;
			case SU_FUNCTION:
				gray_function(s, obj);
				break;
			case SU_MAP:
				add_to_gray(s, &((map_t*)obj)->root->gc);
				break;
			case MAP_COLLISION:
				add_to_gray(s, &((node_collision_t*)obj)->leaves->gc);
				break;
			case MAP_FULL:
				add_to_gray(s, &((node_full_t*)obj)->nodes->gc);
				break;
			case MAP_IDX:
				add_to_gray(s, &((node_idx_t*)obj)->nodes->gc);
				break;
			case MAP_LEAF:
				child = get_gc_object(&((node_leaf_t*)obj)->key);
				if (child) add_to_gray(s, child);
				child = get_gc_object(&((node_leaf_t*)obj)->val);
				if (child) add_to_gray(s, child);
				break;
			case CELL_SEQ:
				child = get_gc_object(&((cell_seq_t*)obj)->first);
				if (child) add_to_gray(s, child);
				child = get_gc_object(&((cell_seq_t*)obj)->rest);
				if (child) add_to_gray(s, child);
				break;
			case IT_SEQ:
				add_to_gray(s, ((it_seq_t*)obj)->obj);
				break;
		}
	}
}

static void end(su_state *s) {
	gc_t *tmp;
	gc_t *prev = NULL;
	gc_t *obj = s->gc_root;

	while (obj) {
		if (obj->flags == GC_FLAG_WHITE) {
			if (prev)
				prev->next = obj->next;
			else
				s->gc_root = obj->next;
			tmp = obj;
			obj = obj->next;
			free_object(s, tmp);
			s->num_objects--;
		} else {
			obj->flags = GC_FLAG_WHITE;
			prev = obj;
			obj = obj->next;
		}
	}
}

void gc_trace(su_state *s) {
	if (s->num_objects > ALIVE_OBJECTS) {
		if (s->gc_gray_size == 0)
			begin(s);
		scan(s);
		if (s->gc_gray_size == 0)
			end(s);
	}
}

void su_gc(su_state *s) {
	while (s->gc_gray_size > 0)
		gc_trace(s);
	begin(s);
	while (s->gc_gray_size > 0)
		scan(s);
	end(s);
}
