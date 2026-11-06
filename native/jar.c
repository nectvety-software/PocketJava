#include "pocket.h"
#include "miniz_tinfl.h"
#include <string.h>
static uint16_t le16(const unsigned char*p){return p[0]|(uint16_t)p[1]<<8;}
static uint32_t le32(const unsigned char*p){return le16(p)|(uint32_t)le16(p+2)<<16;}
static int fail(char *e,const char*s){strncpy(e,s,PJ_ERROR_CAP-1);e[PJ_ERROR_CAP-1]=0;return 0;}
static uint32_t crc32(const unsigned char*p,size_t n){uint32_t c=~0u;size_t i;int k;for(i=0;i<n;i++){c^=p[i];for(k=0;k<8;k++)c=(c>>1)^(0xedb88320u& (0u-(c&1u)));}return ~c;}
/* DEFLATE state is too large for the small native callback stack on phones. */
static size_t inflate_memory(void*out,size_t size,const void*input,size_t packed){tinfl_decompressor*state=pj_alloc(sizeof(*state));if(!state)return (size_t)-1;tinfl_init(state);size_t consumed=packed,written=size;tinfl_status status=tinfl_decompress(state,input,&consumed,out,out,&written,TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);pj_free(state);return status==TINFL_STATUS_DONE?written:(size_t)-1;}
static int inflate_file(PJJar*j,size_t offset,size_t packed,unsigned char*out,size_t size){
    typedef struct {tinfl_decompressor state;unsigned char input[1024];} Scratch;
    Scratch*scratch=pj_alloc(sizeof(*scratch));if(!scratch)return 0;size_t ip=0,in=0,total=0;int ok=0;tinfl_init(&scratch->state);
    for(;;){if(ip==in&&packed){in=packed>sizeof(scratch->input)?sizeof(scratch->input):packed;if(!platform_read_at(j->file,offset,scratch->input,in))break;offset+=in;packed-=in;ip=0;}size_t used=in-ip,written=size-total;unsigned flags=TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF|(packed?TINFL_FLAG_HAS_MORE_INPUT:0);tinfl_status status=tinfl_decompress(&scratch->state,scratch->input+ip,&used,out,out+total,&written,flags);ip+=used;total+=written;if(status==TINFL_STATUS_DONE){ok=total==size;break;}if(status<0||(!used&&!written)||total>size)break;}
    pj_free(scratch);return ok;
}

