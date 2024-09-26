/* See LICENSE file for copyright and license details. */

#ifndef MAX
#define MAX(A, B)               ((A) > (B) ? (A) : (B))
#endif
#ifndef MIN
#define MIN(A, B)               ((A) < (B) ? (A) : (B))
#endif
#ifndef BETWEEN
#define BETWEEN(X, A, B)        ((A) <= (X) && (X) <= (B))
#endif
#ifndef SWAP
#define SWAP(a, b)              do { typeof(a) _ = (a); (a) = (b); (b) = (_); } while (0)
#endif
#define LENGTH(X)               (sizeof X / sizeof X[0])

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
