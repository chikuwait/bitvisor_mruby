#if 0
#include <linux/ctype.h>
#endif
#define isalnum(c) (isalpha(c) || isdigit(c))
#define isalpha(c) (isupper(c) || islower(c))
#define isupper(c) ('A' <= (c) && (c) <= 'Z')
#define islower(c) ('a' <= (c) && (c) <= 'z')
#define isdigit(c) ('0' <= (c) && (c) <= '9')
#define toupper(c) (isalpha(c) ? (c) & ~0x20 : (c))
#define tolower(c) (isalpha(c) ? (c) |  0x20 : (c))
#define isspace(c) ((c) == 0x20) | (0x09 <= (c) && (c) <= 0x0d)
#define isprint(c) (0x20 <= (c) && (c) <= 0x7e)
#define isxdigit(c) (isdigit(c) || ('a' <= (c) && (c) <= 'f') || ('A' <= (c) && (c) <= 'F'))
