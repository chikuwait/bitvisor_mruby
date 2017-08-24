/*
** numeric.c - Numeric, Integer, Float, Fixnum class
**
** See Copyright Notice in mruby.h
*/

#include <float.h>
#include <limits.h>
//#include <math.h>
#include <stdlib.h>
#include <bitvisor/softfloat.h>
#include "mruby.h"
#include "mruby/array.h"
#include "mruby/numeric.h"
#include "mruby/string.h"

#ifdef MRB_USE_FLOAT
#define floor(f) floorf(f)
#define ceil(f) ceilf(f)
#define fmod(x,y) fmodf(x,y)
#define FLO_MAX_DIGITS 7
#define FLO_MAX_SIGN_LENGTH 3
//#define FLO_EPSILON FLT_EPSILON
#else
#define FLO_MAX_DIGITS 14
#define FLO_MAX_SIGN_LENGTH 10
//#define FLO_EPSILON DBL_EPSILON
#endif
#define f64_to_i64(x) f64_to_i64((x), softfloat_round_near_even, 1)
#define f64_signbit(f)((f.v>>63))
static float64_t NAN = {0xFFFFFFFFFFFFFFFF};
static float64_t INFINITY = {0x7FF0000000000000};
static float64_t FLO_EPSILON = {0x3E112E0BE826D695};

static int
f64_isnan(float64_t f)
{
    return f64_isSignalingNaN(f);
}
static int
f64_isinf (float64_t f) {
  if (((f.v>>52) & 0x07FF) != 0x07FF){
    return 0;
  }
  if ((f.v<<12) != 0){
    return 0;
  }
  return 1;
}
static int
f64_isfinite(float64_t f)
{
    if(f64_isinf(f))
        return 0;
    if(f64_isnan(f))
        return 0;
    return 1;
}

static float64_t
f64_floor (float64_t f) {
    return f64_roundToInt(f,softfloat_round_min,0);
}

float64_t
f64_log(float64_t arg)
{
    float64_t integral = {0};
    int n = 50000;
    float64_t dx = f64_div(f64_sub(arg,ui32_to_f64(1)),ui32_to_f64(n));
    if(f64_lt(arg,ui32_to_f64(1))) return i32_to_f64(-1);

    for(int i = 1 ; i-1 < n;i++){
        integral = f64_add(integral,f64_div(ui32_to_f64(1),f64_add(f64_mul(ui32_to_f64(i),dx),ui32_to_f64(1))));
    }
    integral = f64_add(integral,f64_div(ui32_to_f64(1),ui32_to_f64(2)));
    integral = f64_add(integral,f64_div(ui32_to_f64(1),arg));
    integral = f64_mul(integral,dx);
    return integral;
}

float64_t
f64_log10(float64_t arg)
{

    float64_t ex = f64_log(arg);
    float64_t e10 = f64_log(i32_to_f64(10));
    return f64_div(ex,e10);
}
static float64_t
f64_ceil (float64_t f) {
  return f64_roundToInt(f,softfloat_round_max,0);
}

float64_t
f64_pow(float64_t a,float64_t b)
{
    int n = f64_to_i64(b);
    float64_t buf = {a.v};
    if(b.v == 0 ||a.v == 0x3FF0000000000000){
        buf.v =0x3FF0000000000000;
        return buf;
    }
    if(a.v == 0){
        buf.v = 0;
        return buf;
    }
    for(int m = 1; m < n; m++){
        buf = f64_mul(buf,a);
    }
    return buf;
}

MRB_API mrb_float
mrb_to_flo(mrb_state *mrb, mrb_value val)
{
  switch (mrb_type(val)) {
  case MRB_TT_FIXNUM:
   // return (mrb_float)mrb_fixnum(val);
   return (i64_to_f64(mrb_fixnum(val)));
  case MRB_TT_FLOAT:
    break;
  default:
    mrb_raise(mrb, E_TYPE_ERROR, "non float value");
  }
  return mrb_float(val);
}

/*
 * call-seq:
 *
 *  num ** other  ->  num
 *
 * Raises <code>num</code> the <code>other</code> power.
 *
 *    2.0**3      #=> 8.0
 */
static mrb_value
num_pow(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_float d, yv;

  mrb_get_args(mrb, "o", &y);
  yv = mrb_to_flo(mrb, y);
  d = f64_pow(mrb_to_flo(mrb, x), yv);
  if (mrb_fixnum_p(x) && mrb_fixnum_p(y) && FIXABLE(d) && f64_lt(i64_to_f64(0),yv))
//    return mrb_fixnum_value((mrb_int)d);
     return mrb_fixnum_value(f64_to_i64(d));
  return mrb_float_value(mrb, d);
}

/* 15.2.8.3.4  */
/* 15.2.9.3.4  */
/*
 * call-seq:
 *   num / other  ->  num
 *
 * Performs division: the class of the resulting object depends on
 * the class of <code>num</code> and on the magnitude of the
 * result.
 */

mrb_value
mrb_num_div(mrb_state *mrb, mrb_value x, mrb_value y)
{
  return mrb_float_value(mrb, f64_div(mrb_to_flo(mrb, x),mrb_to_flo(mrb, y)));
}

/* 15.2.9.3.19(x) */
/*
 *  call-seq:
 *     num.quo(numeric)  ->  real
 *
 *  Returns most exact division.
 */

static mrb_value
num_div(mrb_state *mrb, mrb_value x)
{
  mrb_float y;

  mrb_get_args(mrb, "f", &y);
  return mrb_float_value(mrb, f64_div(mrb_to_flo(mrb, x),y));
}

/********************************************************************
 *
 * Document-class: Float
 *
 *  <code>Float</code> objects represent inexact real numbers using
 *  the native architecture's double-precision floating point
 *  representation.
 */
