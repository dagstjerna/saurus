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

#include "intern.h"
#include "ref.h"
#include "seq.h"
#include "gc.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <assert.h>
#include <math.h>
#include <stdarg.h>

enum {
	OP_PUSH,
	OP_POP,
	OP_COPY,

	OP_ADD,
	OP_SUB,
	OP_MUL,
	OP_DIV,
	OP_MOD,
	OP_POW,
	OP_UNM,

	OP_EQ,
	OP_LESS,
	OP_LEQUAL,

	OP_NOT,
	OP_AND,
	OP_OR,

	OP_TEST,
	OP_JMP,

	OP_RETURN,
	OP_CALL,
	OP_TCALL,
	OP_LAMBDA,

	OP_GETGLOBAL,
	OP_SETGLOBAL,
	OP_LOAD
};

struct instruction {
	unsigned char id;
	unsigned char a;
	short b;
};

struct upvalue {
	unsigned short lv;
	unsigned short idx;
};

struct reader_buffer{
	su_reader reader;
	void *data;
	char *buffer;
	size_t offset;
	size_t len;
	size_t size;
};

const char *su_version(int *major, int *minor, int *patch) {
	if (major) *major = VERSION_MAJOR;
	if (minor) *minor = VERSION_MINOR;
	if (patch) *patch = VERSION_PATCH;
	return VERSION_STRING;
}

void *su_allocate(su_state *s, void *p, size_t n) {
	void *np;
	if (n) {
		s->interupt |= IGC;
		np = s->alloc(p, n);
		su_assert(s, np != NULL, "Out of memory!");
		return np;
	} else {
		return s->alloc(p, 0);
	}
}

void push_value(su_state *s, value_t *v) {
	su_assert(s, s->stack_top < STACK_SIZE, "Stack overflow!");
	s->stack[s->stack_top++] = *v;
}

int value_eq(value_t *a, value_t *b) {
	if (a->type != b->type)
		return 0;
	if (a->type == SU_NUMBER && a->obj.num == b->obj.num)
		return 1;
	return a->obj.ptr == b->obj.ptr;
}

void su_pop(su_state *s, int n) {
	s->stack_top -= n;
	assert(s->stack_top >= 0);
}

void su_copy(su_state *s, int idx) {
	push_value(s, STK(idx));
}

void su_copy_range(su_state *s, int idx, int num) {
	memcpy(&s->stack[s->stack_top], &s->stack[s->stack_top + idx], sizeof(value_t) * num);
	s->stack_top += num;
}

static reader_buffer_t *buffer_open(su_state *s, su_reader reader, void *data) {
	reader_buffer_t *buffer = (reader_buffer_t*)su_allocate(s, NULL, sizeof(reader_buffer_t));
	buffer->reader = reader;
	buffer->data = data;
	buffer->offset = 0;
	buffer->size = 0;
	buffer->len = 0;
	buffer->buffer = NULL;
	return buffer;
}

static void buffer_close(su_state *s, reader_buffer_t *buffer) {
	buffer->reader(NULL, buffer->data);
	su_allocate(s, buffer->buffer, 0);
	su_allocate(s, buffer, 0);
}

static int buffer_read(su_state *s, reader_buffer_t *buffer, void *dest, size_t num_bytes) {
	unsigned dest_offset = 0;
	do {
		const void *res;
		size_t size = buffer->len - buffer->offset;
		size_t num = num_bytes < size ? num_bytes : size;
		memcpy(((char*)dest) + dest_offset, &buffer->buffer[buffer->offset], num);

		buffer->offset += num;
		dest_offset += num;
		num_bytes -= num;

		if (num_bytes == 0) return 0;
		res = buffer->reader(&size, buffer->data);
		if (!res || size == 0) return -1;

		while (buffer->size < size) {
			buffer->size = buffer->size * 2 + 1;
			buffer->buffer = su_allocate(s, buffer->buffer, buffer->size);
		}

		buffer->offset = 0;
		buffer->len = size;
		memcpy(buffer->buffer, res, size);
	} while (num_bytes);
	return 0;
}

static int verify_header(su_state *s, reader_buffer_t *buffer) {
	char sign[4];
	unsigned char version[2];
	unsigned short flags;

	buffer_read(s, buffer, sign, sizeof(sign));
	if (memcmp(sign, "\x1bsuc", sizeof(sign)))
		return -1;

	buffer_read(s, buffer, version, sizeof(version));
	if (version[0] != VERSION_MAJOR && version[1] != VERSION_MINOR)
		return -1;

	buffer_read(s, buffer, &flags, sizeof(flags));
	if (flags != 0)
		return -1;

	return 0;
}

