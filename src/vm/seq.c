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

/* The vector and hashmap implementation is based on Rich Hickey's solution for Clojure. */

#include "seq.h"
#include "intern.h"

#include <string.h>
#include <assert.h>

static value_t cell_first(su_state *s, seq_t *q) {
	return ((cell_seq_t*)q)->first;
}

static value_t cell_rest(su_state *s, seq_t *q) {
	return ((cell_seq_t*)q)->rest;
}

value_t cell_create_array(su_state *s, value_t *array, int num) {
	int i;
	value_t tmp;
	cell_seq_t *cell = (cell_seq_t*)allocate(s, NULL, sizeof(cell_seq_t) * num);
	tmp.type = SU_NIL;
	
	for (i = num - 1; i >= 0; i--) {
		cell->first = array[i];
		cell->rest = tmp;
		cell->q.first = &cell_first;
		cell->q.rest = &cell_rest;
		tmp.type = CELL_SEQ;
		tmp.obj.gc_object = gc_insert_object(s, &cell->q.gc, CELL_SEQ);
		cell++;
	}
	
	return tmp;
}

value_t cell_create(su_state *s, value_t *first, value_t *rest) {
	value_t v;
	cell_seq_t *cell = (cell_seq_t*)allocate(s, NULL, sizeof(cell_seq_t));
	cell->first = *first;
	cell->rest = *rest;
	cell->q.first = &cell_first;
	cell->q.rest = &cell_rest;
	
	v.type = CELL_SEQ;
	v.obj.gc_object = gc_insert_object(s, &cell->q.gc, CELL_SEQ);
	return v;
}

static value_t it_next(su_state *s, it_seq_t *iq) {
	value_t v;
	it_seq_t *it = (it_seq_t*)allocate(s, NULL, sizeof(it_seq_t));
	it->idx = iq->idx + 1;
	it->obj = iq->obj;
	it->q.first = iq->q.first;
	it->q.rest = iq->q.rest;
	
	v.type = IT_SEQ;
	v.obj.gc_object = gc_insert_object(s, &it->q.gc, IT_SEQ);
	return v;
}

static value_t it_string_first(su_state *s, seq_t *q) {
	value_t v;
	char buffer[2] = {0, 0};
	it_seq_t *iq = (it_seq_t*)q;
	buffer[0] = ((string_t*)iq->obj)->str[iq->idx];
	
	v.type = SU_STRING;
	v.obj.gc_object = string_from_db(s, murmur(buffer, 2, 0), 2, buffer);
	return v;
}

static value_t it_string_rest(su_state *s, seq_t *q) {
	value_t v;
	it_seq_t *iq = (it_seq_t*)q;
	
	if (iq->idx + 2 == ((string_t*)iq->obj)->size) {
		v.type = SU_NIL;
		return v;
	}
	
	return it_next(s, iq);
}

static value_t it_vector_first(su_state *s, seq_t *q) {
	it_seq_t *iq = (it_seq_t*)q;
	return vector_index(s, (vector_t*)iq->obj, iq->idx);
}

static value_t it_vector_rest(su_state *s, seq_t *q) {
	value_t v;
	it_seq_t *iq = (it_seq_t*)q;
	
	if (iq->idx + 1 == ((vector_t*)iq->obj)->cnt) {
		v.type = SU_NIL;
		return v;
	}
	
	return it_next(s, iq);
}

value_t it_create_vector(su_state *s, vector_t *vec) {
	value_t v;
	it_seq_t *it;
	
	if (vec->cnt == 0) {
		v.type = SU_NIL;
		return v;
	}
	
	it = (it_seq_t*)allocate(s, NULL, sizeof(it_seq_t));
	it->idx = 0;
	it->obj = (gc_t*)vec;
	it->q.first = &it_vector_first;
	it->q.rest = &it_vector_rest;
	
	v.type = IT_SEQ;
	v.obj.gc_object = gc_insert_object(s, &it->q.gc, IT_SEQ);
	return v;
}

value_t it_create_string(su_state *s, string_t *str) {
	value_t v;
	it_seq_t *it;
	
	if (str->size <= 1) {
		v.type = SU_NIL;
		return v;
	}
	
	it = (it_seq_t*)allocate(s, NULL, sizeof(it_seq_t));
	it->idx = 0;
	it->obj = (gc_t*)str;
	it->q.first = &it_string_first;
	it->q.rest = &it_string_rest;
	
	v.type = IT_SEQ;
	v.obj.gc_object = gc_insert_object(s, &it->q.gc, IT_SEQ);
	return v;
}