#define f64_signbit(f)((f.v>>63))

static mrb_value
mrb_flo_to_str(mrb_state *mrb, mrb_float flo)
{
  float64_t n = flo;
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

    /* preserve significands */
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
      /* exponent representation */
      e = TRUE;
      n = f64_div(n,f64_pow(i64_to_f64(10),i64_to_f64(exp)));
      if (f64_isinf(n)) {
        if (s < c) {            /* s[0] == '-' */
          return mrb_str_new_lit(mrb, "-0.0");
        }
        else {
          return mrb_str_new_lit(mrb, "0.0");
        }
      }
    }
    else {
      /* un-exponent (normal) representation */
      if (exp > 0) {
        m = exp;
      }
    }

    /* puts digits */
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

/* 15.2.9.3.16(x) */
/*
 *  call-seq:
 *     flt.to_s  ->  string
 *
 *  Returns a string containing a representation of self. As well as a
 *  fixed or exponential form of the number, the call may return
 *  ``<code>NaN</code>'', ``<code>Infinity</code>'', and
 *  ``<code>-Infinity</code>''.
 */

static mrb_value
flo_to_s(mrb_state *mrb, mrb_value flt)
{
  return mrb_flo_to_str(mrb, mrb_float(flt));
}

/* 15.2.9.3.2  */
/*
 * call-seq:
 *   float - other  ->  float
 *
 * Returns a new float which is the difference of <code>float</code>
 * and <code>other</code>.
 */

static mrb_value
flo_minus(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);
  return mrb_float_value(mrb, f64_sub(mrb_float(x),mrb_to_flo(mrb, y)));
}

/* 15.2.9.3.3  */
/*
 * call-seq:
 *   float * other  ->  float
 *
 * Returns a new float which is the product of <code>float</code>
 * and <code>other</code>.
 */

static mrb_value
flo_mul(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);
  return mrb_float_value(mrb, f64_mul(mrb_float(x) ,mrb_to_flo(mrb, y)));
}
static void
flodivmod(mrb_state *mrb, mrb_float x, mrb_float y, mrb_float *divp, mrb_float *modp)
{
  mrb_float div;
  mrb_float mod;

//  if (y == (mrb_float)0.0) {
    if(f64_eq(y,i64_to_f64(0))){
    div = INFINITY;
    mod = NAN;
  }
  else {
    mod = f64_rem(x, y);
    if (f64_isinf(x) && f64_isfinite(y))
      div = x;
    else
        div = f64_div(f64_sub(x,mod),y);
    //  div = (x - mod) / y;
    if (f64_lt(f64_mul(y,mod),i64_to_f64(0))){
        mod =  f64_add(mod,y);
      //mod += y;
     // div -= (mrb_float)1.0;
        div = f64_sub(div,i64_to_f64(1));
    }
  }

  if (modp) *modp = mod;
  if (divp) *divp = div;
}
/* 15.2.9.3.5  */
/*
 *  call-seq:
 *     flt % other        ->  float
 *     flt.modulo(other)  ->  float
 *
 *  Return the modulo after division of <code>flt</code> by <code>other</code>.
 *
 *     6543.21.modulo(137)      #=> 104.21
 *     6543.21.modulo(137.24)   #=> 92.9299999999996
 */

static mrb_value
flo_mod(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_float mod;

  mrb_get_args(mrb, "o", &y);

  flodivmod(mrb, mrb_float(x), mrb_to_flo(mrb, y), 0, &mod);
  return mrb_float_value(mrb, mod);
}
/* 15.2.8.3.16 */
/*
 *  call-seq:
 *     num.eql?(numeric)  ->  true or false
 *
 *  Returns <code>true</code> if <i>num</i> and <i>numeric</i> are the
 *  same type and have equal values.
 *
 *     1 == 1.0          #=> true
 *     1.eql?(1.0)       #=> false
 *     (1.0).eql?(1.0)   #=> true
 */
static mrb_value
fix_eql(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);
  if (!mrb_fixnum_p(y)) return mrb_false_value();
  return mrb_bool_value(mrb_fixnum(x) == mrb_fixnum(y));
}

static mrb_value
flo_eql(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);
  if (!mrb_float_p(y)) return mrb_false_value();
  //return mrb_bool_value(mrb_float(x) == (mrb_float)mrb_fixnum(y));
  return mrb_bool_value(f64_eq(mrb_float(x),i64_to_f64(mrb_fixnum(y))));
}

/* 15.2.9.3.7  */
/*
 *  call-seq:
 *     flt == obj  ->  true or false
 *
 *  Returns <code>true</code> only if <i>obj</i> has the same value
 *  as <i>flt</i>. Contrast this with <code>Float#eql?</code>, which
 *  requires <i>obj</i> to be a <code>Float</code>.
 *
 *     1.0 == 1   #=> true
 *
 */

static mrb_value
flo_eq(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_get_args(mrb, "o", &y);

  switch (mrb_type(y)) {
  case MRB_TT_FIXNUM:
    //return mrb_bool_value(mrb_float(x) == (mrb_float)mrb_fixnum(y));
    return mrb_bool_value(f64_eq(mrb_float(x),i64_to_f64(mrb_fixnum(x))));
  case MRB_TT_FLOAT:
    //return mrb_bool_value(mrb_float(x) == mrb_float(y));
    return mrb_bool_value(f64_eq(mrb_float(x),mrb_float(y)));
  default:
    return mrb_false_value();
  }
}

/* 15.2.8.3.18 */
/*
 * call-seq:
 *   flt.hash  ->  integer
 *
 * Returns a hash code for this float.
 */