unsigned murmur(const void *key, int len, unsigned seed) {
	const unsigned m = 0x5bd1e995;
	const int r = 24;
	unsigned h = seed ^ len;
	const unsigned char * data = (const unsigned char*)key;
	while (len >= 4) {
		unsigned k = *(unsigned*)data;
		k *= m;
		k ^= k >> r;
		k *= m;
		h *= m;
		h ^= k;
		data += 4;
		len -= 4;
	}
	switch (len) {
		case 3: h ^= data[2] << 16;
		case 2: h ^= data[1] << 8;
		case 1: h ^= data[0];
	        h *= m;
	};
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return h;
}

unsigned hash_value(value_t *v) {
	switch (v->type) {
		case SU_NIL:
			return (unsigned)SU_NIL;
		case SU_BOOLEAN:
			return (unsigned)v->obj.b + (unsigned)SU_BOOLEAN;
		case SU_NUMBER:
			return murmur(&v->obj.num, sizeof(double), (unsigned)SU_NUMBER);
		case SU_STRING:
			return v->obj.str->hash;
		default:
			return murmur(&v->obj.ptr, sizeof(void*), (unsigned)v->type);
	}
}

gc_t *gc_insert_object(su_state *s, gc_t *obj, su_object_type_t type) {
	obj->type = type;
	obj->flags = GC_FLAG_TRANS;
	obj->next = s->gc_root;
	s->gc_root = obj;
	s->num_objects++;
	return obj;
}

gc_t *string_from_db(su_state *s, unsigned hash, unsigned size, const char *str) {
	value_t key, v;
	key.type = SU_NATIVEPTR;
	key.obj.ptr = 0;
	memcpy(&key.obj.ptr, &hash, sizeof(unsigned));

	v = map_get(s, s->strings.obj.m, &key, hash);
	if (v.type != SU_INV)
		return v.obj.gc_object;

	v.type = SU_STRING;
	v.obj.ptr = su_allocate(s, NULL, sizeof(string_t) + size);
	v.obj.str->size = size;
	memcpy(v.obj.str->str, str, size);
	v.obj.str->str[size] = '\0';
	v.obj.str->hash = hash;
	gc_insert_object(s, v.obj.gc_object, SU_STRING);
	s->strings = map_insert(s, s->strings.obj.m, &key, hash, &v);

	return v.obj.gc_object;
}

const char *su_stringify(su_state *s, int idx) {
	int tmp;
	value_t *v = STK(idx);
	switch (v->type) {
		case SU_NIL:
			return "nil";
		case SU_BOOLEAN:
			return v->obj.b ? "true" : "false";
		case SU_NUMBER:
			tmp = (int)v->obj.num;
			if (v->obj.num == (double)tmp)
				sprintf(s->scratch_pad, "%i", tmp);
			else
				sprintf(s->scratch_pad, "%f", v->obj.num);
			break;
		case SU_STRING:
			sprintf(s->scratch_pad, "%s", ((string_t*)v->obj.gc_object)->str);
			break;
		case SU_FUNCTION:
			sprintf(s->scratch_pad, "<function %p>", (void*)v->obj.func);
			break;
		case SU_NATIVEFUNC:
			sprintf(s->scratch_pad, "<native-function %p>", (void*)v->obj.nfunc);
			break;
		case SU_NATIVEPTR:
			sprintf(s->scratch_pad, "<native-pointer %p>", v->obj.ptr);
			break;
		case SU_NATIVEDATA:
			sprintf(s->scratch_pad, "<native-data %p>", v->obj.ptr);
			break;
		case SU_VECTOR:
			sprintf(s->scratch_pad, "<vector %p>", v->obj.ptr);
			break;
		case SU_MAP:
			sprintf(s->scratch_pad, "<hashmap %p>", v->obj.ptr);
			break;
		case SU_LOCAL:
			sprintf(s->scratch_pad, "<reference %p>", v->obj.ptr);
			break;
		case SU_INV:
			su_error(s, "Invalid type!");
			break;
		case IT_SEQ:
		case CELL_SEQ:
			sprintf(s->scratch_pad, "<sequence %p>", v->obj.ptr);
			break;
		default:
			assert(0);
	}
	return s->scratch_pad;
}

static void update_global_ref(su_state *s) {
	value_t key, val;
	unsigned hash = murmur("_G", 3, 0);
	key.type = SU_STRING;
	key.obj.gc_object = string_from_db(s, hash, 3, "_G");
	val = ref_local(s, &s->globals);

	hash = hash_value(&key);
	s->globals = map_insert(s, s->globals.obj.m, &key, hash, &val);
}

static int isseq(su_state *s, int idx) {
	switch (STK(idx)->type) {
		case IT_SEQ:
		case CELL_SEQ:
			return 1;
		default:
			return 0;
	}
}

void su_pushnil(su_state *s) {
	value_t v;
	v.type = SU_NIL;
	push_value(s, &v);
}

void su_pushfunction(su_state *s, su_nativefunc f) {
	value_t v;
	v.type = SU_NATIVEFUNC;
	v.obj.nfunc = f;
	push_value(s, &v);
}

