#ifndef _XLOCALE_H
#define _XLOCALE_H

typedef struct __locale_struct
{
    struct __locale_data *__locales[13]; /* 13 = __LC_LAST. */
    const unsigned short int *__ctype_b;
    const int *__ctype_tolower;
    const int *__ctype_toupper;
    const char *__names[13];
} *__locale_t;
typedef __locale_t locale_t;

#endif