static mrb_value
flo_hash(mrb_state *mrb, mrb_value num)
{
  mrb_float d;
  char *c;
  size_t i;
  int hash;

  d = i64_to_f64(mrb_fixnum(num));
  //d = (mrb_float)mrb_fixnum(num);
  /* normalize -0.0 to 0.0 */
 // if (d == 0) d = (mrb_float)0.0;
  if(f64_eq(d,i64_to_f64(0))) d = i64_to_f64(0);
  c = (char*)&d;
  for (hash=0, i=0; i<sizeof(mrb_float);i++) {
    hash = (hash * 971) ^ (unsigned char)c[i];
  }
  if (hash < 0) hash = -hash;
  return mrb_fixnum_value(hash);
}

/* 15.2.9.3.13 */
/*
 * call-seq:
 *   flt.to_f  ->  self
 *
 * As <code>flt</code> is already a float, returns +self+.
 */

static mrb_value
flo_to_f(mrb_state *mrb, mrb_value num)
{
  return num;
}

/* 15.2.9.3.11 */
/*
 *  call-seq:
 *     flt.infinite?  ->  nil, -1, +1
 *
 *  Returns <code>nil</code>, -1, or +1 depending on whether <i>flt</i>
 *  is finite, -infinity, or +infinity.
 *
 *     (0.0).infinite?        #=> nil
 *     (-1.0/0.0).infinite?   #=> -1
 *     (+1.0/0.0).infinite?   #=> 1
 */

static mrb_value
flo_infinite_p(mrb_state *mrb, mrb_value num)
{
  mrb_float value = mrb_float(num);

 // if (isinf(value)) {
  if(f64_isinf(value)){
   // return mrb_fixnum_value(value < 0 ? -1 : 1);
   return mrb_fixnum_value(f64_lt(value,i64_to_f64(0)) ? -1 : 1);
  }
  return mrb_nil_value();
}

/* 15.2.9.3.9  */
/*
 *  call-seq:
 *     flt.finite?  ->  true or false
 *
 *  Returns <code>true</code> if <i>flt</i> is a valid IEEE floating
 *  point number (it is not infinite, and <code>nan?</code> is
 *  <code>false</code>).
 *
 */

static mrb_value
flo_finite_p(mrb_state *mrb, mrb_value num)
{
 // return mrb_bool_value(isfinite(mrb_float(num)));
  return mrb_bool_value(f64_isfinite(mrb_float(num)));
}

/* 15.2.9.3.10 */
/*
 *  call-seq:
 *     flt.floor  ->  integer
 *
 *  Returns the largest integer less than or equal to <i>flt</i>.
 *
 *     1.2.floor      #=> 1
 *     2.0.floor      #=> 2
 *     (-1.2).floor   #=> -2
 *     (-2.0).floor   #=> -2
 */

static mrb_value
flo_floor(mrb_state *mrb, mrb_value num)
{
  mrb_float f = f64_floor(mrb_float(num));

  if (!FIXABLE(f)) {
    return mrb_float_value(mrb, f);
  }
  //return mrb_fixnum_value((mrb_int)f);
  return mrb_fixnum_value(f64_to_i64(f));
}

/* 15.2.9.3.8  */
/*
 *  call-seq:
 *     flt.ceil  ->  integer
 *
 *  Returns the smallest <code>Integer</code> greater than or equal to
 *  <i>flt</i>.
 *
 *     1.2.ceil      #=> 2
 *     2.0.ceil      #=> 2
 *     (-1.2).ceil   #=> -1
 *     (-2.0).ceil   #=> -2
 */

static mrb_value
flo_ceil(mrb_state *mrb, mrb_value num)
{
  mrb_float f = f64_ceil(mrb_float(num));

  if (!FIXABLE(f)) {
    return mrb_float_value(mrb, f);
  }
  //return mrb_fixnum_value((mrb_int)f);
  return mrb_fixnum_value(f64_to_i64(f));
}

/* 15.2.9.3.12 */
/*
 *  call-seq:
 *     flt.round([ndigits])  ->  integer or float
 *
 *  Rounds <i>flt</i> to a given precision in decimal digits (default 0 digits).
 *  Precision may be negative.  Returns a floating point number when ndigits
 *  is more than zero.
 *
 *     1.4.round      #=> 1
 *     1.5.round      #=> 2
 *     1.6.round      #=> 2
 *     (-1.5).round   #=> -2
 *
 *     1.234567.round(2)  #=> 1.23
 *     1.234567.round(3)  #=> 1.235
 *     1.234567.round(4)  #=> 1.2346
 *     1.234567.round(5)  #=> 1.23457
 *
 *     34567.89.round(-5) #=> 0
 *     34567.89.round(-4) #=> 30000
 *     34567.89.round(-3) #=> 35000
 *     34567.89.round(-2) #=> 34600
 *     34567.89.round(-1) #=> 34570
 *     34567.89.round(0)  #=> 34568
 *     34567.89.round(1)  #=> 34567.9
 *     34567.89.round(2)  #=> 34567.89
 *     34567.89.round(3)  #=> 34567.89
 *
 */