su_nativefunc su_tofunction(su_state *s, int idx) {
	if (STK(idx)->type == SU_NATIVEFUNC)
		return STK(idx)->obj.nfunc;
	return NULL;
}

void su_pushnumber(su_state *s, double n) {
	value_t v;
	v.type = SU_NUMBER;
	v.obj.num = n;
	push_value(s, &v);
}

double su_tonumber(su_state *s, int idx) {
	if (STK(idx)->type == SU_NUMBER)
		return STK(idx)->obj.num;
	return 0.0;
}

void su_pushpointer(su_state *s, void *ptr) {
	value_t v;
	v.type = SU_NATIVEPTR;
	v.obj.ptr = ptr;
	push_value(s, &v);
}

void *su_topointer(su_state *s, int idx) {
	if (STK(idx)->type == SU_NATIVEPTR)
		return STK(idx)->obj.ptr;
	return NULL;
}

void su_pushinteger(su_state *s, int i) {
	su_pushnumber(s, (double)i);
}

int su_tointeger(su_state *s, int idx) {
	return (int)su_tonumber(s, idx);
}

void su_pushboolean(su_state *s, int b) {
	value_t v;
	v.type = SU_BOOLEAN;
	v.obj.b = b;
	push_value(s, &v);
}

int su_toboolean(su_state *s, int idx) {
	if (STK(idx)->type == SU_BOOLEAN)
		return STK(idx)->obj.b;
	return 0;
}

void su_pushbytes(su_state *s, const char *ptr, unsigned size) {
	value_t v;
	v.type = SU_STRING;
	v.obj.gc_object = string_from_db(s, murmur(ptr, size, 0), size, ptr);
	push_value(s, &v);
}

void su_pushstring(su_state *s, const char *str) {
	su_pushbytes(s, str, (unsigned)strlen(str) + 1);
}

const char *su_tostring(su_state *s, int idx, unsigned *size) {
	string_t *str;
	if (STK(idx)->type == SU_STRING) {
		str = (string_t*)STK(idx)->obj.gc_object;
		if (size) *size = str->size;
		return str->str;
	}
	return NULL;
}

void error(su_state *s, const char *fmt, va_list args) {
	int i;
	const_string_t *str;
	char *filename = "?";

	if (fmt) {
		fputs("\n", s->fstderr);
		vfprintf(s->fstderr, fmt, args);
		fputs("\n", s->fstderr);
	}

	for (i = s->frame_top - 1; i >= 0; i--) {
		str = s->frames[i].func->prot->name;
		if (str->size) {
			filename = str->str;
			break;
		}
	}

	if (s->prot && s->pc < s->prot->num_lineinf)
		fprintf(s->fstderr, "%s : %i>\n\n", filename, s->prot->lineinf[s->pc]);
	else
		fprintf(s->fstderr, "%s : -1>\n\n", filename);

	fflush(s->fstderr);
	if (s->errtop >= 0)
		longjmp(s->err, 1);
	else
		exit(-1);
}

void su_error(su_state *s, const char *fmt, ...) {
	va_list args;
	if (fmt) {
		va_start(args, fmt);
		error(s, fmt, args);
		va_end(args);
	} else {
		error(s, fmt, args);
	}
}

void su_assert(su_state *s, int cond, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	if (!cond) error(s, fmt, args);
	va_end(args);
}

static const char *type_name(su_object_type_t type) {
	switch ((unsigned)type) {
		case SU_NIL: return "nil";
		case SU_BOOLEAN: return "boolean";
		case SU_STRING: return "string";
		case SU_NUMBER: return "number";
		case SU_FUNCTION: return "function";
		case SU_NATIVEFUNC: return "native-function";
		case SU_NATIVEPTR: return "native-pointer";
		case SU_NATIVEDATA: return "native-data";
		case SU_VECTOR: return "vector";
		case SU_MAP: return "map";
		case SU_LOCAL: return "reference";
		case SU_SEQ:
		case CELL_SEQ:
		case IT_SEQ:
			return "sequence";
		default: assert(0);
	}
	return NULL;
}

const char *su_type_name(su_state *s, int idx) {
	return type_name(STK(idx)->type);
}

void su_map(su_state *s, int num) {
	int i;
	value_t k, v;
	value_t m = map_create_empty(s);
	su_assert(s, num % 2 == 0, "Expected key value pairs!");
	for (i = num; i > 0; i -= 2) {
		k = *STK(-i);
		v = *STK(-i + 1);
		m = map_insert(s, m.obj.m, &k, hash_value(&k), &v);
	}
	push_value(s, &m);
}

int su_map_length(su_state *s, int idx) {
	return map_length(STK(idx)->obj.m);
}

int su_map_get(su_state *s, int idx) {
	value_t v = *STK(-1);
	unsigned hash = hash_value(&v);
	v = map_get(s, STK(idx)->obj.m, &v, hash);
	if (v.type == SU_INV)
		return 0;
	push_value(s, &v);
	return 1;
}

