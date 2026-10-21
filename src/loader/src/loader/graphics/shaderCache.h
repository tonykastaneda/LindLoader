#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

bool shaderFileInList(const char *pathname, int *idx);
size_t freadReplace(void *buf, size_t size, size_t count, int idx);
long int ftellGetShaderSize(int idx);
void *vt3Open(void *fb, const char *s, int mode);
int vt3Close(void *fb);
uint32_t *vt3Tellg(uint32_t *size, void *is);
void *vt3Read(void *is, char *s, long long n);