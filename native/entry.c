#include <stddef.h>
#include <stdint.h>
#include "vmsys.h"
#include "vmstdlib.h"
typedef VMINT (*Resolver)(char*);
Resolver vm_get_sym_entry;
extern void vm_main(void);
extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);
void *malloc(size_t n){return n>0x7fffffff?NULL:vm_malloc((int)n);}
void *calloc(size_t n,size_t size){if(size&&n>0x7fffffff/size)return NULL;return vm_calloc((int)(n*size));}
void *realloc(void*p,size_t n){return n>0x7fffffff?NULL:vm_realloc(p,(int)n);}
void free(void*p){vm_free(p);}
void *_sbrk(ptrdiff_t n){(void)n;return (void*)-1;}
int _write(int fd,const void*p,size_t n){(void)fd;(void)p;return (int)n;}
int _read(int fd,void*p,size_t n){(void)fd;(void)p;(void)n;return -1;}
int _close(int fd){(void)fd;return -1;}
int _lseek(int fd,int offset,int whence){(void)fd;(void)offset;(void)whence;return -1;}
int _fstat(int fd,void*p){(void)fd;(void)p;return -1;}
int _isatty(int fd){(void)fd;return 0;}
int _getpid(void){return 1;}
int _kill(int pid,int sig){(void)pid;(void)sig;return -1;}
void _exit(int code){(void)code;vm_exit_app();for(;;){}}
void gcc_entry(unsigned entry,unsigned array,unsigned count){unsigned i;void (**p)(void);vm_get_sym_entry=(Resolver)entry;if(!array){p=__init_array_start;count=(unsigned)(__init_array_end-__init_array_start);}else p=(void(**)(void))array;for(i=0;i<count;i++)p[i]();vm_main();}