void su_map_insert(su_state *s, int idx) {
	value_t key = *STK(-2);
	unsigned hash = hash_value(&key);
	key = map_insert(s, STK(idx)->obj.m, &key, hash, STK(-1));
	push_value(s, &key);
}

void su_map_remove(su_state *s, int idx) {
	value_t key = *STK(-1);
	unsigned hash = hash_value(&key);
	key = map_remove(s, STK(idx)->obj.m, &key, hash);
	push_value(s, &key);
}

int su_map_has(su_state *s, int idx) {
	value_t v = *STK(-1);
	unsigned hash = hash_value(&v);
	return map_get(s, STK(idx)->obj.m, &v, hash).type != SU_INV;
}

void su_list(su_state *s, int num) {
	value_t seq = cell_create_array(s, STK(-num), num);
	push_value(s, &seq);
}

void su_first(su_state *s, int idx) {
	value_t v = seq_first(s, STK(idx)->obj.q);
	push_value(s, &v);
}

void su_rest(su_state *s, int idx) {
	value_t v = seq_rest(s, STK(idx)->obj.q);
	push_value(s, &v);
}

void su_cons(su_state *s, int idx) {
	value_t v = cell_create(s, STK(-1), STK(idx));
	push_value(s, &v);
}

void su_seq(su_state *s, int idx) {
	value_t v;
	value_t *seq = STK(idx);
	switch (su_type(s, idx)) {
		case SU_VECTOR:
			v = it_create_vector(s, seq->obj.vec);
			break;
		case SU_MAP:
			su_error(s, "Not implemented!");
			break;
		case SU_STRING:
			v = it_create_string(s, seq->obj.str);
			break;
		case SU_SEQ:
			v = *seq;
			break;
		default:
			su_error(s, "Can't sequence object of type: %s", type_name((su_object_type_t)seq->type));
	}
	push_value(s, &v);
}

void su_vector(su_state *s, int num) {
	int i;
	value_t vec = vector_create_empty(s);
	for (i = 0; i < num; i++)
		vec = vector_push(s, vec.obj.vec, STK(-(num - i)));
	push_value(s, &vec);
}

int su_vector_length(su_state *s, int idx) {
	return vector_length(STK(idx)->obj.vec);
}

void su_vector_index(su_state *s, int idx) {
	s->stack[s->stack_top - 1] = vector_index(s, STK(idx)->obj.vec, (int)STK(-1)->obj.num);
}

void su_vector_set(su_state *s, int idx) {
	s->stack[s->stack_top - 2] = vector_set(s, STK(idx)->obj.vec, (int)STK(-2)->obj.num, STK(-1));
	su_pop(s, 1);
}

void su_vector_push(su_state *s, int idx, int num) {
	int i;
	value_t vec = *STK(idx);
	for (i = 0; i < num; i++)
		vec = vector_push(s, vec.obj.vec, STK(-(num - i)));
	push_value(s, &vec);
}

void su_vector_pop(su_state *s, int idx, int num) {
	int i;
	int n = (int)STK(num)->obj.num;
	value_t vec = *STK(idx);
	for (i = 0; i < n; i++)
		vec = vector_pop(s, vec.obj.vec);
	push_value(s, &vec);
}

static void push_varg(su_state *s, int num) {
	int i;
	value_t vec = vector_create_empty(s);
	for (i = 0; i < num; i++)
		vec = vector_push(s, vec.obj.vec, STK(-(num - i)));
	su_pop(s, num);
	push_value(s, &vec);
}

void su_check_type(su_state *s, int idx, su_object_type_t t) {
	if (STK(idx)->type != t)
		su_error(s, "Bad argument: Expected %s, but got %s.", type_name(t), type_name((su_object_type_t)STK(idx)->type));
}

void su_seterror(su_state *s, jmp_buf jmp, int flag) {
	memcpy(s->err, jmp, sizeof(jmp_buf));
	if (flag && s->errtop >= 0)
		s->stack_top = s->errtop;
	else if (flag < 0)
		s->errtop = -1;
	else
		s->errtop = s->stack_top;
}

su_object_type_t su_type(su_state *s, int idx) {
	return isseq(s, idx) ? SU_SEQ : (su_object_type_t)STK(idx)->type;
}

int su_getglobal(su_state *s, const char *name) {
	value_t v;
	unsigned size = strlen(name) + 1;
	unsigned hash = murmur(name, size, 0);

	v.type = SU_STRING;
	v.obj.gc_object = string_from_db(s, hash, size, name);
	hash = hash_value(&v);

	v = map_get(s, s->globals.obj.m, &v, hash);
	if (v.type == SU_INV)
		return 0;

	push_value(s, &v);
	return 1;
}