value_t seq_first(su_state *s, seq_t *q) {
	return q->first(s, q);
}

value_t seq_rest(su_state *s, seq_t *q) {
	return q->rest(s, q);
}

/* --------------------------------- Vector implementation --------------------------------- */

#define tailoff(v) ((v)->cnt - (v)->tail->len)

static vector_node_t *push_tail(su_state *s, int level, vector_node_t *arr, vector_node_t *tail_node, vector_node_t **expansion);
static vector_node_t *insert(su_state *s, int level, vector_node_t *arr, int i, value_t *val);
static vector_node_t *pop_tail(su_state *s, int shift, vector_node_t *arr, vector_node_t **ptail);

static vector_node_t *node_create_only(su_state *s, int len) {
	vector_node_t *node = (vector_node_t*)allocate(s, NULL, (sizeof(vector_node_t) + sizeof(value_t) * len) - sizeof(value_t));
	node->len = (unsigned char)len;
	gc_insert_object(s, (gc_t*)node, VECTOR_NODE);
	return node;
}

static vector_node_t *node_create1(su_state *s, value_t *v) {
	vector_node_t *node = node_create_only(s, 1);
	node->data[0] = *v;
	return node;
}

static vector_node_t *node_create2(su_state *s, value_t *v1, value_t *v2) {
	vector_node_t *node = node_create_only(s, 2);
	node->data[0] = *v1;
	node->data[1] = *v2;
	return node;
}

static vector_node_t *node_clone(su_state *s, vector_node_t *src) {
	vector_node_t *node = node_create_only(s, src->len);
	memcpy(node->data, src->data, sizeof(value_t) * src->len);
	return node;
}

int vector_length(vector_t *v) {
	return v->cnt;
}

value_t vector_index(su_state *s, vector_t *v, int i) {
	int level;
	vector_node_t *arr;
	if (i >= 0 && i < v->cnt) {
		if (i >= tailoff(v))
			return v->tail->data[i & 0x01f];
		arr = v->root;
		for (level = v->shift; level > 0; level -= 5)
			arr = arr->data[(i >> level) & 0x01f].obj.vec_node;
		return arr->data[i & 0x01f];
	}
	su_error(s, "Index is out of bounds: %i", i);
	return *v->root->data;
}

value_t vector_create(su_state *s, unsigned cnt, int shift, vector_node_t *root, vector_node_t *tail) {
	vector_t *vec;
	value_t v;
	v.type = SU_VECTOR;
	v.obj.gc_object = (gc_t*)allocate(s, NULL, sizeof(vector_t));
	gc_insert_object(s, v.obj.gc_object, SU_VECTOR);
	
	assert(root);
	assert(tail);
	
	vec = (vector_t*)v.obj.gc_object;
	vec->cnt = cnt;
	vec->shift = shift;
	vec->root = root;
	vec->tail = tail;
	return v;
}

value_t vector_create_empty(su_state *s) {
	return vector_create(s, 0, 5, node_create_only(s, 0), node_create_only(s, 0));
}

value_t vector_push(su_state *s, vector_t *vec, value_t *val) {
	value_t expansion_value, tmp;
	vector_node_t *expansion = NULL, *new_root;
	int new_shift = vec->shift;
	
	if (vec->tail->len < 32) {
		vector_node_t *new_tail = node_create_only(s, vec->tail->len + 1);
		memcpy(new_tail->data, vec->tail->data, sizeof(value_t) * vec->tail->len);
		new_tail->data[vec->tail->len] = *val;
		return vector_create(s, vec->cnt + 1, vec->shift, vec->root, new_tail);
	}
	
	new_root = push_tail(s, vec->shift - 5, vec->root, vec->tail, &expansion);
	if (expansion) {
		expansion_value.type = tmp.type = VECTOR_NODE;
		expansion_value.obj.vec_node = expansion;
		tmp.obj.vec_node = new_root;
		new_root = node_create2(s, &tmp, &expansion_value);
		new_shift += 5;
	}
	
	return vector_create(s, vec->cnt + 1, new_shift, new_root, node_create1(s, val));
}

