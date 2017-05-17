/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __CORE_STRING_H
#define __CORE_STRING_H

#include <core/types.h>

#define USE_BUILTIN_STRING

#ifndef __STRING_INLINE
#  ifndef __extern_inline
#    define __STRING_INLINE inline                                                                                 
#  else
#    define __STRING_INLINE __extern_inline
#  endif
#endif
static inline void *
memset_slow (void *addr, int val, int len)
{
	char *p;

	p = addr;
	while (len--)
		*p++ = val;
	return addr;
};

static inline void *
memcpy_slow (void *dest, void const *src, int len)
{
	char *p;
    char const *q;

	p = dest;
	q = src;
	while (len--)
		*p++ = *q++;
	return dest;
};

static inline int
strcmp_slow (char const *s1, char const *s2)
{
	int r, c1, c2;

	do {
		c1 = *s1++;
		c2 = *s2++;
		r = c1 - c2;
	} while (!r && c1);
	return r;
};

static inline int
memcmp_slow (void const *p1, void const *p2, int len)
{
	int r, i;
	char const *q1, *q2;

	q1 = p1;
	q2 = p2;
	for (r = 0, i = 0; !r && i < len; i++)
		r = *q1++ - *q2++;
	return r;
};

static inline int
strlen_slow (char const *p)
{
	int len = 0;

	while (*p++)
		len++;
	return len;
};
static inline void *
memchr_slow(const void *ptr, int ch, size_t count)
{
    char const *p = ptr;
    char const *end = p +count;
    for(p = ptr ; p < end ; p++){
        if(*p == ch){return (void*)p;}
    }
};
__STRING_INLINE void *__memmove_g (void *, const void *, size_t)
    __asm__ ("memmove");

__STRING_INLINE void *
__memmove_g (void *__dest, const void *__src, size_t __n)
{
    register unsigned long int __d0, __d1, __d2;
    register void *__tmp = __dest;
    if (__dest < __src)
        __asm__ __volatile__
        ("cld\n\t"
         "rep; movsb"
          : "=&c" (__d0), "=&S" (__d1), "=&D" (__d2),
        "=m" ( *(struct { __extension__ char __x[__n]; } *)__dest)
          : "0" (__n), "1" (__src), "2" (__tmp),
        "m" ( *(struct { __extension__ char __x[__n]; } *)__src));
     else
        __asm__ __volatile__
        ("decl %1\n\t"
         "decl %2\n\t"
         "std\n\t"
         "rep; movsb\n\t"
         "cld"
         : "=&c" (__d0), "=&S" (__d1), "=&D" (__d2),
        "=m" ( *(struct { __extension__ char __x[__n]; } *)__dest)
         : "0" (__n), "1" (__n + (const char *) __src),
        "2" (__n + (char *) __tmp),
        "m" ( *(struct { __extension__ char __x[__n]; } *)__src));
      return __dest;
};

__STRING_INLINE char *__strchr_c (const char *__s, int __c);

__STRING_INLINE char *
__strchr_c (const char *__s, int __c)
{
    register unsigned long int __d0;
    register char *__res;
    __asm__ __volatile__
        ("1:\n\t"
        "movb  (%0),%%al\n\t"
        "cmpb  %%ah,%%al\n\t"
        "je    2f\n\t"
        "leal  1(%0),%0\n\t"
        "testb %%al,%%al\n\t"
        "jne   1b\n\t"
        "xorl  %0,%0\n"
        "2:"
        : "=r" (__res), "=&a" (__d0)
        : "0" (__s), "1" (__c),
          "m" ( *(struct { char __x[0xfffffff]; } *)__s)
        : "cc");
    return __res;
};
 
__STRING_INLINE char *__strchr_g (const char *__s, int __c);

__STRING_INLINE char *
__strchr_g (const char *__s, int __c)
{
    register unsigned long int __d0;
    register char *__res;
    __asm__ __volatile__
        ("movb  %%al,%%ah\n"
        "1:\n\t"
        "movb  (%0),%%al\n\t"
        "cmpb  %%ah,%%al\n\t"
        "je    2f\n\t"
        "leal  1(%0),%0\n\t"
        "testb %%al,%%al\n\t"
        "jne   1b\n\t"
        "xorl  %0,%0\n"
        "2:"
        : "=r" (__res), "=&a" (__d0)
        : "0" (__s), "1" (__c),
          "m" ( *(struct { char __x[0xfffffff]; } *)__s)
        : "cc");
    return __res;
};