void su_setglobal(su_state *s, int replace, const char *name) {
	value_t v;
	unsigned size = strlen(name) + 1;
	unsigned hash = murmur(name, size, 0);

	v.type = SU_STRING;
	v.obj.gc_object = string_from_db(s, hash, size, name);
	hash = hash_value(&v);

	if (!replace)
		su_assert(s, map_get(s, s->globals.obj.m, &v, hash).type == SU_INV, "Duplicated global!");

	s->globals = map_insert(s, s->globals.obj.m, &v, hash, STK(-1));
	update_global_ref(s);
	su_pop(s, 1);
}

value_t create_value(su_state *s, const_t *constant) {
	unsigned hash;
	value_t v;
	switch (constant->id) {
		case CSTRING:
			v.type = SU_STRING;
			hash = murmur(constant->obj.str->str, constant->obj.str->size, 0);
			v.obj.gc_object = string_from_db(s, hash, constant->obj.str->size, constant->obj.str->str);
			break;
		case CNUMBER:
			v.type = SU_NUMBER;
			v.obj.num = constant->obj.num;
			break;
		case CTRUE:
			v.type = SU_BOOLEAN;
			v.obj.b = SU_TRUE;
			break;
		case CFALSE:
			v.type = SU_BOOLEAN;
			v.obj.b = SU_FALSE;
			break;
		case CNIL:
			v.type = SU_NIL;
			break;
		default:
			assert(0);
	}
	return v;
}

#define READ(p, n) { if (buffer_read(s, buffer, (p), (n))) goto error; }

static const_string_t *read_string(su_state *s, reader_buffer_t *buffer) {
	const_string_t *str;
	unsigned size;

	if (buffer_read(s, buffer, &size, sizeof(unsigned)))
		return NULL;

	str = su_allocate(s, NULL, sizeof(unsigned) + size);
	if (buffer_read(s, buffer, str->str, size))
		return NULL;

	str->size = size;
	return str;
}

int read_prototype(su_state *s, reader_buffer_t *buffer, prototype_t *prot) {
	unsigned i;
	memset(prot, 0, sizeof(prototype_t));

	assert(sizeof(unsigned) == 4);
	assert(sizeof(instruction_t) == 4);

	READ(&prot->num_inst, sizeof(unsigned));
	prot->inst = su_allocate(s, NULL, sizeof(instruction_t) * prot->num_inst);
	for (i = 0; i < prot->num_inst; i++)
		READ(&prot->inst[i], sizeof(instruction_t));

	READ(&prot->num_const, sizeof(unsigned));
	prot->constants = su_allocate(s, NULL, sizeof(const_t) * prot->num_const);
	for (i = 0; i < prot->num_const; i++) {
		READ(&prot->constants[i].id, sizeof(char));
		switch (prot->constants[i].id) {
			case CSTRING:
				prot->constants[i].obj.str = read_string(s, buffer);
				if (!prot->constants[i].obj.str)
					goto error;
				break;
			case CNUMBER:
				READ(&prot->constants[i].obj.num, sizeof(double));
				break;
			case CTRUE:
			case CFALSE:
			case CNIL:
				break;
			default:
				assert(0);
		}
	}

	READ(&prot->num_ups, sizeof(unsigned));
	prot->upvalues = su_allocate(s, NULL, sizeof(upvalue_t) * prot->num_ups);
	for (i = 0; i < prot->num_ups; i++)
		READ(&prot->upvalues[i], sizeof(upvalue_t));

	READ(&prot->num_prot, sizeof(unsigned));
	prot->prot = su_allocate(s, NULL, sizeof(prototype_t) * prot->num_prot);
	for (i = 0; i < prot->num_prot; i++) {
		if (read_prototype(s, buffer, &prot->prot[i]))
			goto error;
	}

	prot->name = read_string(s, buffer);
	if (!prot->name)
		goto error;

	READ(&prot->num_lineinf, sizeof(unsigned));
	prot->lineinf = su_allocate(s, NULL, sizeof(int) * prot->num_lineinf);
	for (i = 0; i < prot->num_lineinf; i++)
		READ(&prot->lineinf[i], sizeof(unsigned));

	return 0;

error:
	/* TODO: Deleate the rest of the objects. */
	return -1;
}

#undef READ

void lambda(su_state *s, prototype_t *prot, int narg) {
	unsigned i, tmp;
	value_t v;
	function_t *func = su_allocate(s, NULL, sizeof(function_t));

	func->narg = narg;
	func->prot = prot;
	func->num_const = prot->num_const;
	func->num_ups = prot->num_ups;
	func->constants = su_allocate(s, NULL, sizeof(value_t) * prot->num_const);
	func->upvalues = su_allocate(s, NULL, sizeof(value_t) * prot->num_ups);

	for (i = 0; i < func->num_const; i++)
		func->constants[i] = create_value(s, &prot->constants[i]);

	for (i = 0; i < func->num_ups; i++) {
		tmp = s->frame_top - prot->upvalues[i].lv;
		tmp = s->frames[tmp].stack_top + prot->upvalues[i].idx + 1;
		func->upvalues[i] = s->stack[tmp];
	}

	gc_insert_object(s, (gc_t*)func, SU_FUNCTION);
	v.type = SU_FUNCTION;
	v.obj.func = func;
	push_value(s, &v);
}

