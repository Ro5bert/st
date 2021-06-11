/* See LICENSE for license details. */
/* Requires: stdint.h, size_t, ssize_t */

#define UTF_INVALID 0xFFFD
#define UTF_SIZ     4

#define MIN(a, b)              ((a) < (b) ? (a) : (b))
#define MAX(a, b)              ((a) < (b) ? (b) : (a))
#define LEN(a)                 (sizeof(a) / sizeof(a)[0])
#define BETWEEN(x, a, b)       ((a) <= (x) && (x) <= (b))
#define DIVCEIL(n, d)          (((n) + ((d) - 1)) / (d))
#define DEFAULT(a, b)          (a) = (a) ? (a) : (b)
#define LIMIT(x, a, b)         (x) = (x) < (a) ? (a) : (x) > (b) ? (b) : (x)
#define TIMEDIFF(t1, t2)       ((t1.tv_sec-t2.tv_sec)*1000 + \
                                (t1.tv_nsec-t2.tv_nsec)/1E6)
#define MODBIT(x, set, bit)    ((set) ? ((x) |= (bit)) : ((x) &= ~(bit)))
#define MATCH(state, set, clr) (((set & ~state) | (clr & state)) == 0)

typedef unsigned char uchar;
typedef unsigned short ushort;
typedef unsigned int uint;
typedef unsigned long ulong;

typedef uint_least32_t Rune;

void die(const char *, ...);
void *xmalloc(size_t);
void *xrealloc(void *, size_t);
char *xstrdup(const char *);
ssize_t xwrite(int, const char *, size_t);
size_t utf8dec(const char *, Rune *, size_t);
size_t utf8enc(Rune, char *);
char *base64dec(const char *);
size_t csienc(char *, size_t, uint, uint, uint, char);
