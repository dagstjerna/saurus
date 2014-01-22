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

#ifndef _SEQ_H_
#define _SEQ_H_

#include "saurus.h"
#include "intern.h"

struct vector_node {
	gc_t gc;
	unsigned char len;
	value_t data[1];
};

struct vector {
	gc_t gc;
	int cnt;
	int shift;
	vector_node_t *root;
	vector_node_t *tail;
};

int vector_length(vector_t *v);
value_t vector_index(su_state *s, vector_t *v, int i);
value_t vector_create_empty(su_state *s);
value_t vector_push(su_state *s, vector_t *vec, value_t *val);
value_t vector_pop(su_state *s, vector_t *vec);
value_t vector_set(su_state *s, vector_t *vec, int i, value_t *val);

/***********************************************************************************/

typedef node_t* (*node_set_func_t)(su_state *s, node_t *n, int shift, int hash, value_t *key, value_t *val, node_t **added_leaf);
typedef node_t* (*node_without_func_t)(su_state *s, node_t *n, int hash, value_t *key);
typedef node_leaf_t* (*node_find_func_t)(su_state *s, node_t *n, int hash, value_t *key);
typedef int (*node_get_hash_t)(su_state *s, node_t *n);

struct node {
	gc_t gc;
	node_set_func_t set;
	node_without_func_t without;
	node_find_func_t find;
	node_get_hash_t get_hash;
};

struct node_leaf {
	node_t n;
	int hash;
	value_t key;
	value_t val;
};

struct node_collision {
	node_t n;
	int hash;
	vector_node_t *leaves;
};

struct node_idx {
	node_t n;
	int bitmap;
	int shift;
	int hash;
	vector_node_t *nodes;
};
	
struct node_full {
	node_t n;
	int shift;
	int hash;
	vector_node_t *nodes;
};

struct map {
	gc_t gc;
	int cnt;
	node_t *root;
};

value_t map_create_empty(su_state *s);
value_t map_get(su_state *s, map_t *m, value_t *key, unsigned hash);
value_t map_remove(su_state *s, map_t *m, value_t *key, unsigned hash);
value_t map_insert(su_state *s, map_t *m, value_t *key, unsigned hash, value_t *val);
int map_length(map_t *m);

/***********************************************************************************/

typedef value_t (*seq_fr_func_t)(su_state *s, seq_t *q);

struct seq {
	gc_t gc;
	seq_fr_func_t first;
	seq_fr_func_t rest;
};

typedef struct {
	seq_t q;
	int idx;
	gc_t *obj;
} it_seq_t;

typedef struct {
	seq_t q;
	value_t first, rest;
} cell_seq_t;

value_t cell_create_array(su_state *s, value_t *array, int num);
value_t cell_create(su_state *s, value_t *first, value_t *rest);

value_t it_create_vector(su_state *s, vector_t *vec);
value_t it_create_string(su_state *s, string_t *str);

value_t seq_first(su_state *s, seq_t *q);
value_t seq_rest(su_state *s, seq_t *q);

#endif
