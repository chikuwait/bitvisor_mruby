typedef void *FILE;
typedef unsigned long size_t;
FILE stderr;
int snprintf (char *str, size_t size, const char *format, ...);
void fprintf (FILE *fp, const char *format, ...);
int printf(const char *fmt, ...);// __attribute__ ((format (printf, 1, 2)));


