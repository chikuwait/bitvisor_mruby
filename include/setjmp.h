#ifndef _BITVISOR_SETJMP_H
#define _BITVISOR_SETJMP_H

typedef int jmp_buf[6];
#define setjmp(env) __builtin_setjmp(env)
#define longjmp(env, val) __builtin_longjmp(env, val)

#endif