int jar_open(PJJar*j,unsigned char*d,size_t n,char*e){
    size_t p,low,q;uint32_t cdlen;unsigned i;
    memset(j,0,sizeof(*j));j->file=-1;
    if(n<22||n>PJ_JAR_LIMIT)return fail(e,"JAR size invalid (max 8 MiB)");
    low=n>65557?n-65557:0;p=n-22;
    for(;;){if(le32(d+p)==0x06054b50u && p+22+le16(d+p+20)==n)break;if(p==low)return fail(e,"ZIP end directory missing");p--;}
    if(le16(d+p+4)||le16(d+p+6)||le16(d+p+8)!=le16(d+p+10))return fail(e,"Multi-disk ZIP unsupported");
    j->entries=le16(d+p+10);j->cd=le32(d+p+16);cdlen=le32(d+p+12);
    if(j->entries==65535||j->entries>2048||j->cd>p||cdlen>p-j->cd)return fail(e,"ZIP directory invalid / ZIP64 unsupported");
    j->end=j->cd+cdlen;q=j->cd;
    for(i=0;i<j->entries;i++){
        size_t len;
        if(q>j->end||j->end-q<46||le32(d+q)!=0x02014b50u)return fail(e,"ZIP entry invalid");
        len=46u+le16(d+q+28)+le16(d+q+30)+le16(d+q+32);
        if(len>j->end-q)return fail(e,"ZIP entry truncated");q+=len;
    }
    if(q!=j->end)return fail(e,"ZIP directory length mismatch");
    j->data=d;j->directory=d+j->cd;j->size=n;return 1;
}
void jar_close(PJJar*j){if(j->file!=-1){platform_close(j->file);pj_free(j->directory);}pj_free(j->data);memset(j,0,sizeof(*j));j->file=-1;}
int jar_open_file(PJJar*j,const char*path,char*e){
    size_t n,tailn,p,base,q;unsigned char*tail;uint32_t cdlen;unsigned i;
    memset(j,0,sizeof(*j));j->file=platform_open(path,&n);
    if(j->file==-1)return fail(e,"Cannot open JAR file");
    if(n<22||n>PJ_JAR_LIMIT){jar_close(j);return fail(e,"JAR size invalid (max 8 MiB)");}
    tailn=n>65557?65557:n;base=n-tailn;tail=pj_alloc(tailn);
    if(!tail){jar_close(j);return fail(e,"Memory limit: ZIP directory");}
    if(!platform_read_at(j->file,base,tail,tailn)){pj_free(tail);jar_close(j);return fail(e,"Cannot read ZIP directory");}
    p=tailn-22;for(;;){if(le32(tail+p)==0x06054b50u&&p+22+le16(tail+p+20)==tailn)break;if(!p){pj_free(tail);jar_close(j);return fail(e,"ZIP end directory missing");}p--;}
    j->entries=le16(tail+p+10);j->cd=le32(tail+p+16);cdlen=le32(tail+p+12);
    if(le16(tail+p+4)||le16(tail+p+6)||le16(tail+p+8)!=j->entries||j->entries>2048||j->cd>base+p||cdlen>base+p-j->cd||cdlen>65536){pj_free(tail);jar_close(j);return fail(e,"Invalid/unsupported ZIP directory");}
    pj_free(tail);j->directory=pj_alloc(cdlen);j->size=n;j->end=j->cd+cdlen;
    if(!j->directory||!platform_read_at(j->file,j->cd,j->directory,cdlen)){jar_close(j);return fail(e,"Cannot load ZIP directory");}
    q=0;for(i=0;i<j->entries;i++){size_t len;if(q>cdlen||cdlen-q<46||le32(j->directory+q)!=0x02014b50u){jar_close(j);return fail(e,"ZIP entry invalid");}len=46u+le16(j->directory+q+28)+le16(j->directory+q+30)+le16(j->directory+q+32);if(len>cdlen-q){jar_close(j);return fail(e,"ZIP entry truncated");}q+=len;}
    if(q!=cdlen){jar_close(j);return fail(e,"ZIP directory length mismatch");}return 1;
}
unsigned char *jar_extract(PJJar*j,const char*name,size_t*n,char*e){
    size_t q=0,namelen=strlen(name);unsigned i;unsigned char *out;
    *n=0;
    for(i=0;i<j->entries;i++){
        const unsigned char*p=j->directory+q;uint16_t len=le16(p+28);
        if(len==namelen&&!memcmp(p+46,name,len)){
            uint32_t packed=le32(p+20),size=le32(p+24),off=le32(p+42),start;uint16_t mode=le16(p+10),flags=le16(p+8);const unsigned char *local;unsigned char head[30],*compressed=NULL;
            if(flags&1){fail(e,"Encrypted JAR unsupported");return NULL;}
            if(size>PJ_ENTRY_LIMIT||off>j->cd||j->cd-off<30){fail(e,"JAR member too large / invalid");return NULL;}
            if(j->data)local=j->data+off;else {if(!platform_read_at(j->file,off,head,30)){fail(e,"Cannot read member header");return NULL;}local=head;}
            if(le32(local)!=0x04034b50u||le16(local+8)!=mode||le16(local+26)!=len||le16(local+6)!=flags){fail(e,"ZIP local header mismatch");return NULL;}
            start=off+30u+le16(local+26)+le16(local+28);
            if(start>j->cd||packed>j->cd-start){fail(e,"ZIP member truncated");return NULL;}
            if(j->data){if(memcmp(local+30,name,len)){fail(e,"ZIP member name mismatch");return NULL;}}
            else{unsigned char check[256];if(len>=sizeof(check)||!platform_read_at(j->file,off+30,check,len)||memcmp(check,name,len)){fail(e,"ZIP member name mismatch");return NULL;}}
            if(mode!=0&&mode!=8){fail(e,"ZIP compression unsupported");return NULL;}
            if(mode==0&&size!=packed){fail(e,"ZIP stored size mismatch");return NULL;}
            out=(unsigned char*)pj_alloc(size+1);if(!out){fail(e,"Memory limit loading JAR");return NULL;}
            if(mode==0){if(j->data)memcpy(out,j->data+start,size);else if(!platform_read_at(j->file,start,out,size)){pj_free(out);fail(e,"Cannot read stored member");return NULL;}}
            else {
                const unsigned char*input;
                if(j->data)input=j->data+start;
                else{if(!inflate_file(j,start,packed,out,size)){pj_free(out);fail(e,"Invalid streamed DEFLATE data");return NULL;}input=NULL;}
                if(input&&inflate_memory(out,size,input,packed)!=size){pj_free(compressed);pj_free(out);fail(e,"Invalid DEFLATE data");return NULL;}pj_free(compressed);
            }
            if(crc32(out,size)!=le32(p+16)){pj_free(out);fail(e,"JAR CRC32 mismatch");return NULL;}
            out[size]=0;*n=size;return out;
        }
        q+=46u+len+le16(p+30)+le16(p+32);
    }
    return NULL;
}
int jar_property(const unsigned char*d,size_t n,const char*key,char*out,size_t cap){
    size_t p=0,k=strlen(key),written=0;int found=0;
    if(!cap)return 0;out[0]=0;
    while(p<n){size_t s=p,end;while(p<n&&d[p]!='\n'&&d[p]!='\r')p++;end=p;while(p<n&&(d[p]=='\n'||d[p]=='\r'))p++;
        if(found){if(end>s&&d[s]==' ')s++;else break;}
        else {if(end-s<k+1||memcmp(d+s,key,k)||d[s+k]!=':')continue;found=1;s+=k+1;while(s<end&&d[s]==' ')s++;}
        if(end-s>=cap-written)return -1;
        memcpy(out+written,d+s,end-s);written+=end-s;out[written]=0;
    }
    return found;
}
struct PJZipStream {
    PJJar *j;
    uint32_t offset,packed,size,total,crc,expected;
    uint8_t method,done,eager,pad;
};
typedef struct {
    PJZipStream base;
    tinfl_decompressor inflater;
    size_t ip,in,op,on,dict;
    unsigned char input[128],output[32768];
} PJZipLarge;
typedef struct {
    PJZipStream base;
    uint16_t cache_start;
    uint8_t cache_len,pad;
    unsigned char cache[64];
} PJZipReplay;
static uint32_t crc_update(uint32_t c,const unsigned char*p,size_t n){for(size_t i=0;i<n;i++){c^=p[i];for(int k=0;k<8;k++)c=(c>>1)^(0xedb88320u&(0u-(c&1u)));}return c;}
static int jar_read(PJJar*j,size_t off,void*out,size_t n){if(off>j->size||n>j->size-off)return 0;if(j->data){memcpy(out,j->data+off,n);return 1;}return platform_read_at(j->file,off,out,n);}
static void *stream_alloc(PJ*v,size_t n){void*p=pj_alloc(n);if(!p&&v){pj_collect(v);p=pj_alloc(n);}return p;}
PJZipStream *jar_stream_open(PJ*v,PJJar*j,const char*name,char*e){
    size_t q=0,n=strlen(name);
    for(unsigned i=0;i<j->entries;i++){
        const unsigned char*p=j->directory+q;uint16_t len=le16(p+28);
        if(n==len&&!memcmp(p+46,name,n)){
            uint32_t off=le32(p+42),packed=le32(p+20),size=le32(p+24);unsigned char head[30],check[256];int method=le16(p+10);
            if(len>=256||size>PJ_ENTRY_LIMIT||le16(p+8)&1||(method!=0&&method!=8)||!jar_read(j,off,head,30)||le32(head)!=0x04034b50u||le16(head+8)!=method||le16(head+26)!=len){fail(e,"Invalid ZIP stream member");return NULL;}
            uint32_t start=off+30u+le16(head+26)+le16(head+28);
            if(start>j->cd||packed>j->cd-start||!jar_read(j,off+30,check,len)||memcmp(check,name,len)){fail(e,"Invalid ZIP stream bounds");return NULL;}
            if(method==8&&size<=1024u){
                PJZipReplay*r=stream_alloc(v,sizeof(*r));
                if(!r){fail(e,"ZIP stream RAM limit");return NULL;}
                memset(r,0,sizeof(*r));PJZipStream*s=&r->base;s->j=j;s->offset=start;s->packed=packed;s->size=size;s->expected=le32(p+16);s->method=8;s->eager=2;return s;
            }
            if(method==8&&size<=4096u){
                size_t bytes=sizeof(PJZipStream)+(size?size:1u);
                PJZipStream*s=stream_alloc(v,bytes);
                if(!s){fail(e,"ZIP stream RAM limit");return NULL;}
                memset(s,0,sizeof(*s));s->j=j;s->offset=start;s->packed=packed;s->size=size;s->expected=le32(p+16);s->method=8;s->eager=1;s->done=1;
                unsigned char*out=(unsigned char*)(s+1);int ok;
                if(j->data)ok=tinfl_decompress_mem_to_mem(out,size,j->data+start,packed,0)==size;
                else ok=inflate_file(j,start,packed,out,size);
                if(!ok||crc32(out,size)!=s->expected){pj_free(s);fail(e,"Invalid small DEFLATE stream");return NULL;}
                return s;
            }
            if(method==0){
                PJZipStream*s=stream_alloc(v,sizeof(*s));
                if(!s){fail(e,"ZIP stream RAM limit");return NULL;}
                memset(s,0,sizeof(*s));s->j=j;s->offset=start;s->packed=packed;s->size=size;s->crc=~0u;s->expected=le32(p+16);s->method=0;return s;
            }
            PJZipLarge*l=stream_alloc(v,sizeof(*l));
            if(!l){fail(e,"ZIP stream RAM limit");return NULL;}
            memset(l,0,sizeof(*l));PJZipStream*s=&l->base;s->j=j;s->offset=start;s->packed=packed;s->size=size;s->crc=~0u;s->expected=le32(p+16);s->method=8;tinfl_init(&l->inflater);return s;
        }
        q+=46u+len+le16(p+30)+le16(p+32);
    }
    return NULL;
}
void jar_stream_close(PJZipStream*s){pj_free(s);}
size_t jar_stream_available(PJZipStream*s){return s&&s->total<=s->size?(size_t)(s->size-s->total):0;}
int jar_stream_read(PJZipStream*s,void*out,size_t count,char*e){
    size_t copied=0;if(count>s->size-s->total)count=s->size-s->total;
    if(s->eager==2){
        PJZipReplay*r=(PJZipReplay*)s;unsigned char decoded[1024];
        while(copied<count){
            if(!r->cache_len||s->total<r->cache_start||s->total>=(uint32_t)r->cache_start+r->cache_len){
                int ok;if(s->j->data)ok=tinfl_decompress_mem_to_mem(decoded,s->size,s->j->data+s->offset,s->packed,0)==s->size;else ok=inflate_file(s->j,s->offset,s->packed,decoded,s->size);
                if(!ok){fail(e,"Invalid replay DEFLATE stream");return -1;}
                if(!s->done){if(crc32(decoded,s->size)!=s->expected){fail(e,"JAR CRC32 mismatch");return -1;}s->done=1;}
                r->cache_start=(uint16_t)s->total;size_t remain=s->size-s->total;r->cache_len=(uint8_t)(remain>sizeof(r->cache)?sizeof(r->cache):remain);memcpy(r->cache,decoded+s->total,r->cache_len);
            }
            size_t off=s->total-r->cache_start,n=r->cache_len-off;if(n>count-copied)n=count-copied;memcpy((unsigned char*)out+copied,r->cache+off,n);copied+=n;s->total+=(uint32_t)n;
        }
        return (int)copied;
    }
    if(s->eager==1){if(count)memcpy(out,(unsigned char*)(s+1)+s->total,count);s->total+=(uint32_t)count;return (int)count;}
    if(s->method==0){if(!jar_read(s->j,s->offset,out,count)){fail(e,"Cannot read ZIP stream");return -1;}s->offset+=(uint32_t)count;s->total+=(uint32_t)count;s->crc=crc_update(s->crc,out,count);if(s->total==s->size&&~s->crc!=s->expected){fail(e,"JAR CRC32 mismatch");return -1;}return (int)count;}
    PJZipLarge*l=(PJZipLarge*)s;
    while(copied<count){
        if(l->op==l->on){if(s->done){fail(e,"Truncated ZIP stream");return -1;}size_t produced;
            do{
                if(l->ip==l->in&&s->packed){l->in=s->packed>sizeof(l->input)?sizeof(l->input):s->packed;if(!jar_read(s->j,s->offset,l->input,l->in)){fail(e,"Cannot read compressed stream");return -1;}s->offset+=(uint32_t)l->in;s->packed-=(uint32_t)l->in;l->ip=0;}
                size_t input=l->in-l->ip;produced=sizeof(l->output)-l->dict;tinfl_status status=tinfl_decompress(&l->inflater,l->input+l->ip,&input,l->output,l->output+l->dict,&produced,s->packed?TINFL_FLAG_HAS_MORE_INPUT:0);l->ip+=input;
                if(status<0){fail(e,"Invalid ZIP DEFLATE stream");return -1;}if(status==TINFL_STATUS_DONE)s->done=1;
                if(!produced&&s->done){fail(e,"ZIP stream length mismatch");return -1;}
                if(!produced&&!s->packed&&l->ip==l->in){fail(e,"Truncated DEFLATE stream");return -1;}
            }while(!produced);
            l->op=l->dict;l->on=l->dict+produced;l->dict=(l->dict+produced)&32767;
        }
        size_t n=l->on-l->op;if(n>count-copied)n=count-copied;memcpy((unsigned char*)out+copied,l->output+l->op,n);s->crc=crc_update(s->crc,l->output+l->op,n);l->op+=n;copied+=n;s->total+=(uint32_t)n;
    }
    if(s->total==s->size&&~s->crc!=s->expected){fail(e,"JAR CRC32 mismatch");return -1;}return (int)copied;
}