static mrb_value
flo_round(mrb_state *mrb, mrb_value num)
{
  //double number, f;
  float64_t number,f;
  mrb_int ndigits = 0;
  int i;

  mrb_get_args(mrb, "|i", &ndigits);
  number = mrb_float(num);

//  if (isinf(number)) {
  if(f64_isinf(number)){
    if (0 < ndigits) return num;
 //   else mrb_raise(mrb, E_FLOATDOMAIN_ERROR, number < 0 ? "-Infinity" : "Infinity");
    else mrb_raise(mrb, E_FLOATDOMAIN_ERROR, f64_lt(i64_to_f64(0),number)? "-Infinity" : "Infinity");
  }
//  if (isnan(number)) {
  if(f64_isnan(number)){
    if (0 < ndigits) return num;
    else mrb_raise(mrb, E_FLOATDOMAIN_ERROR, "NaN");
  }

  //f = 1.0;
  f = i64_to_f64(1);
  i = abs(ndigits);
  while  (--i >= 0)
  //  f = f*(mrb_float)10.0;
    f = f64_mul(f,i64_to_f64(10));

//  if (isinf(f)) {
  if(f64_isinf(f)){
    //if (ndigits < 0) number = 0;
    if(ndigits < 0) number = i64_to_f64(0);
  }
  else {
    float64_t d;

   // if (ndigits < 0) number /= f;
    if (ndigits < 0) number = f64_div(number,f);
    //else number *= f;
    else number = f64_mul(number,f);

    /* home-made inline implementation of round(3) */
    //if (number > 0.0) {
    if(f64_lt(i64_to_f64(0),number)){
      //d = floor(number);
      float64_t buf = {0x3FE0000000000000};
      d = f64_floor(number);
      //number = d + (number - d >= 0.5);
      number = f64_add(d,i64_to_f64(f64_le(buf,f64_sub(number,d))));
    }
    //else if (number < 0.0) {
    else if (f64_lt(number,i64_to_f64(0))){
      //d = ceil(number);
      float64_t buf = {0x3FE0000000000000};
      d = f64_ceil(number);
      //number = d - (d - number >= 0.5);
      number = f64_sub(d,i64_to_f64(f64_le(buf,f64_sub(d,number))));
    }

    //if (ndigits < 0) number *= f;
    if(ndigits < 0) number = f64_mul(number,f);
    //else number /= f;
    else number = f64_div(number,f);
  }

  if (ndigits > 0) {
    if (!f64_isfinite(number)) return num;
    return mrb_float_value(mrb, number);
  }
  //return mrb_fixnum_value((mrb_int)number);
  return mrb_fixnum_value(f64_to_i64(number));
}

/* 15.2.9.3.14 */
/* 15.2.9.3.15 */
/*
 *  call-seq:
 *     flt.to_i      ->  integer
 *     flt.to_int    ->  integer
 *     flt.truncate  ->  integer
 *
 *  Returns <i>flt</i> truncated to an <code>Integer</code>.
 */

static mrb_value
flo_truncate(mrb_state *mrb, mrb_value num)
{
  mrb_float f = mrb_float(num);

  //if (f > (mrb_float)0.0) f = floor(f);
  //if (f < (mrb_float)0.0) f = ceil(f);
  if(f64_lt(i64_to_f64(0),f)) f = f64_floor(f);
  if(f64_lt(f,i64_to_f64(0))) f = f64_ceil(f);

  if (!FIXABLE(f)) {
    return mrb_float_value(mrb, f);
  }
  //return mrb_fixnum_value((mrb_int)f);
  return mrb_fixnum_value(f64_to_i64(f));
}

static mrb_value
flo_nan_p(mrb_state *mrb, mrb_value num)
{
//  return mrb_bool_value(isnan(mrb_float(num)));
    return mrb_bool_value(f64_isnan(mrb_float(num)));
}

/*
 * Document-class: Integer
 *
 *  <code>Integer</code> is the basis for the two concrete classes that
 *  hold whole numbers, <code>Bignum</code> and <code>Fixnum</code>.
 *
 */


/*
 *  call-seq:
 *     int.to_i      ->  integer
 *     int.to_int    ->  integer
 *
 *  As <i>int</i> is already an <code>Integer</code>, all these
 *  methods simply return the receiver.
 */

static mrb_value
int_to_i(mrb_state *mrb, mrb_value num)
{
  return num;
}

/*tests if N*N would overflow*/
#define SQRT_INT_MAX ((mrb_int)1<<((MRB_INT_BIT-1-MRB_FIXNUM_SHIFT)/2))
#define FIT_SQRT_INT(n) (((n)<SQRT_INT_MAX)&&((n)>=-SQRT_INT_MAX))

mrb_value
mrb_fixnum_mul(mrb_state *mrb, mrb_value x, mrb_value y)
{
  mrb_int a;

  a = mrb_fixnum(x);
  if (mrb_fixnum_p(y)) {
    mrb_float c;
    mrb_int b;

    if (a == 0) return x;
    b = mrb_fixnum(y);
    if (FIT_SQRT_INT(a) && FIT_SQRT_INT(b))
      return mrb_fixnum_value(a*b);
    //c = a * b;
    c = f64_mul(i64_to_f64(a),i64_to_f64(b));
    //if ((a != 0 && c/a != b) || !FIXABLE(c)) {
    if ((a != 0 && !f64_eq(f64_div(c,i64_to_f64(a)),i64_to_f64(b))) || !FIXABLE(c)) {
      //return mrb_float_value(mrb, (mrb_float)a*(mrb_float)b);
      return mrb_float_value(mrb,f64_mul(i64_to_f64(a),i64_to_f64(b)));
    }
    //return mrb_fixnum_value((mrb_int)c);
    return mrb_fixnum_value(f64_to_i64(c));
  }
//  return mrb_float_value(mrb, (mrb_float)a * mrb_to_flo(mrb, y));
  return mrb_float_value(mrb,f64_mul(i64_to_f64(a),mrb_to_flo(mrb,y)));
}

/* 15.2.8.3.3  */
/*
 * call-seq:
 *   fix * numeric  ->  numeric_result
 *
 * Performs multiplication: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */

static mrb_value
fix_mul(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);
  return mrb_fixnum_mul(mrb, x, y);
}

