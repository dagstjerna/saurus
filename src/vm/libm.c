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

#include <math.h>
#include <float.h>

#define REG(name) \
	su_pushfunction(s, &_##name); \
	su_setglobal(s, 1, "math-" #name);

#define FUNC(name) \
	static int _##name(su_state *s, int narg) { \
		su_check_arguments(s, 1, SU_NUMBER); \
		su_pushnumber(s, name(su_tonumber(s, -1))); \
		return 1; \
	}

#define FUNC2(name) \
	static int _##name(su_state *s, int narg) { \
		su_check_arguments(s, 2, SU_NUMBER, SU_NUMBER); \
		su_pushnumber(s, name(su_tonumber(s, -2), su_tonumber(s, -1))); \
		return 1; \
	}

static int not_implemented(su_state *s, int narg) {
	su_error(s, "Not implemented!");
	return 0;
}

FUNC(cos)
FUNC(sin)
FUNC(tan)
FUNC(acos)
FUNC(asin)
FUNC(atan)
FUNC2(atan2)

FUNC(cosh)
FUNC(sinh)
FUNC(tanh)
FUNC(acosh)
FUNC(asinh)
FUNC(atanh)

FUNC(sqrt)
FUNC(exp)
FUNC2(fmod)

static int _frexp(su_state *s, int narg) {
	return not_implemented(s, narg);
}

static int _ldexp(su_state *s, int narg) {
	return not_implemented(s, narg);
}

FUNC(log)
FUNC(log10)

static int _modfi(su_state *s, int narg) {
	double n;
	su_check_arguments(s, 1, SU_NUMBER);
	su_pushnumber(s, modf(su_tonumber(s, -1), &n));
	return 1;
}

static int _modff(su_state *s, int narg) {
	double n;
	su_check_arguments(s, 1, SU_NUMBER);
	modf(su_tonumber(s, -1), &n);
	su_pushnumber(s, n);
	return 1;
}

static int _random(su_state *s, int narg) {
	su_check_arguments(s, 0);
	su_pushinteger(s, rand());
	return 0;
}

static int _randomseed(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_NUMBER);
	srand((unsigned)su_tonumber(s, -1));
	return 0;
}

static int _deg(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_NUMBER);
	su_pushnumber(s, su_tonumber(s, -1) * 180.0 / M_PI);
	return 1;
}

static int _rad(su_state *s, int narg) {
	su_check_arguments(s, 1, SU_NUMBER);
	su_pushnumber(s, su_tonumber(s, -1) * M_PI / 180.0);
	return 1;
}

static int _max(su_state *s, int narg) {
	int i;
	double m = 0.0;
	for (i = 1; i <= narg; i++)
		m = fmax(m, su_tonumber(s, -i));
	su_pushnumber(s, m);
	return 1;
}

static int _min(su_state *s, int narg) {
	int i;
	double m = 0.0;
	for (i = 1; i <= narg; i++)
		m = fmin(m, su_tonumber(s, -i));
	su_pushnumber(s, m);
	return 1;
}

FUNC(ceil)
FUNC(floor)
FUNC(abs)

static int _clamp(su_state *s, int narg) {
	su_check_arguments(s, 3, SU_NUMBER, SU_NUMBER, SU_NUMBER);
	su_pushnumber(s, fmin(fmax(su_tonumber(s, -3), su_tonumber(s, -2)), su_tonumber(s, -1)));
	return 1;
}

void libm(su_state *s) {
	REG(cos);
	REG(sin);
	REG(tan);
	REG(acos);
	REG(asin);
	REG(atan);
	REG(atan2);
	
	REG(cosh);
	REG(sinh);
	REG(tanh);
	REG(acosh);
	REG(asinh);
	REG(atanh);
	
	REG(sqrt);
	REG(exp);
	REG(fmod);
	REG(frexp);
	REG(ldexp);
	REG(log);
	REG(log10);
	REG(modfi);
	REG(modff);

	REG(random);
	REG(randomseed);
	
	REG(deg);
	REG(rad);
	REG(max);
	REG(min);
	REG(ceil);
	REG(floor);
	REG(abs);
	REG(clamp);
	
	su_pushnumber(s, M_PI);
	su_setglobal(s, 1, "math-pi");
	su_pushnumber(s, DBL_MAX);
	su_setglobal(s, 1, "math-big");
	su_pushnumber(s, DBL_MIN);
	su_setglobal(s, 1, "math-small");
}
