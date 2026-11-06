#include "pocket.h"
#include <stdlib.h>
/* Eight-byte alignment is sufficient for every structure used by this VM. */
typedef union { size_t size; uint64_t align; } Header;
static size_t used, peak;
void *pj_alloc(size_t n) {
    Header *p;
    if (!n) n=1;
    if(n>PJ_MEMORY_LIMIT || used>PJ_MEMORY_LIMIT-n || used+n>PJ_MEMORY_LIMIT-sizeof(Header)) return NULL;
    p=(Header*)malloc(n+sizeof(Header));
    if(!p) return NULL;
    p->size=n+sizeof(Header); used+=p->size;
    if(used>peak) peak=used;
    return p+1;
}
void pj_free(void *p) { if(p) {Header *h=(Header*)p-1;used-=h->size;free(h);} }
size_t pj_memory(void) {return used;}
size_t pj_peak(void) {return peak;}

void *pj_realloc(void *p,size_t n){if(!p)return pj_alloc(n);Header*h=(Header*)p-1;size_t old=h->size;if(!n)n=1;if(n>PJ_MEMORY_LIMIT-sizeof(Header)||used-old>PJ_MEMORY_LIMIT-n-sizeof(Header))return NULL;Header*q=realloc(h,n+sizeof(Header));if(!q)return NULL;q->size=n+sizeof(Header);used=used-old+q->size;if(used>peak)peak=used;return q+1;}