static void
fixdivmod(mrb_state *mrb, mrb_int x, mrb_int y, mrb_int *divp, mrb_int *modp)
{
  mrb_int div, mod;

  /* TODO: add mrb_assert(y != 0) to make sure */

  if (y < 0) {
    if (x < 0)
      div = -x / -y;
    else
      div = - (x / -y);
  }
  else {
    if (x < 0)
      div = - (-x / y);
    else
      div = x / y;
  }
  mod = x - div*y;
  if ((mod < 0 && y > 0) || (mod > 0 && y < 0)) {
    mod += y;
    div -= 1;
  }
  if (divp) *divp = div;
  if (modp) *modp = mod;
}

/* 15.2.8.3.5  */
/*
 *  call-seq:
 *    fix % other        ->  real
 *    fix.modulo(other)  ->  real
 *
 *  Returns <code>fix</code> modulo <code>other</code>.
 *  See <code>numeric.divmod</code> for more information.
 */

static mrb_value
fix_mod(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_int a;

  mrb_get_args(mrb, "o", &y);
  a = mrb_fixnum(x);
  if (mrb_fixnum_p(y)) {
    mrb_int b, mod;

    if ((b=mrb_fixnum(y)) == 0) {
      return mrb_float_value(mrb, NAN);
    }
    fixdivmod(mrb, a, b, 0, &mod);
    return mrb_fixnum_value(mod);
  }
  else {
    mrb_float mod;

    //flodivmod(mrb, (mrb_float)a, mrb_to_flo(mrb, y), 0, &mod);
    flodivmod(mrb, i64_to_f64(a), mrb_to_flo(mrb, y), 0, &mod);
    return mrb_float_value(mrb, mod);
  }
}

/*
 *  call-seq:
 *     fix.divmod(numeric)  ->  array
 *
 *  See <code>Numeric#divmod</code>.
 */
static mrb_value
fix_divmod(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);

  if (mrb_fixnum_p(y)) {
    mrb_int div, mod;

    if (mrb_fixnum(y) == 0) {
      return mrb_assoc_new(mrb, mrb_float_value(mrb, INFINITY),
        mrb_float_value(mrb, NAN));
    }
    fixdivmod(mrb, mrb_fixnum(x), mrb_fixnum(y), &div, &mod);
    return mrb_assoc_new(mrb, mrb_fixnum_value(div), mrb_fixnum_value(mod));
  }
  else {
    mrb_float div, mod;
    mrb_value a, b;

   // flodivmod(mrb, (mrb_float)mrb_fixnum(x), mrb_to_flo(mrb, y), &div, &mod);
    flodivmod(mrb, i64_to_f64(mrb_fixnum(x)), mrb_to_flo(mrb, y), &div, &mod);
    //a = mrb_float_value(mrb, (mrb_int)div);
    a = mrb_float_value(mrb,div);
    b = mrb_float_value(mrb, mod);
    return mrb_assoc_new(mrb, a, b);
  }
}

static mrb_value
flo_divmod(mrb_state *mrb, mrb_value x)
{
  mrb_value y;
  mrb_float div, mod;
  mrb_value a, b;

  mrb_get_args(mrb, "o", &y);

  flodivmod(mrb, mrb_float(x), mrb_to_flo(mrb, y), &div, &mod);
  //a = mrb_float_value(mrb, (mrb_int)div);
   a = mrb_float_value(mrb,div);
  b = mrb_float_value(mrb, mod);
  return mrb_assoc_new(mrb, a, b);
}

/* 15.2.8.3.7  */
/*
 * call-seq:
 *   fix == other  ->  true or false
 *
 * Return <code>true</code> if <code>fix</code> equals <code>other</code>
 * numerically.
 *
 *   1 == 2      #=> false
 *   1 == 1.0    #=> true
 */

static mrb_value
fix_equal(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);
  switch (mrb_type(y)) {
  case MRB_TT_FIXNUM:
    return mrb_bool_value(mrb_fixnum(x) == mrb_fixnum(y));
  case MRB_TT_FLOAT:
    //return mrb_bool_value((mrb_float)mrb_fixnum(x) == mrb_float(y));
    return mrb_bool_value(f64_eq(i64_to_f64(mrb_fixnum(x)),mrb_float(y)));
  default:
    return mrb_false_value();
  }
}

/* 15.2.8.3.8  */
/*
 * call-seq:
 *   ~fix  ->  integer
 *
 * One's complement: returns a number where each bit is flipped.
 *   ex.0---00001 (1)-> 1---11110 (-2)
 *   ex.0---00010 (2)-> 1---11101 (-3)
 *   ex.0---00100 (4)-> 1---11011 (-5)
 */

static mrb_value
fix_rev(mrb_state *mrb, mrb_value num)
{
  mrb_int val = mrb_fixnum(num);

  return mrb_fixnum_value(~val);
}

static mrb_value
bit_coerce(mrb_state *mrb, mrb_value x)
{
  while (!mrb_fixnum_p(x)) {
    if (mrb_float_p(x)) {
      mrb_raise(mrb, E_TYPE_ERROR, "can't convert Float into Integer");
    }
    x = mrb_to_int(mrb, x);
  }
  return x;
}

/* 15.2.8.3.9  */
/*
 * call-seq:
 *   fix & integer  ->  integer_result
 *
 * Bitwise AND.
 */

static mrb_value
fix_and(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);

  y = bit_coerce(mrb, y);
  return mrb_fixnum_value(mrb_fixnum(x) & mrb_fixnum(y));
}

/* 15.2.8.3.10 */
/*
 * call-seq:
 *   fix | integer  ->  integer_result
 *
 * Bitwise OR.
 */

static mrb_value
fix_or(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);

  y = bit_coerce(mrb, y);
  return mrb_fixnum_value(mrb_fixnum(x) | mrb_fixnum(y));
}

/* 15.2.8.3.11 */
/*
 * call-seq:
 *   fix ^ integer  ->  integer_result
 *
 * Bitwise EXCLUSIVE OR.
 */