static vector_node_t *push_tail(su_state *s, int level, vector_node_t *arr, vector_node_t *tail_node, vector_node_t **expansion) {
	value_t tmp;
	vector_node_t *new_child, *ret;
	
	if (level == 0) {
		new_child = tail_node;
	} else {
		new_child = push_tail(s, level - 5, arr->data[arr->len - 1].obj.vec_node, tail_node, expansion);
		if (*expansion == NULL) {
			ret = node_clone(s, arr);
			tmp.type = VECTOR_NODE;
			tmp.obj.vec_node = new_child;
			ret->data[arr->len - 1] = tmp;
			return ret;
		} else {
			new_child = *expansion;
		}
	}
	
	/* Do expansion */
	
	tmp.type = VECTOR_NODE;
	tmp.obj.vec_node = new_child;
	
	if (arr->len == 32) {
		*expansion = node_create1(s, &tmp);
		return arr;
	}
	
	ret = node_create_only(s, arr->len + 1);
	memcpy(ret->data, arr->data, sizeof(value_t) * arr->len);
	ret->data[arr->len] = tmp;
	*expansion = NULL;
	return ret;
}

value_t vector_set(su_state *s, vector_t *vec, int i, value_t *val) {
	vector_node_t *new_tail;
	if (i >= 0 && i < vec->cnt) {
		if (i >= tailoff(vec)) {
			new_tail = node_create_only(s, vec->tail->len);
			memcpy(new_tail->data, vec->tail->data, sizeof(value_t) * vec->tail->len);
			new_tail->data[i & 0x01f] = *val;
			return vector_create(s, vec->cnt, vec->shift, vec->root, new_tail);
		}
		return vector_create(s, vec->cnt, vec->shift, insert(s, vec->shift, vec->root, i, val), vec->tail);
	}
	su_error(s, "Index is out of bounds: %i", i);
	return *val;
}

static vector_node_t *insert(su_state *s, int level, vector_node_t *arr, int i, value_t *val) {
	int subidx;
	value_t tmp;
	vector_node_t *ret = node_clone(s, arr);
	if (level == 0) {
		ret->data[i & 0x01f] = *val;
	} else {
		subidx = (i >> level) & 0x01f;
		tmp.type = VECTOR_NODE;
		tmp.obj.vec_node = insert(s, level - 5, arr->data[subidx].obj.vec_node, i, val);
		ret->data[subidx] = tmp;
	}
	return ret;
}

value_t vector_pop(su_state *s, vector_t *vec) {
	vector_node_t *new_tail, *new_root;
	vector_node_t *ptail = NULL;
	int new_shift = vec->shift;
	
	if (vec->cnt == 0)
		su_error(s, "Can't pop empty vector!");
	if (vec->cnt == 1)
		return vector_create_empty(s);
	
	if (vec->tail->len > 1) {
		new_tail = node_create_only(s, vec->tail->len - 1);
		memcpy(new_tail->data, vec->tail->data, sizeof(value_t) * new_tail->len);
		return vector_create(s, vec->cnt - 1, vec->shift, vec->root, new_tail);
	}

	new_root = pop_tail(s, vec->shift - 5, vec->root, &ptail);
	if (!new_root)
		new_root = node_create_only(s, 0);

	if (vec->shift > 5 && new_root->len == 1) {
		new_root = new_root->data[0].obj.vec_node;
		new_shift -= 5;
	}

	return vector_create(s, vec->cnt - 1, new_shift, new_root, ptail);
}

static vector_node_t *pop_tail(su_state *s, int shift, vector_node_t *arr, vector_node_t **ptail) {
	vector_node_t *new_child, *ret;
	value_t tmp;
	
	if (shift > 0) {
		new_child = pop_tail(s, shift - 5, arr->data[arr->len - 1].obj.vec_node, ptail);
		if (new_child != NULL) {
			tmp.type = VECTOR_NODE;
			tmp.obj.vec_node = new_child;
			ret = node_clone(s, arr);
			ret->data[arr->len - 1] = tmp;
			return ret;
		}
	}
	
	if (shift == 0)
		*ptail = arr->data[arr->len - 1].obj.vec_node;
	
	/* Contraction */
	
	if (arr->len == 1)
		return NULL;
	
	ret = node_create_only(s, arr->len - 1);
	memcpy(ret->data, arr->data, sizeof(value_t) * ret->len);
	return ret;
}

/* --------------------------------- HashMap implementation --------------------------------- */

