/*
 * Reusing the code from the MIR C driver, and picked up the part of it and a little modified.
 *  Regarding the original source code, refer to https://github.com/vnmakarov/mir/blob/master/c2mir/c2mir-driver.c
 *  The original copyright is below.
 *  Copyright (C) 2018-2021 Vladimir Makarov <vmakarov.gcc@gmail.com>.
 */
#include "resolver.h"
#include <stdio.h>
#include <stdarg.h>

#ifndef _WIN32
#include <dlfcn.h>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif
#else
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

struct lib {
  char *name;
  void *handler;
};

typedef struct lib lib_t;

#if defined(__unix__)
#if UINTPTR_MAX == 0xffffffff
static lib_t std_libs[]
  = {{"/lib/libc.so.6", NULL},   {"/lib32/libc.so.6", NULL},     {"/lib/libm.so.6", NULL},
     {"/lib32/libm.so.6", NULL}, {"/lib/libpthread.so.0", NULL}, {"/lib32/libpthread.so.0", NULL}};
static const char *std_lib_dirs[] = {"/lib", "/lib32"};
#elif UINTPTR_MAX == 0xffffffffffffffff
#if defined(__x86_64__)
static lib_t std_libs[]
  = {{"/lib64/libc.so.6", NULL},           {"/lib/x86_64-linux-gnu/libc.so.6", NULL},
     {"/lib64/libm.so.6", NULL},           {"/lib/x86_64-linux-gnu/libm.so.6", NULL},
     {"/usr/lib64/libpthread.so.0", NULL}, {"/lib/x86_64-linux-gnu/libpthread.so.0", NULL}};
static const char *std_lib_dirs[] = {"/lib64", "/lib/x86_64-linux-gnu"};
#elif (__aarch64__)
static lib_t std_libs[]
  = {{"/lib64/libc.so.6", NULL},       {"/lib/aarch64-linux-gnu/libc.so.6", NULL},
     {"/lib64/libm.so.6", NULL},       {"/lib/aarch64-linux-gnu/libm.so.6", NULL},
     {"/lib64/libpthread.so.0", NULL}, {"/lib/aarch64-linux-gnu/libpthread.so.0", NULL}};
static const char *std_lib_dirs[] = {"/lib64", "/lib/aarch64-linux-gnu"};
#elif (__PPC64__)
static lib_t std_libs[] = {
  {"/lib64/libc.so.6", NULL},
  {"/lib64/libm.so.6", NULL},
  {"/lib64/libpthread.so.0", NULL},
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  {"/lib/powerpc64le-linux-gnu/libc.so.6", NULL},
  {"/lib/powerpc64le-linux-gnu/libm.so.6", NULL},
  {"/lib/powerpc64le-linux-gnu/libpthread.so.0", NULL},
#else
  {"/lib/powerpc64-linux-gnu/libc.so.6", NULL},
  {"/lib/powerpc64-linux-gnu/libm.so.6", NULL},
  {"/lib/powerpc64-linux-gnu/libpthread.so.0", NULL},
#endif
};
static const char *std_lib_dirs[] = {
  "/lib64",
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  "/lib/powerpc64le-linux-gnu",
#else
  "/lib/powerpc64-linux-gnu",
#endif
};
#elif (__s390x__)
static lib_t std_libs[]
  = {{"/lib64/libc.so.6", NULL},       {"/lib/s390x-linux-gnu/libc.so.6", NULL},
     {"/lib64/libm.so.6", NULL},       {"/lib/s390x-linux-gnu/libm.so.6", NULL},
     {"/lib64/libpthread.so.0", NULL}, {"/lib/s390x-linux-gnu/libpthread.so.0", NULL}};
static const char *std_lib_dirs[] = {"/lib64", "/lib/s390x-linux-gnu"};
#elif (__riscv)
static lib_t std_libs[]
  = {{"/lib64/libc.so.6", NULL},       {"/lib/riscv64-linux-gnu/libc.so.6", NULL},
     {"/lib64/libm.so.6", NULL},       {"/lib/riscv64-linux-gnu/libm.so.6", NULL},
     {"/lib64/libpthread.so.0", NULL}, {"/lib/riscv64-linux-gnu/libpthread.so.0", NULL}};
static const char *std_lib_dirs[] = {"/lib64", "/lib/riscv64-linux-gnu"};
#else
#error cannot recognize 32- or 64-bit target
#endif
#endif
static const char *lib_suffix = ".so";
#endif

#ifdef _WIN32
static const int slash = '\\';
#else
static const int slash = '/';
#endif

#if defined(__APPLE__)
static lib_t std_libs[] = {{"/usr/lib/libc.dylib", NULL}, {"/usr/lib/libm.dylib", NULL}};
static const char *std_lib_dirs[] = {"/usr/lib"};
static const char *lib_suffix = ".dylib";
#endif

#ifdef _WIN32
static lib_t std_libs[] = {{"C:\\Windows\\System32\\msvcrt.dll", NULL},
                           {"C:\\Windows\\System32\\kernel32.dll", NULL},
                           {"C:\\Windows\\System32\\ucrtbase.dll", NULL}};
static const char *std_lib_dirs[] = {"C:\\Windows\\System32"};
static const char *lib_suffix = ".dll";
#define dlopen(n, f) LoadLibrary (n)
#define dlclose(h) FreeLibrary (h)
#define dlsym(h, s) GetProcAddress (h, s)
#endif

void open_std_libs(void)
{
    for (int i = 0; i < sizeof (std_libs) / sizeof (struct lib); i++) {
        if (std_libs[i].handler == NULL) {
            std_libs[i].handler = dlopen(std_libs[i].name, RTLD_LAZY);
        }
    }
}

void close_std_libs(void)
{
    for (int i = 0; i < sizeof (std_libs) / sizeof (lib_t); i++) {
        if (std_libs[i].handler != NULL) {
            dlclose(std_libs[i].handler);
        }
    }
}

void *import_resolver(const char *name)
{
    void *handler, *sym = NULL;

    for (int i = 0; i < sizeof (std_libs) / sizeof (struct lib); i++) {
        if ((handler = std_libs[i].handler) != NULL && (sym = dlsym(handler, name)) != NULL) break;
    }
    if (sym == NULL) {
    #ifdef _WIN32
        if (strcmp(name, "LoadLibrary") == 0) return LoadLibrary;
        if (strcmp(name, "FreeLibrary") == 0) return FreeLibrary;
        if (strcmp(name, "GetProcAddress") == 0) return GetProcAddress;
    #else
        if (strcmp(name, "dlopen") == 0) return dlopen;
        if (strcmp(name, "dlerror") == 0) return dlerror;
        if (strcmp(name, "dlclose") == 0) return dlclose;
        if (strcmp(name, "dlsym") == 0) return dlsym;
        if (strcmp(name, "stat") == 0) return stat;
        if (strcmp(name, "lstat") == 0) return lstat;
        if (strcmp(name, "fstat") == 0) return fstat;
    #if defined(__APPLE__) && defined(__aarch64__)
        if (strcmp(name, "__nan") == 0) return __nan;
        if (strcmp(name, "_MIR_set_code") == 0) return _MIR_set_code;
    #endif
    #endif
        fprintf(stderr, "can not load symbol %s\n", name);
        close_std_libs();
        exit(1);
    }
    return sym;
}
