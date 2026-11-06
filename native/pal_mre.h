#ifndef POCKETJAVA_PAL_MRE_H
#define POCKETJAVA_PAL_MRE_H

#include <stddef.h>
#include "../build/nanojava-reference/src/pal/pal.h"

/* Native MRE adapter for the portable Nano Java MIDP runtime. */
int pocketjava_mre_start(const char *midlet_class, const void *jar, unsigned jar_size,
                         const void *classlib, unsigned classlib_size,
                         size_t heap_size);
void pocketjava_mre_stop(void);

#endif