#define CAST_AND_TEST(t, f) t *thiz = (t*)n; assert(n->gc.type == (f))
#define xmemcpy(dest, doff, src, soff, num) memcpy((void*)(((char*)(dest))+(doff)),(void*)(((char*)(src))+(soff)),(num))

#define MASK(h, s) (((h) >> (s)) & 0x01f)
#define BITPOS(h, s) (1 << MASK((h), (s)))

static node_t *create_leaf_node(su_state *s, int hash, value_t *key, value_t *val);
static node_t *create_collision_node(su_state *s, int hash, vector_node_t *leaves);
static node_t *create_full_node(su_state *s, vector_node_t *nodes, int shift);
static node_t *create_idx_node(su_state *s, int bitmap, vector_node_t *nodes, int shift);
static node_t *create_idx_node2(su_state *s, int bitmap, vector_node_t *nodes, int shift);
static node_t *create_idx_node3(su_state *s, int shift, node_t *branch, int hash, value_t *key, value_t *val, node_t **added_leaf);

static int bit_count(int x)
{
	#ifdef __GNUC__
		return __builtin_popcount(x);
	#else
		x = (x & 0x55555555) + ((x >> 1) & 0x55555555);
		x = (x & 0x33333333) + ((x >> 2) & 0x33333333);
		x = (x & 0x0f0f0f0f) + ((x >> 4) & 0x0f0f0f0f);
		x = (x & 0x00ff00ff) + ((x >> 8) & 0x00ff00ff);
		x = (x & 0x0000ffff) + ((x >> 16)& 0x0000ffff);
		return x;
	#endif
}

/* Full node */

static node_t *full_node_set(su_state *s, node_t *n, int shift, int hash, value_t *key, value_t *val, node_t **added_leaf) {
	int idx;
	node_t *tmp;
	value_t v;
	vector_node_t *new_nodes;
	CAST_AND_TEST(node_full_t, MAP_FULL);
	
	idx = MASK(hash, shift);
	tmp = thiz->nodes->data[idx].obj.map_node;
	tmp = tmp->set(s, tmp, shift + 5, hash, key, val, added_leaf);
	if (tmp == thiz->nodes->data[idx].obj.map_node) {
		return n;
	} else {
		new_nodes = node_clone(s, thiz->nodes);
		v.type = tmp->gc.type;
		v.obj.map_node = tmp;
		new_nodes->data[idx] = v;
		return create_full_node(s, new_nodes, shift);
	}
}

static node_t *full_node_without(su_state *s, node_t *n, int hash, value_t *key) {
	int idx;
	value_t v;
	node_t *tmp;
	vector_node_t *new_nodes;
	CAST_AND_TEST(node_full_t, MAP_FULL);
	
	idx = MASK(hash, thiz->shift);
	tmp = thiz->nodes->data[idx].obj.map_node;
	tmp = tmp->without(s, tmp, hash, key);
	if (tmp != thiz->nodes->data[idx].obj.map_node) {
		if (!tmp) {
			new_nodes = node_create_only(s, thiz->nodes->len - 1);
			memcpy(new_nodes->data, thiz->nodes->data, sizeof(value_t) * idx);
			xmemcpy(new_nodes->data, sizeof(value_t) * idx, thiz->nodes->data, sizeof(value_t) * (idx + 1), sizeof(value_t) * (thiz->nodes->len - (idx + 1)));
			return create_idx_node(s, ~BITPOS(hash, thiz->shift), new_nodes, thiz->shift);
		}
		
		new_nodes = node_clone(s, thiz->nodes);
		v.type = tmp->gc.type;
		v.obj.map_node = tmp;
		new_nodes->data[idx] = v;
		return create_full_node(s, new_nodes, thiz->shift);
	}
	return n;
}

static node_leaf_t *full_node_find(su_state *s, node_t *n, int hash, value_t *key) {
	node_t *tmp;
	CAST_AND_TEST(node_full_t, MAP_FULL);
	tmp = thiz->nodes->data[MASK(hash, thiz->shift)].obj.map_node;
	return tmp->find(s, tmp, hash, key);
}

static int full_node_get_hash(su_state *s, node_t *n) {
	CAST_AND_TEST(node_full_t, MAP_FULL);
	return thiz->hash;
}

