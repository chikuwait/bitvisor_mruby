/*

Most code in this file originates from musl (src/stdio/vfprintf.c)
which, just like mruby itself, is licensed under the MIT license.

Copyright (c) 2005-2014 Rich Felker, et al.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*/

//#include <limits.h>
#if BITVISOR_PROCESS
  #include <lib_string.h>
#else
  #include <string.h>
#endif
//#include <stdint.h>
#include <float.h>
//#include <ctype.h>
#ifdef MRB_DISABLE_STDIO
#include <printf.h>
#endif

#include <mruby.h>
#include <mruby/string.h>
#include <mruby/numeric.h>
#include <bitvisor/softfloat.h>
#ifndef MRB_WITHOUT_FLOAT
struct fmt_args {
  mrb_state *mrb;
  mrb_value str;
};

#define MAX(a,b) ((a)>(b) ? (a) : (b))
#define MIN(a,b) ((a)<(b) ? (a) : (b))

/* Convenient bit representation for modifier flags, which all fall
 * within 31 codepoints of the space character. */

#define ALT_FORM   (1U<<('#'-' '))
#define ZERO_PAD   (1U<<('0'-' '))
#define LEFT_ADJ   (1U<<('-'-' '))
#define PAD_POS    (1U<<(' '-' '))
#define MARK_POS   (1U<<('+'-' '))
#define extF80_signbit(f)((f.signExp>>15))
#define extF80_isnan(a) (((~(a.signExp) & UINT64_C(0x7FFF)) == 0) && ((a.signif) & UINT64_C(0xFFFFFFFFFFFFFFFF)))
#define extF80_isinf(a) (((a.signExp & 0x7FFF)==0x7FFF) && (a.signif == 0))
#define extF80_to_i64(x) extF80_to_i64((x),softfloat_round_min,1)
#define f64_to_i64(x) f64_to_i64((x),softfloat_round_min,1)
#define __HI(x) *(1+(int *)&x.v)
#define __LO(x) *(int *)&x.v
static float64_t two54 =  {0x40000000000000}; /*  0x43500000, 0x00000000 */
float64_t frexp(float64_t x, int *eptr)
{
    int  hx, ix, lx;
    hx = __HI(x);
    ix = 0x7fffffff&hx;
    lx = __LO(x);
    *eptr = 0;
    if(ix>=0x7ff00000||((ix|lx)==0)) return x;  /*  0,inf,nan */
    if (ix<0x00100000) {        /*  subnormal */
        x =f64_mul(x,two54);
        hx = __HI(x);
        ix = hx&0x7fffffff;
        *eptr = -54;
    }
    *eptr += (ix>>20)-1022;
    hx = (hx&0x800fffff)|0x3fe00000;
    __HI(x) = hx;
    return x;
}
int
extF80_isfinite(extFloat80_t f)
{
    if(extF80_isinf(f) || extF80_isnan(f)) return 0;
    return 1;
}


static void
out(struct fmt_args *f, const char *s, size_t l)
{
  mrb_str_cat(f->mrb, f->str, s, l);
}

#define PAD_SIZE 256
static void
pad(struct fmt_args *f, char c, ptrdiff_t w, ptrdiff_t l, uint8_t fl)
{
  char pad[PAD_SIZE];
  if (fl & (LEFT_ADJ | ZERO_PAD) || l >= w) return;
  l = w - l;
  memset(pad, c, l>PAD_SIZE ? PAD_SIZE : l);
  for (; l >= PAD_SIZE; l -= PAD_SIZE)
    out(f, pad, PAD_SIZE);
  out(f, pad, l);
}

