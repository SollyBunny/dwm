/* See LICENSE file for copyright and license details. */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#ifdef Bool
	#undef Bool
#endif /* Bool */

#ifndef MAX
	#define MAX(A, B)           ((A) > (B) ? (A) : (B))
#endif /* MAX */
#ifndef MIN
	#define MIN(A, B)           ((A) < (B) ? (A) : (B))
#endif /* MIN */
#ifndef BETWEEN
	#define BETWEEN(X, A, B)    ((A) <= (X) && (X) <= (B))
#endif /* BETWEEN */
#ifndef SWAP
	#define SWAP(a, b)          do { typeof(a) _ = (a); (a) = (b); (b) = (_); } while (0)
#endif /* SWAP */
#define LENGTH(X)               (sizeof(X) / sizeof(*X))

void die(const char *fmt, ...);
void *ecalloc(size_t nmemb, size_t size);
