#ifndef POCKET_H
#define POCKET_H
#include <stdint.h>
#include <stddef.h>
#define PJ_ERROR_CAP 160u
#ifdef MRE
#define PJ_MEMORY_LIMIT (816u*1024u)
#else
#define PJ_MEMORY_LIMIT (896u*1024u)
#endif
#define PJ_JAR_LIMIT (8u*1024u*1024u)
#define PJ_ENTRY_LIMIT (320u*1024u)
/* A time-bounded slice; the opcode cap only stops very fast/simple loops. */
#define PJ_SLICE_OPCODES 12000u
#define PJ_SLICE_MS 8u
#define PJ_FRAME_MS 33u
typedef struct PJ PJ;
typedef struct {
    void (*rect)(int,int,int,int,uint32_t);
    void (*text)(int,int,const char*,uint32_t);
    uint32_t (*clock_ms)(void);
} PJPlatform;
void *pj_alloc(size_t n);
void *pj_realloc(void *p,size_t n);
void pj_free(void *p);
size_t pj_memory(void);
size_t pj_peak(void);
void pj_collect(PJ *v);
unsigned char *platform_read(const char *path, size_t limit, size_t *size);
intptr_t platform_open(const char *path,size_t *size);
int platform_read_at(intptr_t file,size_t offset,void *out,size_t size);
void platform_close(intptr_t file);
int platform_tone(int note,int duration,int volume);
void platform_tone_stop(void);
int platform_record_write(uint32_t store,int id,const void *data,size_t n);
unsigned char *platform_record_read(uint32_t store,int id,size_t *n);
typedef struct { unsigned char *data,*directory; size_t size; uint32_t cd, end; uint16_t entries; intptr_t file; } PJJar;
int jar_open(PJJar *j, unsigned char *data, size_t size, char *error);
int jar_open_file(PJJar *j,const char *path,char *error);
void jar_close(PJJar *j);
typedef struct PJZipStream PJZipStream;
PJZipStream *jar_stream_open(PJ *v,PJJar *j,const char *name,char *error);
void jar_stream_close(PJZipStream *s);
int jar_stream_read(PJZipStream *s,void *out,size_t count,char *error);
size_t jar_stream_available(PJZipStream *s);
unsigned char *jar_extract(PJJar *j,const char *name,size_t *size,char *error);
int jar_property(const unsigned char *data,size_t size,const char *key,char *out,size_t cap);
PJ *pj_create(PJPlatform platform);
void pj_destroy(PJ *v);
int pj_load(PJ *v, const char *path);
int pj_load_memory(PJ *v,const unsigned char *data,size_t size);
int pj_step(PJ *v,unsigned instructions,unsigned max_ms);
void pj_key(PJ *v,int key,int released);
void pj_repaint(PJ *v);
void pj_pause(PJ *v,int paused);
const char *pj_error(PJ *v);
const char *pj_title(PJ *v);
uint64_t pj_instructions(PJ *v);
int pj_busy(PJ *v);
int pj_rendered(PJ *v);
int pj_finished(PJ *v);
void pj_debug_dump(PJ *v);
#endif