static node_t *create_full_node(su_state *s, vector_node_t *nodes, int shift) {
	node_t *tmp, *n;
	node_full_t *fn = (node_full_t*)allocate(s, NULL, sizeof(node_full_t));
	fn->nodes = nodes;
	fn->shift = shift;
	tmp = nodes->data[0].obj.map_node;
	fn->hash = tmp->get_hash(s, tmp);
	
	n = (node_t*)fn;
	n->set = &full_node_set;
	n->without = &full_node_without;
	n->find = &full_node_find;
	n->get_hash = &full_node_get_hash;
	return (node_t*)gc_insert_object(s, (gc_t*)n, MAP_FULL);
}

/* Index node */

static int idx_node_index(node_t *n, int bit) {
	CAST_AND_TEST(node_idx_t, MAP_IDX);
	return bit_count(thiz->bitmap & (bit - 1));
}

static node_t *idx_node_set(su_state *s, node_t *n, int shift, int hash, value_t *key, value_t *val, node_t **added_leaf) {
	int bit, idx;
	node_t *tmp;
	value_t v;
	vector_node_t *new_nodes;
	CAST_AND_TEST(node_idx_t, MAP_IDX);
	bit = BITPOS(hash, shift);
	idx = idx_node_index(n, bit);
	
	if ((thiz->bitmap & bit) != 0) {
		tmp = thiz->nodes->data[idx].obj.map_node;
		tmp = tmp->set(s, tmp, shift + 5, hash, key, val, added_leaf);
		if (tmp == thiz->nodes->data[idx].obj.map_node) {
			return n;
		} else {
			new_nodes = node_clone(s, thiz->nodes);
			v.type = VECTOR_NODE;
			v.obj.map_node = tmp;
			new_nodes->data[idx] = v;
			return create_idx_node(s, thiz->bitmap, new_nodes, shift);
		}
	} else {
		new_nodes = node_create_only(s, thiz->nodes->len + 1);
		memcpy(new_nodes->data, thiz->nodes->data, sizeof(value_t) * idx);
		*added_leaf = new_nodes->data[idx].obj.map_node = create_leaf_node(s, hash, key, val);
		new_nodes->data[idx].type = (*added_leaf)->gc.type;
		xmemcpy(new_nodes->data, sizeof(value_t) * (idx + 1), thiz->nodes->data, sizeof(value_t) * idx, sizeof(value_t) * (thiz->nodes->len - idx));
		return create_idx_node2(s, thiz->bitmap | bit, new_nodes, shift);
	}
}

static node_t *idx_node_without(su_state *s, node_t *n, int hash, value_t *key) {
	int bit, idx;
	node_t *tmp;
	value_t v;
	vector_node_t *new_nodes;
	CAST_AND_TEST(node_idx_t, MAP_IDX);
	bit = BITPOS(hash, thiz->shift);
	
	if ((thiz->bitmap & bit) != 0) {
		idx = idx_node_index(n, bit);
		tmp = thiz->nodes->data[idx].obj.map_node;
		tmp = tmp->without(s, tmp, hash, key);
		if (tmp != thiz->nodes->data[idx].obj.map_node) {
			if (!tmp) {
				if (thiz->bitmap == bit)
					return NULL;

				new_nodes = node_create_only(s, thiz->nodes->len - 1);
				memcpy(new_nodes->data, thiz->nodes->data, sizeof(value_t) * idx);
				xmemcpy(new_nodes->data, sizeof(value_t) * idx, thiz->nodes->data, sizeof(value_t) * (idx + 1), sizeof(value_t) * (thiz->nodes->len - (idx + 1)));
				return create_idx_node(s, thiz->bitmap & ~bit, new_nodes, thiz->shift);
			}
			new_nodes = node_clone(s, thiz->nodes);
			v.type = tmp->gc.type;
			v.obj.map_node = tmp;
			new_nodes->data[idx] = v;
			return create_idx_node(s, thiz->bitmap, new_nodes, thiz->shift);
		}
	}
	return n;
}

static node_leaf_t *idx_node_find(su_state *s, node_t *n, int hash, value_t *key) {
	int bit;
	node_t *tmp;
	CAST_AND_TEST(node_idx_t, MAP_IDX);
	bit = BITPOS(hash, thiz->shift);
	if ((thiz->bitmap & bit) != 0) {
		tmp = thiz->nodes->data[idx_node_index(n, bit)].obj.map_node;
		return tmp->find(s, tmp, hash, key);
	} else {
		return NULL;
	}
}

static int idx_node_get_hash(su_state *s, node_t *n) {
	CAST_AND_TEST(node_idx_t, MAP_IDX);
	return thiz->hash;
}

