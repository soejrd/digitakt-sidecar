/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef SC_MEMORY_H
#define SC_MEMORY_H
/* Freestanding subset used by both firmware and hosted builds. */
static inline void sc_zero(void *p, unsigned n)
{
    unsigned char *d=p; while(n--) *d++=0;
}
static inline int sc_compare(const void *a, const void *b, unsigned n)
{
    const unsigned char *x=a,*y=b;
    while(n--) { if(*x!=*y) return *x-*y; ++x; ++y; } return 0;
}
#endif
