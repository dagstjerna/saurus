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

#ifndef _GC_H_
#define _GC_H_

#include "saurus.h"
#include "intern.h"

#define ALIVE_OBJECTS 100000

enum {
	GC_FLAG_TRANS,
	GC_FLAG_WHITE,
	GC_FLAG_GRAY,
	GC_FLAG_BLACK
};

void gc_trace(su_state *s);

#endif