int su_load(su_state *s, su_reader reader, void *data) {
	prototype_t *prot = su_allocate(s, NULL, sizeof(prototype_t));
	reader_buffer_t *buffer = buffer_open(s, reader, data);

	if (verify_header(s, buffer)) {
		buffer_close(s, buffer);
		return -1;
	}

	if (read_prototype(s, buffer, prot)) {
		buffer_close(s, buffer);
		return -1;
	}

	buffer_close(s, buffer);
	gc_insert_object(s, &prot->gc, PROTOTYPE);
	lambda(s, prot, -1);
	return 0;
}

static void check_args(su_state *s, va_list arg, int start, int num) {
	int i;
	su_object_type_t a, b;
	for (i = start; i - num > 0; i--) {
		a = su_type(s, -i);
		b = va_arg(arg, su_object_type_t);
		if (b != SU_NIL)
			su_assert(s, a == b, "Expected argument %i to be of type '%s', but it is of type '%s'.", start - i, type_name(b), type_name(a));
	}
}

void su_check_arguments(su_state *s, int num, ...) {
	va_list arg;
	va_start(arg, num);

	if (s->narg != num) {
		if (num < 0) {
			num *= -1;
			if (num > s->narg)
				su_error(s, "To few arguments passed to function. Expected at least %i but got %i.", num, s->narg);
			check_args(s, arg, s->narg, num);
		} else {
			su_error(s, "Bad number of arguments to function. Expected %i but got %i.", num, s->narg);
		}
	} else {
		check_args(s, arg, num, num);
	}

	va_end(arg);
}

static void global_error(su_state *s, const char *msg, value_t *constant) {
	assert(constant->type == SU_STRING);
	fprintf(s->fstderr, "%s: %s\n", msg, ((string_t*)constant->obj.gc_object)->str);
	su_error(s, NULL);
}