static node_t *create_idx_node(su_state *s, int bitmap, vector_node_t *nodes, int shift) {
	node_t *n;
	node_t *tmp;
	node_idx_t *in = (node_idx_t*)allocate(s, NULL, sizeof(node_idx_t));
	in->bitmap = bitmap;
	in->shift = shift;
	in->nodes = nodes;
	tmp = nodes->data[0].obj.map_node;
	in->hash = tmp->get_hash(s, tmp);
	
	n = (node_t*)in;
	n->set = &idx_node_set;
	n->without = &idx_node_without;
	n->find = &idx_node_find;
	n->get_hash = &idx_node_get_hash;
	return (node_t*)gc_insert_object(s, (gc_t*)n, MAP_IDX);
}

static node_t *create_idx_node2(su_state *s, int bitmap, vector_node_t *nodes, int shift) {
	if (bitmap == -1)
		return create_full_node(s, nodes, shift);
	return create_idx_node(s, bitmap, nodes, shift);
}

static node_t *create_idx_node3(su_state *s, int shift, node_t *branch, int hash, value_t *key, value_t *val, node_t **added_leaf) {
	value_t v;
	vector_node_t *vec;
	node_t *n;
	
	v.type = branch->gc.type;
	v.obj.map_node = branch;
	vec = node_create1(s, &v);
	
	n = create_idx_node(s, BITPOS(branch->get_hash(s, branch), shift), vec, shift);
	return n->set(s, n, shift, hash, key, val, added_leaf);
}

/* Collision node */

static int collision_node_find_index(su_state *s, node_t *n, int hash, value_t *key) {
	int i;
	node_t *tmp;
	CAST_AND_TEST(node_collision_t, MAP_COLLISION);
	for (i = 0; i < thiz->leaves->len; i++) {
		tmp = thiz->leaves->data[i].obj.map_node;
		if (tmp->find(s, tmp, hash, key))
			return i;
	}
	return -1;
}

static node_t *collision_node_set(su_state *s, node_t *n, int shift, int hash, value_t *key, value_t *val, node_t **added_leaf) {
	int idx;
	value_t v;
	vector_node_t *new_leaves;
	CAST_AND_TEST(node_collision_t, MAP_COLLISION);
	
	if (hash == thiz->hash) {
		idx = collision_node_find_index(s, n, hash, key);
		if (idx != -1) {
			if (value_eq(&thiz->leaves->data[idx], val))
				return n;
			
			new_leaves = node_clone(s, thiz->leaves);
			v.type = MAP_LEAF;
			v.obj.map_node = (node_t*)create_leaf_node(s, hash, key, val);
			new_leaves->data[idx] = v;
			return create_collision_node(s, hash, new_leaves);
		}
		
		new_leaves = node_create_only(s, thiz->leaves->len + 1);
		memcpy(new_leaves->data, thiz->leaves->data, sizeof(value_t) * thiz->leaves->len);
		
		*added_leaf = (node_t*)create_leaf_node(s, hash, key, val);
		v.obj.map_node = *added_leaf;
		v.type = v.obj.map_node->gc.type;
		new_leaves->data[thiz->leaves->len] = v;
		return create_collision_node(s, hash, new_leaves);
	}
	return create_idx_node3(s, shift, n, hash, key, val, added_leaf);
}

static node_t *collision_node_without(su_state *s, node_t *n, int hash, value_t *key) {
	int idx;
	vector_node_t *new_leaves;
	CAST_AND_TEST(node_collision_t, MAP_COLLISION);
	idx = collision_node_find_index(s, n, hash, key);
	if (idx == -1)
		return n;
	if (thiz->leaves->len == 2)
		return idx == 0 ? thiz->leaves->data[1].obj.map_node : thiz->leaves->data[0].obj.map_node;
	
	new_leaves = node_create_only(s, thiz->leaves->len - 1);
	memcpy(new_leaves->data, thiz->leaves->data, sizeof(value_t) * idx);
	xmemcpy(new_leaves->data, sizeof(value_t) * idx, thiz->leaves->data, sizeof(value_t) * (idx + 1), sizeof(value_t) * (thiz->leaves->len - (idx + 1)));
	return create_collision_node(s, hash, new_leaves);
}

