#ifndef _BITVISOR_ERRORNO_H
#define _BITVISOR_ERRORNO_H

extern int *__errno_location (void) __attribute__ ((__const__));
#define errno (*__errno_location ())
#define ERANGE 34
#endif