static void vm_loop(su_state *s, function_t *func) {
	value_t tmpv;
	instruction_t inst;
	int tmp, narg, i;

	s->frame = FRAME();
	s->prot = func->prot;

	#define ARITH_OP(op) \
		su_check_type(s, -2, SU_NUMBER); \
		su_check_type(s, -1, SU_NUMBER); \
		STK(-2)->obj.num = STK(-2)->obj.num op STK(-1)->obj.num; \
		su_pop(s, 1); \
		break;

	#define LOG_OP(op) \
		su_check_type(s, -2, SU_NUMBER); \
		su_check_type(s, -1, SU_NUMBER); \
		STK(-2)->type = SU_BOOLEAN; \
		STK(-2)->obj.b = STK(-2)->obj.num op STK(-1)->obj.num; \
		su_pop(s, 1); \
		break;

	for (s->pc = 0; s->pc < s->prot->num_inst; s->pc++) {
		if (s->interupt) {
			if ((s->interupt & IGC) == IGC)
				gc_trace(s);
			s->interupt = 0x0;
		}
		inst = s->prot->inst[s->pc];
		switch (inst.id) {
			case OP_PUSH:
				push_value(s, &func->constants[inst.a]);
				break;
			case OP_POP:
				su_pop(s, inst.a);
				break;
			case OP_ADD: ARITH_OP(+)
			case OP_SUB: ARITH_OP(-)
			case OP_MUL: ARITH_OP(*)
			case OP_DIV: ARITH_OP(/)
			case OP_MOD:
				su_check_type(s, -2, SU_NUMBER);
				su_check_type(s, -1, SU_NUMBER);
				STK(-2)->obj.num = (double)((int)STK(-2)->obj.num % (int)STK(-1)->obj.num);
				su_pop(s, 1);
				break;
			case OP_POW:
				su_check_type(s, -2, SU_NUMBER);
				su_check_type(s, -1, SU_NUMBER);
				STK(-2)->obj.num = pow(STK(-2)->obj.num, STK(-1)->obj.num);
				su_pop(s, 1);
				break;
			case OP_UNM:
				su_check_type(s, -1, SU_NUMBER);
				STK(-1)->obj.num = -STK(-1)->obj.num;
				break;
			case OP_EQ:
				if (STK(-2)->type != STK(-1)->type)
					STK(-2)->obj.b = false;
				else if (STK(-2)->type == SU_NUMBER)
					STK(-2)->obj.b = STK(-2)->obj.num == STK(-1)->obj.num;
				else
					STK(-2)->obj.b = STK(-2)->obj.ptr == STK(-1)->obj.ptr;
				STK(-2)->type = SU_BOOLEAN;
				su_pop(s, 1);
				break;
			case OP_LESS: LOG_OP(<);
			case OP_LEQUAL: LOG_OP(<=);
			case OP_NOT:
				if (STK(-1)->type == SU_BOOLEAN)
					STK(-1)->obj.b = !STK(-1)->obj.b;
				else
					STK(-1)->obj.b = STK(-1)->type != SU_NIL;
				STK(-1)->type = SU_BOOLEAN;
				break;
			case OP_AND:
				tmp = STK(-2)->type != SU_NIL && (STK(-2)->type != SU_BOOLEAN || STK(-2)->obj.b);
				STK(-2)->obj.b = tmp && STK(-1)->type != SU_NIL && (STK(-1)->type != SU_BOOLEAN || STK(-1)->obj.b);
				STK(-2)->type = SU_BOOLEAN;
				su_pop(s, 1);
				break;
			case OP_OR:
				tmp = STK(-2)->type != SU_NIL || (STK(-2)->type != SU_BOOLEAN || STK(-2)->obj.b);
				STK(-2)->obj.b = tmp || STK(-1)->type != SU_NIL || (STK(-1)->type != SU_BOOLEAN || STK(-1)->obj.b);
				STK(-2)->type = SU_BOOLEAN;
				su_pop(s, 1);
				break;
			case OP_TEST:
				if (STK(-1)->type != SU_NIL && (STK(-1)->type != SU_BOOLEAN || STK(-1)->obj.b))
					s->pc = inst.a - 1;
				su_pop(s, 1);
				break;
			case OP_JMP:
				s->pc = inst.a - 1;
				break;
			case OP_RETURN:
				s->pc = s->frame->ret_addr - 1;
				s->prot = s->frame->func->prot;
				func = s->frame->func;

				s->stack[s->frame->stack_top] = *STK(-1);
				s->stack_top = s->frame->stack_top + 1;
				s->frame_top--;
				s->frame = FRAME();
				break;
			case OP_COPY:
				assert(s->stack_top < STACK_SIZE);
				s->stack[s->stack_top++] = s->stack[FRAME()->stack_top + inst.a];
				break;
			case OP_TCALL:
				s->pc = s->frame->ret_addr - 1;
				s->prot = s->frame->func->prot;
				func = s->frame->func;

				memcpy(&s->stack[s->frame->stack_top], &s->stack[s->stack_top - (inst.a + 1)], sizeof(value_t) * (inst.a + 1));
				s->stack_top = s->frame->stack_top + inst.a + 1;
				s->frame_top--;
				s->frame = FRAME();

				/* Do a normal call. */
			case OP_CALL:
				tmp = s->stack_top - inst.a - 1;
				if (s->stack[tmp].type == SU_FUNCTION) {
					s->frame = &s->frames[s->frame_top++];
					assert(s->frame_top <= MAX_CALLS);
					s->frame->ret_addr = s->pc + 1;
					s->frame->func = func;
					s->frame->stack_top = tmp;

					func = s->stack[tmp].obj.func;
					if (func->narg < 0)
						push_varg(s, inst.a);
					else if (func->narg != inst.a)
						su_error(s, "Bad number of arguments to function! Expected %i, but got %i.", (int)func->narg, (int)inst.a);

					assert(s->stack_top + func->num_ups <= STACK_SIZE);
					memcpy(&s->stack[s->stack_top], func->upvalues, sizeof(value_t) * func->num_ups);
					s->stack_top += func->num_ups;

					s->prot = func->prot;
					s->pc = -1;
				} else if (s->stack[tmp].type == SU_NATIVEFUNC) {
					narg = s->narg;
					s->narg = inst.a;
					if (s->stack[tmp].obj.nfunc(s, inst.a)) {
						s->stack[tmp] = *STK(-1);
					} else {
						s->stack[tmp].type = SU_NIL;
					}
					s->stack_top = tmp + 1;
					s->narg = narg;
				} else if (s->stack[tmp].type == SU_VECTOR) {
					if (inst.a == 0) {
						su_error(s, "Expected at least one argument!");
					} else if (inst.a == 1) {
						su_check_type(s, -1, SU_NUMBER);
						tmpv = vector_index(s, s->stack[tmp].obj.vec, su_tointeger(s, -1));
						su_pop(s, 2);
						push_value(s, &tmpv);
					} else {
						su_error(s, "Not implemented!");
						for (i = tmp + 1; i < vector_length(s->stack[tmp].obj.vec); i++) {
							su_check_type(s, s->stack_top - i, SU_NUMBER);
							tmpv = vector_index(s, s->stack[tmp].obj.vec, su_tointeger(s, s->stack_top - i));
						}
					}
				} else if (s->stack[tmp].type == SU_MAP) {
					if (inst.a == 0) {
						su_error(s, "Expected at least one argument!");
					} else if (inst.a == 1) {
						tmpv = *STK(-1);
						tmpv = map_get(s, s->stack[tmp].obj.m, &tmpv, hash_value(&tmpv));
						su_assert(s, tmpv.type != SU_INV, "No value with that key!");
						su_pop(s, 2);
						push_value(s, &tmpv);
					} else {
						su_error(s, "Not implemented!");
					}
				} else {
					su_error(s, "Expected function to call, got '%s'.", type_name(s->stack[tmp].type));
				}
				break;
			case OP_LAMBDA:
				assert(inst.a < s->prot->num_prot);
				lambda(s, &s->prot->prot[inst.a], inst.b);
				break;
			case OP_GETGLOBAL:
				tmpv = func->constants[inst.a];
				su_assert(s, tmpv.type == SU_STRING, "Global key must be a string!");
				tmpv = map_get(s, s->globals.obj.m, &tmpv, hash_value(&tmpv));
				if (tmpv.type == SU_INV)
					global_error(s, "Can't access global variable", &func->constants[inst.a]);
				push_value(s, &tmpv);
				break;
			case OP_SETGLOBAL:
				tmpv = func->constants[inst.a];
				su_assert(s, tmpv.type == SU_STRING, "Global key must be a string!");
				i = hash_value(&tmpv);
				if (map_get(s, s->globals.obj.m, &tmpv, i).type != SU_INV)
					global_error(s, "Redefinition of global variable", &tmpv);

				s->globals = map_insert(s, s->globals.obj.m, &tmpv, i, STK(-1));
				update_global_ref(s);
				break;
			case OP_LOAD:
				push_value(s, &s->stack[FRAME()->stack_top + 1 + inst.a]);
				break;
			default:
				assert(0);
		}

		#undef ARITH_OP
		#undef LOG_OP
	}
}