static node_leaf_t *collision_node_find(su_state *s, node_t *n, int hash, value_t *key) {
	int idx;
	node_leaf_t *tmp;
	CAST_AND_TEST(node_collision_t, MAP_COLLISION);
	idx = collision_node_find_index(s, n, hash, key);
	if (idx != -1) {
		tmp = (node_leaf_t*)thiz->leaves->data[idx].obj.map_node;
		assert(tmp->n.gc.type == MAP_LEAF);
		return tmp;
	}
	return NULL;
}

static int collision_node_get_hash(su_state *s, node_t *n) {
	CAST_AND_TEST(node_collision_t, MAP_COLLISION);
	return thiz->hash;
}

static node_t *create_collision_node(su_state *s, int hash, vector_node_t *leaves) {
	node_t *n;
	node_collision_t *cn = (node_collision_t*)allocate(s, NULL, sizeof(node_collision_t));
	cn->hash = hash;
	cn->leaves = leaves;
	
	n = (node_t*)cn;
	n->set = &collision_node_set;
	n->without = &collision_node_without;
	n->find = &collision_node_find;
	n->get_hash = &collision_node_get_hash;
	return (node_t*)gc_insert_object(s, (gc_t*)n, MAP_COLLISION);
}

/* Leaf node */

static node_t *leaf_node_set(su_state *s, node_t *n, int shift, int hash, value_t *key, value_t *val, node_t **added_leaf) {
	value_t v, t;
	CAST_AND_TEST(node_leaf_t, MAP_LEAF);
	
	if (hash == thiz->hash) {
		if (value_eq(key, &thiz->key)) {
			if (value_eq(val, &thiz->val))
				return n;
			return create_leaf_node(s, hash, key, val);
		}
		
		*added_leaf = create_leaf_node(s, hash, key, val);
		v.type = (*added_leaf)->gc.type;
		v.obj.map_node = *added_leaf;
		t.type = n->gc.type;
		t.obj.map_node = n;
		
		return create_collision_node(s, hash, node_create2(s, &t, &v));
	}
	return create_idx_node3(s, shift, n, hash, key, val, added_leaf);
}

static node_t *leaf_node_without(su_state *s, node_t *n, int hash, value_t *key) {
	CAST_AND_TEST(node_leaf_t, MAP_LEAF);
	if (hash == thiz->hash && value_eq(key, &thiz->key))
		return NULL;
	return n;
}

static node_leaf_t *leaf_node_find(su_state *s, node_t *n, int hash, value_t *key) {
	CAST_AND_TEST(node_leaf_t, MAP_LEAF);
	if(hash == thiz->hash && value_eq(key, &thiz->key))
		return thiz;
	return NULL;
}

static int leaf_node_get_hash(su_state *s, node_t *n) {
	CAST_AND_TEST(node_leaf_t, MAP_LEAF);
	return thiz->hash;
}

static node_t *create_leaf_node(su_state *s, int hash, value_t *key, value_t *val) {
	node_t *n;
	node_leaf_t *ln = (node_leaf_t*)allocate(s, NULL, sizeof(node_leaf_t));
	ln->hash = hash;
	ln->key = *key;
	ln->val = *val;
	
	n = (node_t*)ln;
	n->set = &leaf_node_set;
	n->without = &leaf_node_without;
	n->find = &leaf_node_find;
	n->get_hash = &leaf_node_get_hash;
	return (node_t*)gc_insert_object(s, (gc_t*)n, MAP_LEAF);
}

/* Empty node */

static node_t *empty_node_set(su_state *s, node_t *n, int shift, int hash, value_t *key, value_t *val, node_t **added_leaf) {
	node_t *ret = create_leaf_node(s, hash, key, val);
	CAST_AND_TEST(node_t, MAP_EMPTY);
	(void)*thiz;
	*added_leaf = ret;
	return ret;
}

static node_t *empty_node_without(su_state *s, node_t *n, int hash, value_t *key) {
	CAST_AND_TEST(node_t, MAP_EMPTY);
	(void)*thiz;
	return n;
}

static node_leaf_t *empty_node_find(su_state *s, node_t *n, int hash, value_t *key) {
	CAST_AND_TEST(node_t, MAP_EMPTY);
	(void)*thiz;
	return NULL;
}

static int empty_node_get_hash(su_state *s, node_t *n) {
	CAST_AND_TEST(node_t, MAP_EMPTY);
	(void)*thiz;
	return 0;
}

