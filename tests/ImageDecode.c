#include "../native/vm.c"
#define main unused_host_main
#include "../native/host.c"
#undef main
int main(int argc,char**argv){
    if(argc!=3)return 2;size_t n,baseline=pj_memory();char error[160]={0};
    unsigned char*d=platform_read(argv[1],PJ_ENTRY_LIMIT,&n);if(!d)return 2;
    PJImage*i=png_decode(d,n,error);pj_free(d);
    if(!i){fprintf(stderr,"%s\n",error);return pj_memory()!=baseline?3:1;}
    FILE*f=fopen(argv[2],"wb");if(!f){pj_free(i);return 2;}
    fprintf(f,"P6\n%d %d\n255\n",i->w,i->h);
    for(size_t k=0;k<(size_t)i->w*i->h;k++){
        uint16_t c=i->indices?i->palette[image_index(i,k)]:i->pixels[k];
        uint32_t rgb=expand565(c);if(i->alpha&&!(i->alpha[k/8]&(1u<<(k%8))))rgb=0x808080;
        unsigned char pixel[3]={(unsigned char)(rgb>>16),(unsigned char)(rgb>>8),(unsigned char)rgb};fwrite(pixel,1,3,f);
    }
    fclose(f);pj_free(i);return pj_memory()!=baseline?3:0;
}
