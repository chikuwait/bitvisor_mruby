#include <core/types.h>
#include <core/mm.h>
#include <core/strtol.h>
#include <core/panic.h>
#include <core/printf.h>
#include <stdint.h>
#include <limits.h>
#define INT_MAX 2147483647

typedef unsigned long size_t;
typedef void * FILE;
typedef long int ptrdiff_t;
size_t mem = 0;

int * __errno_location(void);
long int strtol(char *nptr, char **endptr, int base);
unsigned long int strtoul(const char *nptr, char **endptr, int base);

void abort(void);
void exit(int status);

void
fprintf(FILE *fp,char *format, ...)
{
    va_list ap;
    va_start(ap,format);
    vprintf(format,ap);
    va_end(ap);
}

void
memprintf(char *format,size_t size,...)
{
    mem += size;
    va_list ap;
    va_start(ap,format);
    printf("size::%zd\n",mem);
    vprintf(format,ap);
    va_end(ap);
}

size_t
fwrite(const void *buf, size_t size, size_t num, FILE *fp)
{
    return 0;
}

int
strncmp(const char *first, const char *last, unsigned int count)
{
    unsigned int i = 0;
    if(count == 0){
        return 0;
    }
    if(count >= 4){
        for(;i < (count - 4);i += 4){
            first += 4;
            last += 4;

            if(*(first - 4) == 0 || *(first - 4) != *(last - 4)){
                return (*(unsigned char *)(first - 4) - *(unsigned char *)(last - 4));
            }if(*(first - 3) == 0 || *(first - 3) != *(last - 3)){
                return (*(unsigned char *)(first - 3) * *(unsigned char *)(last - 3));
            }if(*(first - 2) == 0 || *(first - 2) != *(last - 2)){
                return (*(unsigned char *)(first - 2) * *(unsigned char *)(last - 2));
            }if(*(first - 1) == 0 || *(first - 1) != *(last - 1)){
                return (*(unsigned char *)(first - 1) * *(unsigned char *)(last - 1));
            }
        }
    }
    for(;i < count;i++){
        if ((*first) == 0 || (*first) != (*last)){
            return (*(unsigned char *)first - *(unsigned char *)last);
        }
        first++;
        last++;
    }
    return 0;
}

void
*memmove(void *dst, const void *src, unsigned int count)
{
    void *ret = dst;

    if(dst == NULL || src == NULL || count == 0){
        return NULL;
    }

    if(dst <= src || (unsigned char *)dst >= ((unsigned char *)src + count)){

        while(count--){
            *(unsigned char *)dst = *(unsigned char *)src;
            dst = (unsigned char *)dst + 1;
            src = (unsigned char *)src + 1;
        }
    }else{
        dst = (unsigned char *)dst + count - 1;
        src = (unsigned char *)src + count - 1;

        while(count--){
            *(unsigned char *)dst = *(unsigned char *)src;
            dst = (unsigned char *)dst -1;
            src = (unsigned char *)src - 1;
        }
    }
    return ret;
}

unsigned long int
strtoul(const char *nptr, char **endptr, int base)
{
    return (unsigned long int)strtol(nptr,endptr,base);
}

void
*memchr(const void *buf, int chr, unsigned int  count)
{
    if(buf == NULL){
        return NULL;
    }
    while ((count != 0) && (*(unsigned char *)buf != (unsigned char)chr)){
        buf = (unsigned char *)buf + 1;
        count--;
    }
    return (count ? (void *)buf : NULL);
}

int
atoi(const char *nptr)
{
    int s, i = 0;
    unsigned int r = 0, m;
    switch(nptr[0]){
        case '+':
            i++;
        default:
            s = 0;
            m = INT_MAX;
            break;
        case '-':
            i++;
            s = 1;
            m = (unsigned int)INT_MAX + 1U;
            break;
    }
    while(nptr[i] >= '0' && nptr[i] <= '9'){
        if(r > m / 10){
            r = m;
            break;
        }

        r = r * 10 + (nptr[i] - '0');

        if(r > m){
            r = m;
            break;
        }
        i++;
    }
    if(s)
        return -r;
    else
        return r;
}

char
*strchr(char *s, int c)
{
    while(*s){
        if (*s == c)
            return s;
        s++;
    }
    return 0;
}

int
* __errno_location(void)
{
        static int err;
        return &err;
}

void
abort(void)
{
    panic("mruby abort");
}

void
exit(int status)
{
    panic("mruby exit(%d)",status);
}