static const char xdigits[16] = {
  '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

static char*
fmt_u(uint32_t x, char *s)
{
  for (; x; x /= 10) *--s = '0' + x % 10;
  return s;
}

/* Do not override this check. The floating point printing code below
 * depends on the float.h constants being right. If they are wrong, it
 * may overflow the stack. */
#if LDBL_MANT_DIG == 53
typedef char compiler_defines_long_double_incorrectly[9-(int)sizeof(long double)];
#endif

static int
fmt_fp(struct fmt_args *f, extFloat80_t y, ptrdiff_t p, uint8_t fl, int t)
{
  uint32_t big[(LDBL_MANT_DIG+28)/29 + 1          // mantissa expansion
    + (LDBL_MAX_EXP+LDBL_MANT_DIG+28+8)/9]; // exponent expansion
  uint32_t *a, *d, *r, *z;
  uint32_t i;
  int e2=0, e, j;
  ptrdiff_t l;
  char buf[9+LDBL_MANT_DIG/4], *s;
  const char *prefix="-0X+0X 0X-0x+0x 0x";
  ptrdiff_t pl;
  char ebuf0[3*sizeof(int)], *ebuf=&ebuf0[3*sizeof(int)], *estr;

  pl=1;
  if (extF80_signbit(y)) {
    y = extF80_mul(y,i64_to_extF80(-1));
  } else if (fl & MARK_POS) {
    prefix+=3;
  } else if (fl & PAD_POS) {
    prefix+=6;
  } else prefix++, pl=0;
  if (!extF80_isfinite(y)) {
    const char *ss = (t&32)?"inf":"INF";
    if (!extF80_eq(y,y)) ss=(t&32)?"nan":"NAN";
    pad(f, ' ', 0, 3+pl, fl&~ZERO_PAD);
    out(f, prefix, pl);
    out(f, ss, 3);
    pad(f, ' ', 0, 3+pl, fl^LEFT_ADJ);
    return 3+(int)pl;
  }

  y = f64_to_extF80(f64_mul(frexp(extF80_to_f64(y), &e2),i64_to_f64(2)));
  if (!extF80_eq(y,i64_to_extF80(0))) e2--;
  if ((t|32)=='a') {
    extFloat80_t round = i64_to_extF80(8);
    ptrdiff_t re;

    if (t&32) prefix += 9;
    pl += 2;

    if (p<0 || p>=LDBL_MANT_DIG/4-1) re=0;
    else re=LDBL_MANT_DIG/4-1-p;

    if (re) {
      while (re--) round = extF80_mul(round,i64_to_extF80(16));
      if (*prefix=='-') {
        y=extF80_mul(y,i64_to_extF80(-1));
        y=extF80_sub(y,round);
        y=extF80_add(y,round);
        y=extF80_sub(y,y);
      }
      else {
        y=extF80_add(y,round);
        y=extF80_sub(y,round);
      }
    }

    estr=fmt_u(e2<0 ? -e2 : e2, ebuf);
    if (estr==ebuf) *--estr='0';
    *--estr = (e2<0 ? '-' : '+');
    *--estr = t+('p'-'a');

    s=buf;
    do {
      int x=extF80_to_i64(y);
      *s++=xdigits[x]|(t&32);
      y=extF80_mul(i64_to_extF80(16),extF80_sub(y,i64_to_extF80(x)));
      if (s-buf==1 && (!extF80_eq(y,i64_to_extF80(0))||p>0||(fl&ALT_FORM))) *s++='.';
    } while (!extF80_eq(y,i64_to_extF80(0)));

    if (p && s-buf-2 < p)
      l = (p+2) + (ebuf-estr);
    else
      l = (s-buf) + (ebuf-estr);

    pad(f, ' ', 0, pl+l, fl);
    out(f, prefix, pl);
    pad(f, '0', 0, pl+l, fl^ZERO_PAD);
    out(f, buf, s-buf);
    pad(f, '0', l-(ebuf-estr)-(s-buf), 0, 0);
    out(f, estr, ebuf-estr);
    pad(f, ' ', 0, pl+l, fl^LEFT_ADJ);
    return (int)pl+(int)l;
  }
  if (p<0) p=6;

  if (!extF80_eq(y,i64_to_extF80(0))) y =extF80_mul(y,i64_to_extF80(268435456)), e2-=28;

  if (e2<0) a=r=z=big;
  else a=r=z=big+sizeof(big)/sizeof(*big) - LDBL_MANT_DIG - 1;

  do {
    *z = extF80_to_ui32(y,softfloat_round_min,1);
     y = extF80_mul(i64_to_extF80(1000000000),extF80_sub(y,i32_to_extF80(*z++)));
  } while (!extF80_eq(y,i64_to_extF80(0)));

  while (e2>0) {
    uint32_t carry=0;
    int sh=MIN(29,e2);
    for (d=z-1; d>=a; d--) {
      uint64_t x = ((uint64_t)*d<<sh)+carry;
      *d = x % 1000000000;
      carry = (uint32_t)(x / 1000000000);
    }
    if (carry) *--a = carry;
    while (z>a && !z[-1]) z--;
    e2-=sh;
  }
  while (e2<0) {
    uint32_t carry=0, *b;
    int sh=MIN(9,-e2), need=1+((int)p+LDBL_MANT_DIG/3+8)/9;
    for (d=a; d<z; d++) {
      uint32_t rm = *d & ((1<<sh)-1);
      *d = (*d>>sh) + carry;
      carry = (1000000000>>sh) * rm;
    }
    if (!*a) a++;
    if (carry) *z++ = carry;
    /* Avoid (slow!) computation past requested precision */
    b = (t|32)=='f' ? r : a;
    if (z-b > need) z = b+need;
    e2+=sh;
  }

  if (a<z) for (i=10, e=9*(int)(r-a); *a>=i; i*=10, e++);
  else e=0;

  /* Perform rounding: j is precision after the radix (possibly neg) */
  j = (int)p - ((t|32)!='f')*e - ((t|32)=='g' && p);
  if (j < 9*(z-r-1)) {
    uint32_t x;
    /* We avoid C's broken division of negative numbers */
    d = r + 1 + ((j+9*LDBL_MAX_EXP)/9 - LDBL_MAX_EXP);
    j += 9*LDBL_MAX_EXP;
    j %= 9;
    for (i=10, j++; j<9; i*=10, j++);
    x = *d % i;
    /* Are there any significant digits past j? */
    if (x || d+1!=z) {
      float64_t small1 ={0x3FE0000000000000};
      float64_t small2 ={0xFF8000000000000};
      float64_t epsilon={0x3C00000000000000};
      extFloat80_t round = extF80_div(i64_to_extF80(2),f64_to_extF80(epsilon));
      extFloat80_t small;
      if (*d/i & 1) round =extF80_add(round,i64_to_extF80(2));
      if (x<i/2) small=f64_to_extF80(small1);
      else if (x==i/2 && d+1==z) small=i64_to_extF80(1.0);
      else small=f64_to_extF80(small2);
      if (pl && *prefix=='-') round=extF80_mul(round,i64_to_extF80(-1)), small=extF80_mul(small,i64_to_extF80(-1));
      *d -= x;
      /* Decide whether to round by probing round+small */
      if (!extF80_eq(extF80_add(round,small),round)){
        *d = *d + i;
        while (*d > 999999999) {
          *d--=0;
          if (d<a) *--a=0;
          (*d)++;
        }
        for (i=10, e=9*(int)(r-a); *a>=i; i*=10, e++);
      }
    }
    if (z>d+1) z=d+1;
  }
  for (; z>a && !z[-1]; z--);

  if ((t|32)=='g') {
    if (!p) p++;
    if (p>e && e>=-4) {
      t--;
      p-=e+1;
    }
    else {
      t-=2;
      p--;
    }
    if (!(fl&ALT_FORM)) {
      /* Count trailing zeros in last place */
      if (z>a && z[-1]) for (i=10, j=0; z[-1]%i==0; i*=10, j++);
      else j=9;
      if ((t|32)=='f')
        p = MIN(p,MAX(0,9*(z-r-1)-j));
      else
        p = MIN(p,MAX(0,9*(z-r-1)+e-j));
    }
  }
  l = 1 + p + (p || (fl&ALT_FORM));
  if ((t|32)=='f') {
    if (e>0) l+=e;
  }
  else {
    estr=fmt_u(e<0 ? -e : e, ebuf);
    while(ebuf-estr<2) *--estr='0';
    *--estr = (e<0 ? '-' : '+');
    *--estr = t;
    l += ebuf-estr;
  }

  pad(f, ' ', 0, pl+l, fl);
  out(f, prefix, pl);
  pad(f, '0', 0, pl+l, fl^ZERO_PAD);

  if ((t|32)=='f') {
    if (a>r) a=r;
    for (d=a; d<=r; d++) {
      char *ss = fmt_u(*d, buf+9);
      if (d!=a) while (ss>buf) *--ss='0';
      else if (ss==buf+9) *--ss='0';
      out(f, ss, buf+9-ss);
    }
    if (p || (fl&ALT_FORM)) out(f, ".", 1);
    for (; d<z && p>0; d++, p-=9) {
      char *ss = fmt_u(*d, buf+9);
      while (ss>buf) *--ss='0';
      out(f, ss, MIN(9,p));
    }
    pad(f, '0', p+9, 9, 0);
  }
  else {
    if (z<=a) z=a+1;
    for (d=a; d<z && p>=0; d++) {
      char *ss = fmt_u(*d, buf+9);
      if (ss==buf+9) *--ss='0';
      if (d!=a) while (ss>buf) *--ss='0';
      else {
        out(f, ss++, 1);
        if (p>0||(fl&ALT_FORM)) out(f, ".", 1);
      }
      out(f, ss, MIN(buf+9-ss, p));
      p -= (int)(buf+9-ss);
    }
    pad(f, '0', p+18, 18, 0);
    out(f, estr, ebuf-estr);
  }

  pad(f, ' ', 0, pl+l, fl^LEFT_ADJ);

  return (int)pl+(int)l;
}

static int
fmt_core(struct fmt_args *f, const char *fmt, mrb_float flo)
{
  ptrdiff_t p;
  if (*fmt != '%') {
    return -1;
  }
  ++fmt;
  if (*fmt == '.') {
    ++fmt;
    for (p = 0; ISDIGIT(*fmt); ++fmt) {
      p = 10 * p + (*fmt - '0');
    }
  }
  else {
    p = -1;
  }
  switch (*fmt) {
  case 'e': case 'f': case 'g': case 'a':
  case 'E': case 'F': case 'G': case 'A':
    return fmt_fp(f, f64_to_extF80(flo), p, 0, *fmt);
  default:
    return -1;
  }
}

mrb_value
mrb_float_to_str(mrb_state *mrb, mrb_value flo, const char *fmt)
{
    float64_t n = mrb_float(flo);
    int max_digits = FLO_MAX_DIGITS;
    if(f64_isnan(n)){
        return mrb_str_new_lit(mrb, "NaN");
    }
    else if(f64_isinf(n)){
        if (f64_lt(n,i64_to_f64(0))){
            return mrb_str_new_lit(mrb, "-inf");
        }
        else {
            return mrb_str_new_lit(mrb, "inf");
        }
    }
    else {
        int digit;
        int m = 0;
        int exp;
        mrb_bool e = FALSE;
        char s[48];
        char *c = &s[0];
        int length = 0;

        if (f64_signbit(n)) {
            n = f64_mul(n, i64_to_f64(-1));
            *(c++) = '-';
        }
        if (!f64_eq(n,i64_to_f64(0))) {
            if(!f64_lt(i64_to_f64(1),n)){
                exp = (int)f64_to_i64(f64_floor(f64_log10(n)));
            }
            else {
                exp = (int)f64_to_i64(f64_mul(f64_ceil(f64_mul(f64_log10(n),i64_to_f64(-1))),i64_to_f64(-1)));
            }
        }
        else {
            exp = 0;
        }

        /*  preserve significands */
        if (exp < 0) {
            int i, beg = -1, end = 0;
            float64_t f = n;
            float64_t fd = {0};

            for (i = 0; i < FLO_MAX_DIGITS; ++i) {
                f = f64_mul(f64_sub(f,fd),i64_to_f64(10));
                fd = f64_floor(f64_add(f,FLO_EPSILON));
                if (!f64_eq(fd,i64_to_f64(0))) {
                    if (beg < 0) beg = i;
                    end = i + 1;
                }
            }
            if (beg >= 0) length = end - beg;
            if (length > FLO_MAX_SIGN_LENGTH) length = FLO_MAX_SIGN_LENGTH;
        }

        if (abs(exp) + length >= FLO_MAX_DIGITS) {
            /*  exponent representation */
            e = TRUE;
            n = f64_div(n,f64_pow(i64_to_f64(10),i64_to_f64(exp)));
            if (f64_isinf(n)) {
                if (s < c) {            /*  s[0] == '-' */
                    return mrb_str_new_lit(mrb, "-0.0");
                }
                else {
                    return mrb_str_new_lit(mrb, "0.0");
                }
            }
        }
        else {
            /*  un-exponent (normal) representation */
            if (exp > 0) {
                m = exp;
            }
        }

        /*  puts digits */
        while (max_digits >= 0) {
            float64_t weight = (m < 0) ? i64_to_f64(0) : f64_pow(i64_to_f64(10),i64_to_f64(m));
            float64_t fdigit = (m < 0) ? f64_mul(n,i64_to_f64(10)) : f64_div(n,weight);
            if(f64_lt(fdigit,i64_to_f64(0))) fdigit = n = i64_to_f64(0);
            if(m < -1 && f64_lt(fdigit,FLO_EPSILON)){
                if (e || exp > 0 || m <= -abs(exp)) {
                    break;
                }
            }
            digit = f64_to_i64(f64_floor(f64_add(fdigit,FLO_EPSILON)));
            if (m == 0 && digit > 9) {
                n = f64_div(n,i64_to_f64(10));
                exp++;
                continue;
            }
            *(c++) = '0' + digit;
            n = (m < 0) ? f64_sub(f64_mul(n,i64_to_f64(10)),i64_to_f64(digit)) : f64_sub(n,f64_mul(i64_to_f64(digit),weight));
            max_digits--;
            if (m-- == 0) {
                *(c++) = '.';
            }
        }
        if (c[-1] == '0') {
            while (&s[0] < c && c[-1] == '0') {
                c--;
            }
            c++;
        }

        if (e) {
            *(c++) = 'e';
            if (exp > 0) {
                *(c++) = '+';
            }
            else {
                *(c++) = '-';
                exp = -exp;
            }

            if (exp >= 100) {
                *(c++) = '0' + exp / 100;
                exp -= exp / 100 * 100;
            }

            *(c++) = '0' + exp / 10;
            *(c++) = '0' + exp % 10;
        }

        *c = '\0';

        return mrb_str_new(mrb, &s[0], c - &s[0]);
    }
}
#endif