static mrb_value
fix_xor(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);

  y = bit_coerce(mrb, y);
  return mrb_fixnum_value(mrb_fixnum(x) ^ mrb_fixnum(y));
}

#define NUMERIC_SHIFT_WIDTH_MAX (MRB_INT_BIT-1)

static mrb_value
lshift(mrb_state *mrb, mrb_int val, mrb_int width)
{
  mrb_assert(width >= 0);
  if (width > NUMERIC_SHIFT_WIDTH_MAX) {
    mrb_raisef(mrb, E_RANGE_ERROR, "width(%S) > (%S:MRB_INT_BIT-1)",
               mrb_fixnum_value(width),
               mrb_fixnum_value(NUMERIC_SHIFT_WIDTH_MAX));
  }
  return mrb_fixnum_value(val << width);
}

static mrb_value
rshift(mrb_int val, mrb_int width)
{
  mrb_assert(width >= 0);
  if (width >= NUMERIC_SHIFT_WIDTH_MAX) {
    if (val < 0) {
      return mrb_fixnum_value(-1);
    }
    return mrb_fixnum_value(0);
  }
  return mrb_fixnum_value(val >> width);
}

static inline void
fix_shift_get_width(mrb_state *mrb, mrb_int *width)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);
  *width = mrb_fixnum(bit_coerce(mrb, y));
}

/* 15.2.8.3.12 */
/*
 * call-seq:
 *   fix << count  ->  integer
 *
 * Shifts _fix_ left _count_ positions (right if _count_ is negative).
 */

static mrb_value
fix_lshift(mrb_state *mrb, mrb_value x)
{
  mrb_int width, val;

  fix_shift_get_width(mrb, &width);

  if (width == 0) {
    return x;
  }
  val = mrb_fixnum(x);
  if (width < 0) {
    return rshift(val, -width);
  }
  return lshift(mrb, val, width);
}

/* 15.2.8.3.13 */
/*
 * call-seq:
 *   fix >> count  ->  integer
 *
 * Shifts _fix_ right _count_ positions (left if _count_ is negative).
 */

static mrb_value
fix_rshift(mrb_state *mrb, mrb_value x)
{
  mrb_int width, val;

  fix_shift_get_width(mrb, &width);

  if (width == 0) {
    return x;
  }
  val = mrb_fixnum(x);
  if (width < 0) {
    return lshift(mrb, val, -width);
  }
  return rshift(val, width);
}

/* 15.2.8.3.23 */
/*
 *  call-seq:
 *     fix.to_f  ->  float
 *
 *  Converts <i>fix</i> to a <code>Float</code>.
 *
 */

static mrb_value
fix_to_f(mrb_state *mrb, mrb_value num)
{
  //return mrb_float_value(mrb, (mrb_float)mrb_fixnum(num));
  return mrb_float_value(mrb,i64_to_f64(mrb_fixnum(num)));
}

/*
 *  Document-class: FloatDomainError
 *
 *  Raised when attempting to convert special float values
 *  (in particular infinite or NaN)
 *  to numerical classes which don't support them.
 *
 *     Float::INFINITY.to_r
 *
 *  <em>raises the exception:</em>
 *
 *     FloatDomainError: Infinity
 */
/* ------------------------------------------------------------------------*/
MRB_API mrb_value
mrb_flo_to_fixnum(mrb_state *mrb, mrb_value x)
{
  mrb_int z;

  if (!mrb_float_p(x)) {
    mrb_raise(mrb, E_TYPE_ERROR, "non float value");
    z = 0; /* not reached. just suppress warnings. */
  }
  else {
    mrb_float d = mrb_float(x);

   // if (isinf(d)) {
   if(f64_isinf(d)){
     // mrb_raise(mrb, E_FLOATDOMAIN_ERROR, d < 0 ? "-Infinity" : "Infinity");
      mrb_raise(mrb, E_FLOATDOMAIN_ERROR, f64_lt(d,i64_to_f64(0)) ? "-Infinity" : "Infinity");
    }
   // if (isnan(d)) {
   if(f64_isnan(d)){
      mrb_raise(mrb, E_FLOATDOMAIN_ERROR, "NaN");
    }
    //z = (mrb_int)d;
    z = f64_to_i64(d);
  }
  return mrb_fixnum_value(z);
}

mrb_value
mrb_fixnum_plus(mrb_state *mrb, mrb_value x, mrb_value y)
{
  mrb_int a;

  a = mrb_fixnum(x);
  if (mrb_fixnum_p(y)) {
    mrb_int b, c;

    if (a == 0) return y;
    b = mrb_fixnum(y);
    if (mrb_int_add_overflow(a, b, &c)) {
      //return mrb_float_value(mrb, (mrb_float)a + (mrb_float)b);
      return mrb_float_value(mrb,f64_add(i64_to_f64(a),i64_to_f64(b)));
    }
    return mrb_fixnum_value(c);
  }
  //return mrb_float_value(mrb, (mrb_float)a + mrb_to_flo(mrb, y));
  return mrb_float_value(mrb,f64_add(i64_to_f64(a),mrb_to_flo(mrb,y)));
}

/* 15.2.8.3.1  */
/*
 * call-seq:
 *   fix + numeric  ->  numeric_result
 *
 * Performs addition: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */
static mrb_value
fix_plus(mrb_state *mrb, mrb_value self)
{
  mrb_value other;

  mrb_get_args(mrb, "o", &other);
  return mrb_fixnum_plus(mrb, self, other);
}

