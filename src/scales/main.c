/******************************************************************************/
/* S C A L E S                                                                */
/* Copyright (c) 2014 Andreas T Jonsson <andreas@saurus.org>                  */
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>

#define VERSION_MAJOR 0
#define VERSION_MINOR 0
#define VERSION_PATCH 1

#define PLATFORM_ARCH "amd64"
#define PLATFORM_OS "linux64"

#define TEMP_BUFFER_SIZE (1024*64) 
static char g_command[TEMP_BUFFER_SIZE] = {'\0'};

const char *g_install_path;

char g_name[128];
char g_version[128];
char g_path[256];

static unsigned hash(const void *key) {
	const unsigned seed = 0;
	const unsigned m = 0x5bd1e995;
	const int r = 24;
	int len = strlen(key) + 1;
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

static int file_exist(const char *dst) {
	FILE *fp = fopen(dst, "r");
	if (!fp)
		return 0;
	fclose(fp);
	return 1;
}

static int folder_exist(const char *dst) {
	struct stat info;
	if (stat(dst, &info))
		return 0;
	else if (info.st_mode & S_IFDIR)
		return 1;
	fprintf(stderr, "Database is corrupt!\n");
	return -1;
}

static int rd(const char *dst) {
	sprintf(g_command, "rm -rf \"%s\"", dst);
	return system(g_command);
}

static int clone(const char *src, const char *dst) {
	sprintf(g_command, "git clone \"%s\" \"%s\"", src, dst);
	if (system(g_command) != 0) {
		rd(dst);
		return -1;
	}
	return 0;
}

static int fetch(const char *src) {
	sprintf(g_command, "cd \"%s\" && git fetch --all", src);
	return system(g_command);
}

static int checkout(const char *src, const char *hash) {
	sprintf(g_command, "cd \"%s\" && git checkout %s", src, hash);
	return system(g_command);
}

static int write_desc(const char *dst, const char *src, const char *version) {
	char buffer[256];
	sprintf(buffer, "%s/scale.desc", dst);
	FILE *fp = fopen(buffer, "w");
	if (!fp)
		return -1;
	fprintf(fp, "%s\n%s", src, version);
	fclose(fp);
	return 0;
}

static int write_dep(const char *dst, const char *dep) {
	char buffer[256];
	if (dep) {
		sprintf(buffer, "%s/scale.desc", dst);
		FILE *fp = fopen(buffer, "w");
		if (!fp)
			return -1;
		fprintf(fp, "%s\n", dep);
		fclose(fp);
	}
	return 0;
}

static int ensure_platform_and_version(const char *dst) {
	int major, minor, patch;
	char name[256];
	char buffer[256];
	char arch[128];
	char os[128];
	
	sprintf(buffer, "%s/scale.conf", dst);
	FILE *fp = fopen(buffer, "r");
	if (!fp)
		return -1;
	if (fscanf(fp, "%i.%i.%i", &major, &minor, &patch) != 3) {
		fclose(fp);
		return -1;
	}
	if (fscanf(fp, "%s", name) != 1) {
		fclose(fp);
		return -1;
	}
	if (fscanf(fp, "%s %s", arch, os) != 2) {
		fclose(fp);
		return -1;
	}		
	fclose(fp);
	
	if (major != VERSION_MAJOR || minor != VERSION_MINOR)
		return -1;
	if (!strcmp(arch, "any") && !strcmp(os, "any"))
		return 0;
	if (strcmp(arch, PLATFORM_ARCH) || strcmp(os, PLATFORM_OS))
		return -1;
		
	return 0;
}

int install(const char *src, const char *version, const char *dep) {
	char buffer[128];
	sprintf(buffer, "%s%u", g_install_path, hash(src) + hash(version));
	if (folder_exist(buffer)) {
		fprintf(stderr, "%s:%s is installed!\n", src, version);
		return -1;
	}
	
	if (clone(src, buffer) != 0) {
		fprintf(stderr, "Could not clone %s:%s\n", src, version);
		return -1;
	}
	
	if (strcmp(version, "master")) {
		if (checkout(buffer, version) != 0) {
			fprintf(stderr, "Could find version %s\n", version);
			rd(buffer);
			return -1;
		}
	}
	
	if (
		ensure_platform_and_version(buffer) |
		write_desc(buffer, src, version) |
		write_dep(buffer, dep)
		/*resolv_dep(dst)*/
	) {
		fprintf(stderr, "Package is invalid %s:%s\nTry upgrade Scales!\n", src, version);
		rd(buffer);
		return -1;
	}
	
	return 0;
}

int ensure(const char *src, const char *version) {
	char buffer[128];
	sprintf(buffer, "%s%u", g_install_path, hash(src) + hash(version));
	if (folder_exist(buffer))
		return 0;
	return install(src, version, NULL);
}

int revise(const char *src, const char *version, const char *new_version) {
	return 0;
}

int uninstall(const char *src, const char *version) {
	char buffer[128], buffer2[256];
	sprintf(buffer, "%s%u", g_install_path, hash(src) + hash(version));
	if (!folder_exist(buffer)) {
		fprintf(stderr, "%s:%s is not installed!\n", src, version);
		return -1;
	}
	
	sprintf(buffer2, "%s/scale.dep", buffer);
	if (file_exist(buffer2)) {
		fprintf(stderr, "Could not remove %s:%s because other packages is dependent on it!\n", src, version);
		/* Should print dep */
		return -1;
	}
	
	if (rd(buffer) != 0) {
		fprintf(stderr, "Could not remove %s:%s! Is the package in use by another process?\n", src, version);
		return -1;
	}
	return 0;
}

int update(const char *src, const char *version) {
	if (uninstall(src, version) || install(src, version, NULL))
		return -1;
	return 0;
}

const char *resolve_alias(const char *alias, const char *version) {
	FILE *fp;
	char buffer[512], buffer2[512];
	sprintf(buffer2, "%sscales.alias", g_install_path);
	sprintf(buffer, "%s.%s.%s", buffer2, PLATFORM_ARCH, PLATFORM_OS);
	
	fp = fopen(buffer, "r");
	if (!fp) {
		fprintf(stderr, "Could not open: %s\n", buffer);
		exit(-1);
	}
	do {
		if (fscanf(fp, "%s %s %s", g_name, g_path, g_version) != 3) {
			fclose(fp);
			FILE *fp = fopen(buffer2, "r");
			if (!fp) {
				fprintf(stderr, "Could not open: %s\n", buffer2);
				exit(-1);
			}
			do {
				if (fscanf(fp, "%s %s %s", g_name, g_path, g_version) != 3) {
					fclose(fp);
					strcpy(g_version, version);
					return alias;
				}
			} while (strcmp(alias, g_name));
			
			fclose(fp);
			return g_path;
		}
	} while (strcmp(alias, g_name));
	
	fclose(fp);
	return g_path;
}

int main(int argc, char *argv[]) {
	printf("S C A L E S\nCopyright (c) 2014 Andreas T Jonsson <andreas@saurus.org>\nVersion: %u.%u.%u\n\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	
	if (system("git --version") != 0) {
		fprintf(stderr, "Scales require GIT to be installed!");
		return -1;
	}
	
	g_install_path = getenv("SCALES");
	if (!g_install_path) {
		fprintf(stderr, "Could not find install path!");
		return -1;
	}
	
	if (argc == 3 || argc == 4) {
		if (!strcmp(argv[1], "install"))
			return install(resolve_alias(argv[2], argc == 4 ? argv[3] : "master"), g_version, NULL);
		if (!strcmp(argv[1], "ensure"))
			return ensure(resolve_alias(argv[2], argc == 4 ? argv[3] : "master"), g_version);
		if (!strcmp(argv[1], "uninstall"))
			return uninstall(resolve_alias(argv[2], argc == 4 ? argv[3] : "master"), g_version);
		if (!strcmp(argv[1], "update"))
			return update(resolve_alias(argv[2], argc == 4 ? argv[3] : "master"), g_version);
	}
	
	fprintf(stderr, "Usage: scales [install|uninstall] [alias|path] <version>\n");
	return -1;
}
