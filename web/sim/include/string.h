/* SPDX-License-Identifier: GPL-3.0-only */
/* The simulator built by clang --target=wasm32 without a libc (web/sim/build.sh): the three routines tests/panel.h
 * and the compiler's struct copies need (panel.h renames firmware/src/libc.c's own) */
#pragma once
typedef __SIZE_TYPE__ size_t;
void *memset(void *d, int c, size_t n)
{
    unsigned char *p = d;
    while (n--)
        *p++ = (unsigned char)c;
    return d;
}
void *memcpy(void *d, const void *s, size_t n)
{
    unsigned char *p = d;
    const unsigned char *q = s;
    while (n--)
        *p++ = *q++;
    return d;
}
int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = a, *q = b;
    for (; n; n--, p++, q++)
        if (*p != *q)
            return *p - *q;
    return 0;
}
