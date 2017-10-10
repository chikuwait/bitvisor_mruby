typedef unsigned long size_t;
void free(void *ptr);
void *malloc(size_t size);
void *realloc(void *ptr, size_t size);
int abs(int j);
unsigned long int strtol(const char *nptr, char **endptr, int base);
unsigned long int strtoul(const char *nptr, char **endptr, int base);
void exit(int status);
#define EXIT_SUCCESS 0
#define EXIT_FAILURE (-1)
void abort(void);
int atoi(const char *nptr);
# define strtod(p,e) strtol(p,e,10)
