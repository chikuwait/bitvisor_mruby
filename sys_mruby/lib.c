#include <core/types.h>
#include <core/mm.h>
#include <core/strtol.h>
#include <core/panic.h>
#include <stdint.h>
#include <limits.h>

typedef unsigned long size_t;
void *realloc (void *virt, uint len);
void free (void *virt);

int * __errno_location(void);
long int strtol(const char *nptr, char **endptr, int base);
unsigned long int strtoul(const char *nptr, char **endptr, int base);

void abort(void);
void exit(int status);

unsigned long int
strtoul(const char *nptr, char **endptr, int base)
{
    return (unsigned long int)strtol(nptr,endptr,base);
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


