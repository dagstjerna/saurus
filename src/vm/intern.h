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

#ifndef _INTERN_H_
#define _INTERN_H_

#include "saurus.h"
#include "platform.h"

#include <stdio.h>
#include <setjmp.h>

#define MAX_CALLS 128
#define STACK_SIZE 512
#define SCRATCH_PAD_SIZE 512
#define GC_GRAY_SIZE 512

#define STK(n) (&s->stack[s->stack_top + (n)])
#define FRAME() (&s->frames[s->frame_top - 1])

#define xstr(s) str(s)
#define str(s) #s

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_PATCH 1
#define VERSION_STRING xstr(VERSION_MAJOR) "." xstr(VERSION_MINOR) "." xstr(VERSION_PATCH)

typedef struct gc gc_t;
typedef struct value value_t;
typedef struct function function_t;
typedef struct prototype prototype_t;
typedef struct instruction instruction_t;
typedef struct upvalue upvalue_t;

typedef struct local local_t;
typedef struct seq seq_t;

typedef struct vector vector_t;
typedef struct vector_node vector_node_t;
typedef struct reader_buffer reader_buffer_t;

typedef struct map map_t;
typedef struct node node_t;
typedef struct node_leaf node_leaf_t;
typedef struct node_full node_full_t;
typedef struct node_idx node_idx_t;
typedef struct node_collision node_collision_t;

typedef void (*thread_entry_t)(su_state*);

enum {
	PROTOTYPE = SU_NUM_OBJECT_TYPES,
	VECTOR_NODE,
	MAP_LEAF,
	MAP_EMPTY,
	MAP_FULL,
	MAP_IDX,
	MAP_COLLISION,
	GLOBAL_INTERNAL,
	CELL_SEQ,
	IT_SEQ
};

enum {
	CNIL,
	CFALSE,
	CTRUE,
	CNUMBER,
	CSTRING
};

enum {
	IGC = 0x1
};

struct gc {
	gc_t *next;
	unsigned char type;
	unsigned char flags;
};

typedef struct {
	unsigned size;
	char str[1];
} const_string_t;

typedef struct {
	gc_t gc;
	unsigned hash;
	unsigned size;
	char str[1];
} string_t;

typedef struct {
	unsigned char id;
	union {
		const_string_t *str;
		double num;
	} obj;
} const_t;

typedef struct {
	gc_t gc;
	su_data_class_t *vt;
	unsigned char data[1];
} native_data_t;

struct value {
	union {
		int b;
		double num;
		function_t *func;
		su_nativefunc nfunc;
		gc_t *gc_object;
		string_t *str;
		vector_t *vec;
		vector_node_t *vec_node;
		seq_t *q;
		map_t *m;
		node_t *map_node;
		local_t *loc;
		native_data_t *data;
		void *ptr;
		unsigned char value_data[SU_VALUE_DATA_SIZE];
	} obj;
	unsigned char type;
};

typedef struct {
	function_t *func;
	int stack_top;
	int ret_addr;
} frame_t;

struct prototype {
	gc_t gc;
	
	unsigned num_inst;
	instruction_t *inst;
	unsigned num_const;
	const_t *constants;
	unsigned num_ups;
	upvalue_t *upvalues;
	unsigned num_prot;
	prototype_t *prot;
	
	const_string_t *name;
	unsigned num_lineinf;
	unsigned *lineinf;
};

struct function {
	gc_t gc;
	int narg;
	prototype_t *prot;
	unsigned num_const;
	value_t *constants;
	unsigned num_ups;
	value_t *upvalues;
};

struct state {
	su_alloc alloc;
	
	gc_t *gc_root;
	gc_t *gc_locals;
	gc_t *gc_gray[GC_GRAY_SIZE];
	unsigned gc_gray_size;
	unsigned num_objects;

	value_t globals;
	value_t strings;
	
	char scratch_pad[SCRATCH_PAD_SIZE];
	FILE *fstdin, *fstdout, *fstderr;
	
	char *reader_pad;
	int reader_pad_size;
	int reader_pad_top;
	
	jmp_buf err;
	int errtop;
	
	frame_t *frame;
	prototype_t *prot;
	int pc, narg;
	unsigned interupt;
	
	int frame_top;
	frame_t frames[MAX_CALLS];
	
	int stack_top;
	value_t stack[STACK_SIZE];
};

unsigned hash_value(value_t *v);
void push_value(su_state *s, value_t *v);
int value_eq(value_t *a, value_t *b);
int read_prototype(su_state *s, reader_buffer_t *buffer, prototype_t *prot);
gc_t *gc_insert_object(su_state *s, gc_t *obj, su_object_type_t type);
gc_t *string_from_db(su_state *s, unsigned hash, unsigned size, const char *str);
unsigned murmur(const void *key, int len, unsigned seed);

#endif
