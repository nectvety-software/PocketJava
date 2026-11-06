#include "pocket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static unsigned char pixels[240*320*3];
static unsigned draws;
static uint32_t ticks;
int platform_tone(int note,int duration,int volume){printf("TONE note=%d duration=%d volume=%d (host simulation)\n",note,duration,volume);return 1;}
void platform_tone_stop(void){}
intptr_t platform_open(const char*path,size_t*size){FILE*f=fopen(path,"rb");long n;*size=0;if(!f)return -1;if(fseek(f,0,SEEK_END)||(n=ftell(f))<0){fclose(f);return -1;}*size=(size_t)n;return (intptr_t)f;}
int platform_read_at(intptr_t file,size_t offset,void*out,size_t n){FILE*f=(FILE*)file;return !fseek(f,(long)offset,SEEK_SET)&&fread(out,1,n,f)==n;}
void platform_close(intptr_t file){if(file!=-1)fclose((FILE*)file);}
unsigned char *platform_read(const char*path,size_t limit,size_t*n){FILE*f=fopen(path,"rb");long len;unsigned char*d;*n=0;if(!f)return NULL;if(fseek(f,0,SEEK_END)|| (len=ftell(f))<0||(size_t)len>limit){fclose(f);return NULL;}rewind(f);d=pj_alloc((size_t)len);if(!d){fclose(f);return NULL;}if(fread(d,1,(size_t)len,f)!=(size_t)len){pj_free(d);fclose(f);return NULL;}fclose(f);*n=(size_t)len;return d;}
static void rect(int x,int y,int w,int h,uint32_t color){int xx,yy;draws++;for(yy=y;yy<y+h&&yy<320;yy++)for(xx=x;xx<x+w&&xx<240;xx++)if(xx>=0&&yy>=0){unsigned char*p=pixels+(yy*240+xx)*3;p[0]=(unsigned char)(color>>16);p[1]=(unsigned char)(color>>8);p[2]=(unsigned char)color;}}
static void text(int x,int y,const char*s,uint32_t color){(void)x;(void)y;(void)color;draws++;printf("TEXT %s\n",s);}
static uint32_t ms(void){return ticks;}
int main(int argc,char**argv){PJ*v;unsigned i,steps=1000;int ok;FILE*f;size_t baseline=pj_memory();if(argc<2){fprintf(stderr,"Usage: pocketjava.exe file.jar|jad [steps] [output.ppm]\n");return 2;}if(argc>2)steps=(unsigned)strtoul(argv[2],NULL,10);v=pj_create((PJPlatform){rect,text,ms});ok=v&&pj_load(v,argv[1]);
    for(i=0;ok&&i<steps;i++){ticks+=PJ_FRAME_MS;if(argc>4){const char*event=argv[4];while(*event){unsigned at;int key,used=0;if(sscanf(event,"%u:%d%n",&at,&key,&used)!=2)break;if(i==at){pj_key(v,key,0);}event+=used;if(*event==',')event++;else break;}}else{if(i==20)pj_key(v,-4,0);if(i==40)pj_key(v,-2,0);}ok=pj_step(v,PJ_SLICE_OPCODES,PJ_SLICE_MS);if(!pj_busy(v))pj_repaint(v);}
    if(!ok)pj_debug_dump(v);
    printf("RESULT ok=%d instructions=%llu draws=%u memory=%zu peak=%zu error=%s\n",ok,(unsigned long long)pj_instructions(v),draws,pj_memory(),pj_peak(),pj_error(v));
    if(argc>3&&ok){f=fopen(argv[3],"wb");if(f){fprintf(f,"P6\n240 320\n255\n");fwrite(pixels,1,sizeof(pixels),f);fclose(f);}}
    pj_destroy(v);if(pj_memory()!=baseline){fprintf(stderr,"Memory leak: %zu\n",pj_memory()-baseline);return 3;}return ok?0:1;
}


unsigned char *platform_record_read(uint32_t store,int id,size_t*n){char path[100];snprintf(path,sizeof(path),"build/rms-%08lx-%d.dat",(unsigned long)store,id);return platform_read(path,65536,n);}
int platform_record_write(uint32_t store,int id,const void*data,size_t n){char path[100];snprintf(path,sizeof(path),"build/rms-%08lx-%d.dat",(unsigned long)store,id);FILE*f=fopen(path,"wb");if(!f)return 0;int ok=fwrite(data,1,n,f)==n;return fclose(f)==0&&ok;}