void su_call(su_state *s, int narg, int nret) {
	int pc, tmp, fret;
	prototype_t *prot;
	int top = s->stack_top - narg - 1;
	value_t *f = &s->stack[top];
	frame_t *frame = &s->frames[s->frame_top++];
	assert(s->frame_top <= MAX_CALLS);

	frame->ret_addr = 0xffff;
	frame->func = f->obj.func;
	frame->stack_top = top;

	pc = s->pc;
	prot = s->prot;
	tmp = s->narg;
	s->narg = narg;

	if (f->type == SU_FUNCTION) {
		su_assert(s, f->obj.func->narg < 0 || f->obj.func->narg == narg, "Bad number of argument to function!");
		vm_loop(s, f->obj.func);
		if (nret == 0)
			su_pop(s, 1);
	} else if (f->type == SU_NATIVEFUNC) {
		fret = f->obj.nfunc(s, narg);
		if (nret > 0 && fret > 0) {
			s->stack[top] = *STK(-1);
			su_pop(s, narg);
		} else {
			s->stack_top = top;
			if (nret > 0)
				su_pushnil(s);
		}
		s->frame_top--;
	} else {
		assert(0);
	}

	s->narg = tmp;
	s->prot = prot;
	s->pc = pc;
}

FILE *su_stdout(su_state *s) {
	return s->fstdout;
}

FILE *su_stdin(su_state *s) {
	return s->fstdin;
}

FILE *su_stderr(su_state *s) {
	return s->fstderr;
}

void su_set_stdout(su_state *s, FILE *fp) {
	s->fstdout = fp;
}

void su_set_stdin(su_state *s, FILE *fp) {
	s->fstdin = fp;
}

void su_set_stderr(su_state *s, FILE *fp) {
	s->fstderr = fp;
}

static void *default_alloc(void *ptr, size_t size) {
	if (size) return realloc(ptr, size);
	free(ptr);
	return NULL;
}

su_state *su_init(su_alloc alloc) {
	su_alloc mf = alloc ? alloc : default_alloc;
	su_state *s = (su_state*)mf(NULL, sizeof(su_state));
	s->alloc = mf;

	s->num_objects = 0;
	s->gc_gray_size = 0;
	s->gc_root = NULL;

	s->fstdin = stdin;
	s->fstdout = stdout;
	s->fstderr = stderr;

	s->stack_top = 0;
	s->frame_top = 0;
	s->narg = 0;
	s->pc = 0xffff;
	s->interupt = 0x0;

	s->reader_pad = NULL;
	s->reader_pad_size = 0;
	s->reader_pad_top = 0;

	s->errtop = -1;

	s->strings = map_create_empty(s);
	s->globals = map_create_empty(s);
	update_global_ref(s);
	return s;
}

void su_close(su_state *s) {
	s->stack_top = 0;
	s->globals.type = SU_NIL;
	s->strings.type = SU_NIL;
	su_gc(s);

	if (s->fstdin != stdin) fclose(s->fstdin);
	if (s->fstdout != stdout) fclose(s->fstdout);
	if (s->fstderr != stderr) fclose(s->fstderr);

	s->alloc(s, 0);
}
