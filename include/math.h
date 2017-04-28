#ifndef _BITVISOR_MATH_H
#define _BITVISOR_MATH_H

#define signbit(a) __builtin_signbit(a)
#define frexp(a, b) __builtin_frexp(a, b)
#define isfinite(a) __builtin_fisfinite(a)
#define pow(a, b) __builtin_pow(a, b)
#define isnan(a) __builtin_isnan(a)
#define isinf(a) __builtin_isinf(a)
#define fmod(a, b) __builtin_fmod(a, b)
#define floor(a) __builtin_floor(a)
#define ceil(a) __builtin_ceil(a)

#define MAN __builtin_nan("")
#define INFINITY __builtin_huge_valf()

#endif