mrb_value
mrb_fixnum_minus(mrb_state *mrb, mrb_value x, mrb_value y)
{
  mrb_int a;

  a = mrb_fixnum(x);
  if (mrb_fixnum_p(y)) {
    mrb_int b, c;

    b = mrb_fixnum(y);
    if (mrb_int_sub_overflow(a, b, &c)) {
      //return mrb_float_value(mrb, (mrb_float)a - (mrb_float)b);
       return mrb_float_value(mrb,f64_sub(i64_to_f64(a),i64_to_f64(b)));
    }
    return mrb_fixnum_value(c);
  }
//  return mrb_float_value(mrb, (mrb_float)a - mrb_to_flo(mrb, y));
  return mrb_float_value(mrb,f64_sub(i64_to_f64(a),mrb_to_flo(mrb,y)));

}

/* 15.2.8.3.2  */
/* 15.2.8.3.16 */
/*
 * call-seq:
 *   fix - numeric  ->  numeric_result
 *
 * Performs subtraction: the class of the resulting object depends on
 * the class of <code>numeric</code> and on the magnitude of the
 * result.
 */
static mrb_value
fix_minus(mrb_state *mrb, mrb_value self)
{
  mrb_value other;

  mrb_get_args(mrb, "o", &other);
  return mrb_fixnum_minus(mrb, self, other);
}


MRB_API mrb_value
mrb_fixnum_to_str(mrb_state *mrb, mrb_value x, int base)
{
  char buf[MRB_INT_BIT+1];
  char *b = buf + sizeof buf;
  mrb_int val = mrb_fixnum(x);

  if (base < 2 || 36 < base) {
    mrb_raisef(mrb, E_ARGUMENT_ERROR, "invalid radix %S", mrb_fixnum_value(base));
  }

  if (val == 0) {
    *--b = '0';
  }
  else if (val < 0) {
    do {
      *--b = mrb_digitmap[-(val % base)];
    } while (val /= base);
    *--b = '-';
  }
  else {
    do {
      *--b = mrb_digitmap[(int)(val % base)];
    } while (val /= base);
  }

  return mrb_str_new(mrb, b, buf + sizeof(buf) - b);
}

/* 15.2.8.3.25 */
/*
 *  call-seq:
 *     fix.to_s(base=10)  ->  string
 *
 *  Returns a string containing the representation of <i>fix</i> radix
 *  <i>base</i> (between 2 and 36).
 *
 *     12345.to_s       #=> "12345"
 *     12345.to_s(2)    #=> "11000000111001"
 *     12345.to_s(8)    #=> "30071"
 *     12345.to_s(10)   #=> "12345"
 *     12345.to_s(16)   #=> "3039"
 *     12345.to_s(36)   #=> "9ix"
 *
 */
static mrb_value
fix_to_s(mrb_state *mrb, mrb_value self)
{
  mrb_int base = 10;

  mrb_get_args(mrb, "|i", &base);
  return mrb_fixnum_to_str(mrb, self, base);
}

/* 15.2.9.3.6  */
/*
 * call-seq:
 *     self.f <=> other.f    => -1, 0, +1
 *             <  => -1
 *             =  =>  0
 *             >  => +1
 *  Comparison---Returns -1, 0, or +1 depending on whether <i>fix</i> is
 *  less than, equal to, or greater than <i>numeric</i>. This is the
 *  basis for the tests in <code>Comparable</code>.
 */
static mrb_value
num_cmp(mrb_state *mrb, mrb_value self)
{
  mrb_value other;
  mrb_float x, y;

  mrb_get_args(mrb, "o", &other);

  x = mrb_to_flo(mrb, self);
  switch (mrb_type(other)) {
  case MRB_TT_FIXNUM:
    //y = (mrb_float)mrb_fixnum(other);
    y = i64_to_f64(mrb_fixnum(other));
    break;
  case MRB_TT_FLOAT:
    y = mrb_float(other);
    break;
  default:
    return mrb_nil_value();
  }
  //if (x > y)
  if(f64_lt(y,x))
    return mrb_fixnum_value(1);
  else {
    //if (x < y)
    if(f64_lt(x,y))
      return mrb_fixnum_value(-1);
    return mrb_fixnum_value(0);
  }
}

/* 15.2.9.3.1  */
/*
 * call-seq:
 *   float + other  ->  float
 *
 * Returns a new float which is the sum of <code>float</code>
 * and <code>other</code>.
 */
static mrb_value
flo_plus(mrb_state *mrb, mrb_value x)
{
  mrb_value y;

  mrb_get_args(mrb, "o", &y);
  return mrb_float_value(mrb, f64_add(mrb_float(x),mrb_to_flo(mrb, y)));
}