static node_t *create_empty_node(su_state *s) {
	node_t *n = (node_t*)allocate(s, NULL, sizeof(node_t));
	n->set = &empty_node_set;
	n->without = &empty_node_without;
	n->find = &empty_node_find;
	n->get_hash = &empty_node_get_hash;
	return (node_t*)gc_insert_object(s, &n->gc, MAP_EMPTY);
}

/* Map functions */

static value_t map_create(su_state *s, int cnt, node_t *root) {
	value_t v;
	map_t *m = (map_t*)allocate(s, NULL, sizeof(map_t));
	m->root = root;
	m->cnt = cnt;
	v.type = SU_MAP;
	v.obj.gc_object = gc_insert_object(s, (gc_t*)m, SU_MAP);
	return v;
}

value_t map_create_empty(su_state *s) {
	return map_create(s, 0, create_empty_node(s));
}

value_t map_get(su_state *s, map_t *m, value_t *key, unsigned hash) {
	value_t v;
	node_leaf_t *n = m->root->find(s, m->root, (int)hash, key);
	if (!n) {
		v.type = SU_INV;
		return v;
	}
	return n->val;
}

value_t map_remove(su_state *s, map_t *m, value_t *key, unsigned hash) {
	value_t v;
	node_t *new_root = m->root->without(s, m->root, (int)hash, key);
	v.type = SU_MAP;
	if (new_root == m->root) {
		v.obj.m = m;
		return v;
	}
	if (!new_root)
		return map_create_empty(s);
	return map_create(s, m->cnt - 1, new_root);
}

value_t map_insert(su_state *s, map_t *m, value_t *key, unsigned hash, value_t *val) {
	value_t v;
	node_t *added_leaf = NULL;
	node_t *new_root = m->root->set(s, m->root, 0, (int)hash, key, val, &added_leaf);
	if (new_root == m->root) {
		v.type = SU_MAP;
		v.obj.m = m;
		return v;
	}
	return map_create(s, added_leaf ? m->cnt + 1 : m->cnt, new_root);
}

int map_length(map_t *m) {
	return m->cnt;
}

/* --------------------------------- Seq implementation --------------------------------- */

static value_t it_seq_create_with_index(su_state *s, gc_t *obj, int idx);

static value_t it_seq_string_first(su_state *s, seq_t *q) {
	value_t v;
	char buffer[2] = {0, 0};
	it_seq_t *itq = (it_seq_t*)q;
	string_t *str = (string_t*)itq->obj;
	v.type = SU_STRING;
	buffer[0] = str->str[itq->idx];
	v.obj.gc_object = string_from_db(s, murmur(buffer, 2, 0), 2, buffer);
	return v;
}

static value_t it_seq_string_rest(su_state *s, seq_t *q) {
	value_t v;
	it_seq_t *itq = (it_seq_t*)q;
	v.type = SU_NIL;
	if (itq->idx + 1 >= ((string_t*)itq->obj)->size)
		return v;
	return it_seq_create_with_index(s, itq->obj, itq->idx + 1);
}

static value_t it_seq_vector_first(su_state *s, seq_t *q) {
	it_seq_t *itq = (it_seq_t*)q;
	return vector_index(s, (vector_t*)itq->obj, itq->idx);
}

static value_t it_seq_vector_rest(su_state *s, seq_t *q) {
	value_t v;
	it_seq_t *itq = (it_seq_t*)q;
	v.type = SU_NIL;
	if (itq->idx + 1 >= ((vector_t*)itq->obj)->cnt)
		return v;
	return it_seq_create_with_index(s, itq->obj, itq->idx + 1);
}
	
static value_t it_seq_create_with_index(su_state *s, gc_t *obj, int idx) {
	value_t v;
	it_seq_t *q = (it_seq_t*)allocate(s, NULL, sizeof(it_seq_t));
	v.type = IT_SEQ;
	q->obj = obj;
	q->idx = idx;
	
	if (v.type == SU_STRING) {
		q->q.first = &it_seq_string_first;
		q->q.rest = &it_seq_string_rest;
	} else {
		q->q.first = &it_seq_vector_first;
		q->q.rest = &it_seq_vector_rest;
	}
	
	v.obj.gc_object = gc_insert_object(s, &q->q.gc, IT_SEQ);
	return v;
}

value_t it_seq_create(su_state *s, value_t *obj) {
	return it_seq_create_with_index(s, obj->obj.gc_object, 0);
}