__STRING_INLINE void *__rawmemchr (const void *__s, int __c);
__STRING_INLINE void *
__rawmemchr (const void *__s, int __c)
{
    register unsigned long int __d0;
    register unsigned char *__res;
    __asm__ __volatile__
    ("cld\n\t"
    "repne; scasb\n\t"
    : "=D" (__res), "=&c" (__d0)
    : "a" (__c), "0" (__s), "1" (0xffffffff),
      "m" ( *(struct { char __x[0xfffffff]; } *)__s)
    : "cc");
    return __res - 1;
};

__STRING_INLINE int __strncmp_g (const char *__s1, const char *__s2, size_t __n);
__STRING_INLINE int
__strncmp_g (const char *__s1, const char *__s2, size_t __n)
{
    register int __res;
    __asm__ __volatile__
        ("1:\n\t"
         "subl  $1,%3\n\t"
         "jc    2f\n\t"
         "movb  (%1),%b0\n\t"
         "incl  %1\n\t"
         "cmpb  %b0,(%2)\n\t"
         "jne   3f\n\t"
         "incl  %2\n\t"
         "testb %b0,%b0\n\t"
         "jne   1b\n"
         "2:\n\t"
         "xorl  %0,%0\n\t"
         "jmp   4f\n"
         "3:\n\t"
         "movl  $1,%0\n\t"
         "jb    4f\n\t"
         "negl  %0\n"
         "4:"
         : "=q" (__res), "=&r" (__s1), "=&r" (__s2), "=&r" (__n)
         : "1"  (__s1), "2"  (__s2),  "3" (__n),
             "m" ( *(struct { __extension__ char __x[__n]; } *)__s1),
             "m" ( *(struct { __extension__ char __x[__n]; } *)__s2)
         : "cc");
    return __res;
}

#ifdef USE_BUILTIN_STRING
#	define memset(addr, val, len)	memset_builtin (addr, val, len)
#	define memcpy(dest, src, len)	memcpy_builtin (dest, src, len)
#	define strcmp(s1, s2)		strcmp_builtin (s1, s2)
#	define memcmp(p1, p2, len)	memcmp_builtin (p1, p2, len)
#	define strlen(p)		strlen_builtin (p)
#   define memchr(p, c, count) memchr_builtin(p, c, count)
#else  /* USE_BUILTIN_STRING */
#	define memset(addr, val, len)	memset_slow (addr, val, len)
#	define memcpy(dest, src, len)	memcpy_slow (dest, src, len)
#	define strcmp(s1, s2)		strcmp_slow (s1, s2)
#	define memcmp(p1, p2, len)	memcmp_slow (p1, p2, len)
#	define strlen(p)		strlen_slow (p)
#   define memchr(p, c, count) memchr_slow(p, c, count)
#endif /* USE_BUILTIN_STRING */

#define strchr(s, c) \
(__extension__ (__builtin_constant_p (c)\
? ((c) == '\0'                          \
? (char *) __rawmemchr ((s), (c))                \
: __strchr_c ((s), ((c) & 0xff) << 8))           \
: __strchr_g ((s), (c))))

 # define strncmp(s1, s2, n) \
 (__extension__ (__builtin_constant_p (s1) && strlen (s1) < ((size_t) (n))   \
 ? strcmp ((s1), (s2))                       \
 : (__builtin_constant_p (s2) && strlen (s2) < ((size_t) (n))\
 ? strcmp ((s1), (s2))                    \
 : __strncmp_g ((s1), (s2), (n)))))


#define memmove(dest, src, n) __memmove_g (dest, src, n)  

#ifdef USE_BUILTIN_STRING
static inline void *
memset_builtin (void *addr, int val, int len)
{
	return __builtin_memset (addr, val, len);
};

static inline void *
memcpy_builtin (void *dest, void const *src, int len)
{
	return __builtin_memcpy (dest, src, len);
};

static inline int
strcmp_builtin (char const *s1, char const *s2)
{
	return __builtin_strcmp (s1, s2);
};

static inline int
memcmp_builtin (void const *p1, void const *p2, int len)
{
	return __builtin_memcmp (p1, p2, len);
};

static inline int
strlen_builtin (char const *p)
{
	return __builtin_strlen (p);
};

static inline void*
memchr_builtin(void const *p, int chr, int count)
{
    return __builtin_memchr(p, chr, count);
};
#endif /* USE_BUILTIN_STRING */

#endif
