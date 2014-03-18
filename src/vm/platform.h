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

#ifndef _PLATFORM_H_
#define _PLATFORM_H_

#include "options.h"

#ifndef false
	#define false 0
#endif

#ifndef true
	#define true 1
#endif

#ifndef bool
	#define bool int
#endif

#if __STDC_VERSION__ >= 199901L
	#define INLINE inline
#else
	#define INLINE
#endif

#if !defined(SU_OPT_DYNLIB)
	static INLINE void lib_unload(void *lib) {}

	static INLINE void *lib_load(const char *path) {
		return NULL;
	}

	static INLINE su_nativefunc lib_sym(void *lib, const char *sym) {
		return NULL;
	}
#elif defined(_WIN32)
	#include <windows.h>

	static INLINE void lib_unload(void *lib) {
		FreeLibrary((HINSTANCE)lib);
	}

	static INLINE void *lib_load(const char *path) {
		return (void*)LoadLibraryA(path);
	}

	static INLINE su_nativefunc lib_sym(void *lib, const char *sym) {
		return (su_nativefunc)GetProcAddress((HINSTANCE)lib, sym);
	}
#else
	#include <dlfcn.h>

	static INLINE void lib_unload(void *lib) {
		dlclose(lib);
	}

	static INLINE void *lib_load(const char *path) {
		return dlopen(path, RTLD_NOW);
	}

	static INLINE su_nativefunc lib_sym(void *lib, const char *sym) {
		return (su_nativefunc)dlsym(lib, sym);
	}
#endif

static INLINE const char *platform_name() {
	#if defined(_WIN64)
		return "win64_x86";
	#elif defined(_WIN32)
		return "win32_x86";
	#elif defined(__APPLE__)
		#if defined(__x86_64__)
			return "osx64_x86";
		#elif defined(__i386__)
			return "osx32_x86";
		#endif
	#elif defined(__linux)
		#if defined(__x86_64__)
			return "linux64_x86";
		#elif defined(__ppc64__)
			return "linux64_ppc";
		#elif defined(__ppc__)
			return "linux32_ppc";
		#elif defined(__arm__)
			return "linux32_arm";
		#elif defined(__aarch64__)
			return "linux64_arm";
		#else
			return "linux32_x86";
		#endif
	#endif
	return NULL;
}

#endif