/* ------------------------------------------------------------------------*/
void
mrb_init_numeric(mrb_state *mrb)
{
  struct RClass *numeric, *integer, *fixnum, *fl;

  /* Numeric Class */
  numeric = mrb_define_class(mrb, "Numeric",  mrb->object_class);                /* 15.2.7 */

  mrb_define_method(mrb, numeric, "**",       num_pow,        MRB_ARGS_REQ(1));
  mrb_define_method(mrb, numeric, "/",        num_div,        MRB_ARGS_REQ(1));  /* 15.2.8.3.4  */
  mrb_define_method(mrb, numeric, "quo",      num_div,        MRB_ARGS_REQ(1));  /* 15.2.7.4.5 (x) */
  mrb_define_method(mrb, numeric, "<=>",      num_cmp,        MRB_ARGS_REQ(1));  /* 15.2.9.3.6  */

  /* Integer Class */
  integer = mrb_define_class(mrb, "Integer",  numeric);                          /* 15.2.8 */
  mrb_undef_class_method(mrb, integer, "new");
  mrb_define_method(mrb, integer, "to_i", int_to_i, MRB_ARGS_NONE());            /* 15.2.8.3.24 */
  mrb_define_method(mrb, integer, "to_int", int_to_i, MRB_ARGS_NONE());

  /* Fixnum Class */
  fixnum = mrb->fixnum_class = mrb_define_class(mrb, "Fixnum", integer);
  mrb_define_method(mrb, fixnum,  "+",        fix_plus,          MRB_ARGS_REQ(1)); /* 15.2.8.3.1  */
  mrb_define_method(mrb, fixnum,  "-",        fix_minus,         MRB_ARGS_REQ(1)); /* 15.2.8.3.2  */
  mrb_define_method(mrb, fixnum,  "*",        fix_mul,           MRB_ARGS_REQ(1)); /* 15.2.8.3.3  */
  mrb_define_method(mrb, fixnum,  "%",        fix_mod,           MRB_ARGS_REQ(1)); /* 15.2.8.3.5  */
  mrb_define_method(mrb, fixnum,  "==",       fix_equal,         MRB_ARGS_REQ(1)); /* 15.2.8.3.7  */
  mrb_define_method(mrb, fixnum,  "~",        fix_rev,           MRB_ARGS_NONE()); /* 15.2.8.3.8  */
  mrb_define_method(mrb, fixnum,  "&",        fix_and,           MRB_ARGS_REQ(1)); /* 15.2.8.3.9  */
  mrb_define_method(mrb, fixnum,  "|",        fix_or,            MRB_ARGS_REQ(1)); /* 15.2.8.3.10 */
  mrb_define_method(mrb, fixnum,  "^",        fix_xor,           MRB_ARGS_REQ(1)); /* 15.2.8.3.11 */
  mrb_define_method(mrb, fixnum,  "<<",       fix_lshift,        MRB_ARGS_REQ(1)); /* 15.2.8.3.12 */
  mrb_define_method(mrb, fixnum,  ">>",       fix_rshift,        MRB_ARGS_REQ(1)); /* 15.2.8.3.13 */
  mrb_define_method(mrb, fixnum,  "eql?",     fix_eql,           MRB_ARGS_REQ(1)); /* 15.2.8.3.16 */
  mrb_define_method(mrb, fixnum,  "hash",     flo_hash,          MRB_ARGS_NONE()); /* 15.2.8.3.18 */
  mrb_define_method(mrb, fixnum,  "to_f",     fix_to_f,          MRB_ARGS_NONE()); /* 15.2.8.3.23 */
  mrb_define_method(mrb, fixnum,  "to_s",     fix_to_s,          MRB_ARGS_NONE()); /* 15.2.8.3.25 */
  mrb_define_method(mrb, fixnum,  "inspect",  fix_to_s,          MRB_ARGS_NONE());
  mrb_define_method(mrb, fixnum,  "divmod",   fix_divmod,        MRB_ARGS_REQ(1)); /* 15.2.8.3.30 (x) */

  /* Float Class */
  fl = mrb->float_class = mrb_define_class(mrb, "Float", numeric);                 /* 15.2.9 */
  mrb_undef_class_method(mrb,  fl, "new");
  mrb_define_method(mrb, fl,      "+",         flo_plus,         MRB_ARGS_REQ(1)); /* 15.2.9.3.1  */
  mrb_define_method(mrb, fl,      "-",         flo_minus,        MRB_ARGS_REQ(1)); /* 15.2.9.3.2  */
  mrb_define_method(mrb, fl,      "*",         flo_mul,          MRB_ARGS_REQ(1)); /* 15.2.9.3.3  */
  mrb_define_method(mrb, fl,      "%",         flo_mod,          MRB_ARGS_REQ(1)); /* 15.2.9.3.5  */
  mrb_define_method(mrb, fl,      "==",        flo_eq,           MRB_ARGS_REQ(1)); /* 15.2.9.3.7  */
  mrb_define_method(mrb, fl,      "ceil",      flo_ceil,         MRB_ARGS_NONE()); /* 15.2.9.3.8  */
  mrb_define_method(mrb, fl,      "finite?",   flo_finite_p,     MRB_ARGS_NONE()); /* 15.2.9.3.9  */
  mrb_define_method(mrb, fl,      "floor",     flo_floor,        MRB_ARGS_NONE()); /* 15.2.9.3.10 */
  mrb_define_method(mrb, fl,      "infinite?", flo_infinite_p,   MRB_ARGS_NONE()); /* 15.2.9.3.11 */
  mrb_define_method(mrb, fl,      "round",     flo_round,        MRB_ARGS_OPT(1)); /* 15.2.9.3.12 */
  mrb_define_method(mrb, fl,      "to_f",      flo_to_f,         MRB_ARGS_NONE()); /* 15.2.9.3.13 */
  mrb_define_method(mrb, fl,      "to_i",      flo_truncate,     MRB_ARGS_NONE()); /* 15.2.9.3.14 */
  mrb_define_method(mrb, fl,      "to_int",    flo_truncate,     MRB_ARGS_NONE());
  mrb_define_method(mrb, fl,      "truncate",  flo_truncate,     MRB_ARGS_NONE()); /* 15.2.9.3.15 */
  mrb_define_method(mrb, fl,      "divmod",    flo_divmod,       MRB_ARGS_REQ(1));
  mrb_define_method(mrb, fl,      "eql?",      flo_eql,          MRB_ARGS_REQ(1)); /* 15.2.8.3.16 */

  mrb_define_method(mrb, fl,      "to_s",      flo_to_s,         MRB_ARGS_NONE()); /* 15.2.9.3.16(x) */
  mrb_define_method(mrb, fl,      "inspect",   flo_to_s,         MRB_ARGS_NONE());
  mrb_define_method(mrb, fl,      "nan?",      flo_nan_p,        MRB_ARGS_NONE());

#ifdef INFINITY
  mrb_define_const(mrb, fl, "INFINITY", mrb_float_value(mrb, INFINITY));
#endif
#ifdef NAN
  mrb_define_const(mrb, fl, "NAN", mrb_float_value(mrb, NAN));
#endif
}
