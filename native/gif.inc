/* Static first frame of GIF87a/89a. LZW dictionary lives on the heap. */
typedef struct {const uint8_t*d;size_t n,p;unsigned left,bits;uint32_t value;} GIFBits;
static int gif_byte(GIFBits*s){if(!s->left){if(s->p>=s->n)return -1;s->left=s->d[s->p++];if(!s->left||s->left>s->n-s->p)return -1;}s->left--;return s->d[s->p++];}
static int gif_code(GIFBits*s,unsigned width){while(s->bits<width){int b=gif_byte(s);if(b<0)return -1;s->value|=(uint32_t)b<<s->bits;s->bits+=8;}int c=(int)(s->value&((1u<<width)-1));s->value>>=width;s->bits-=width;return c;}
static unsigned gif16(const uint8_t*p){return p[0]|(unsigned)p[1]<<8;}
static int gif_skip(const uint8_t*d,size_t n,size_t*p){while(*p<n){unsigned len=d[(*p)++];if(!len)return 1;if(len>n-*p)return 0;*p+=len;}return 0;}
static PJImage *gif_decode(const uint8_t*d,size_t n,char*error){
    size_t p=13;unsigned w,h,colors=0;int transparent=-1;const uint8_t*pal=NULL;PJImage*img=NULL;
    typedef struct {uint16_t prefix[4096];uint8_t suffix[4096],stack[4097];} GIFWork;
    GIFWork*work=NULL;
    if(n<13)goto bad;w=gif16(d+6);h=gif16(d+8);
    if(!w||!h||(size_t)w*h>PJ_ENTRY_LIMIT/3)goto bad;
    if(d[10]&128){colors=2u<<(d[10]&7);if(colors*3>n-p)goto bad;pal=d+p;p+=colors*3;}
    while(p<n){unsigned tag=d[p++];
        if(tag==0x21){if(p>=n)goto bad;unsigned label=d[p++];
            if(label==0xf9){if(n-p<6||d[p]!=4||d[p+5])goto bad;transparent=(d[p+1]&1)?d[p+4]:-1;p+=6;}
            else if(!gif_skip(d,n,&p))goto bad;
            continue;
        }
        if(tag!=0x2c||n-p<9)goto bad;
        unsigned left=gif16(d+p),top=gif16(d+p+2),iw=gif16(d+p+4),ih=gif16(d+p+6),flags=d[p+8];p+=9;
        if(!iw||!ih||left+iw>w||top+ih>h)goto bad;
        if(flags&128){colors=2u<<(flags&7);if(colors*3>n-p)goto bad;pal=d+p;p+=colors*3;}
        if(!pal||p>=n||transparent>=(int)colors)goto bad;
        unsigned minimum=d[p++];if(minimum<2||minimum>8)goto bad;
        size_t count=(size_t)w*h,mask=transparent>=0?(count+7)/8:0;
        img=pj_alloc(sizeof(*img)+colors*2+count+mask);work=pj_alloc(sizeof(*work));
        if(!img||!work){strcpy(error,"GIF decode RAM limit");goto fail;}
        memset(img,0,sizeof(*img));img->w=(int)w;img->h=(int)h;img->colors=(uint16_t)colors;
        img->palette=(uint16_t*)(img+1);img->indices=(uint8_t*)(img->palette+colors);img->alpha=mask?img->indices+count:NULL;
        unsigned bg=d[11]<colors?d[11]:0;memset(img->indices,(int)bg,count);if(mask)memset(img->alpha,0,mask);
        for(unsigned k=0;k<colors;k++)img->palette[k]=(uint16_t)((pal[k*3]>>3)<<11|(pal[k*3+1]>>2)<<5|(pal[k*3+2]>>3));
        GIFBits bits={d,n,p,0,0,0};unsigned clear=1u<<minimum,end=clear+1,next=end+1,width=minimum+1;
        int previous=-1;unsigned first=0,x=0,y=0,pass=0;size_t produced=0;int finished=0;
        for(;;){int code=gif_code(&bits,width);if(code<0)goto bad;
            if((unsigned)code==clear){next=end+1;width=minimum+1;previous=-1;continue;}
            if((unsigned)code==end){finished=1;break;}
            unsigned original=(unsigned)code,used=0;
            if((unsigned)code==next){if(previous<0)goto bad;work->stack[used++]=(uint8_t)first;code=previous;}
            else if((unsigned)code>next)goto bad;
            while((unsigned)code>=clear){if((unsigned)code>=next||used>=4096)goto bad;work->stack[used++]=work->suffix[code];code=work->prefix[code];}
            first=(unsigned)code;work->stack[used++]=(uint8_t)first;
            while(used){unsigned index=work->stack[--used];if(index>=colors||produced>=(size_t)iw*ih||y>=ih)goto bad;
                size_t at=(size_t)(top+y)*w+left+x;img->indices[at]=(uint8_t)index;
                if(img->alpha&&index!=(unsigned)transparent)img->alpha[at/8]|=(uint8_t)(1u<<(at%8));
                produced++;if(++x==iw){x=0;if(flags&64){static const unsigned starts[4]={0,4,2,1},steps[4]={8,8,4,2};y+=steps[pass];while(y>=ih&&pass<3)y=starts[++pass];}else y++;}
            }
            if(previous>=0&&next<4096){work->prefix[next]=(uint16_t)previous;work->suffix[next]=(uint8_t)first;next++;if(next==(1u<<width)&&width<12)width++;}
            previous=(int)original;
        }
        if(!finished||produced!=(size_t)iw*ih)goto bad;
        pj_free(work);return image_compact_palette(img);
    }
bad:strcpy(error,"Invalid GIF image data");
fail:pj_free(work);pj_free(img);return NULL;
}
