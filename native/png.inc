/* Streaming PNG decoder: one RGB565 texture, a binary alpha mask, two rows
 * and a bounded DEFLATE dictionary. Supports non-interlaced <=8-bit PNG. */
typedef struct {int w,h;uint16_t *pixels;uint8_t *alpha,*indices;uint16_t *palette,colors;uint8_t mutable,packed4;} PJImage;
typedef struct {tinfl_decompressor state;const uint8_t*input;size_t remaining,op,on,dict;int done;uint8_t output[32768];} PNGInflate;
static uint32_t png32(const uint8_t*p){return (uint32_t)p[0]<<24|(uint32_t)p[1]<<16|(uint32_t)p[2]<<8|p[3];}
static uint16_t rgb332_565(uint8_t k){unsigned r=(k>>5)*255/7,g=((k>>2)&7)*255/7,b=(k&3)*255/3;return (uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));}
static uint8_t image_index(PJImage*i,size_t k){return i->packed4?(uint8_t)((k&1)?(i->indices[k>>1]&15):(i->indices[k>>1]>>4)):i->indices[k];}
static int png_byte(PNGInflate*s){if(s->op==s->on){if(s->done)return -1;size_t in=s->remaining,out=32768-s->dict;tinfl_status status=tinfl_decompress(&s->state,s->input,&in,s->output,s->output+s->dict,&out,TINFL_FLAG_PARSE_ZLIB_HEADER);s->input+=in;s->remaining-=in;if(status<0||!out)return -1;s->done=status==TINFL_STATUS_DONE;s->op=s->dict;s->on=s->dict+out;s->dict=(s->dict+out)&32767;}return s->output[s->op++];}
static uint8_t paeth(uint8_t a,uint8_t b,uint8_t c){int p=(int)a+b-c,pa=p-a,pb=p-b,pc=p-c;if(pa<0)pa=-pa;if(pb<0)pb=-pb;if(pc<0)pc=-pc;return pa<=pb&&pa<=pc?a:pb<=pc?b:c;}
static PJImage *image_alloc(int w,int h,int alpha){if(w<=0||h<=0||w>1024||h>1024||(size_t)w*h>PJ_ENTRY_LIMIT/3)return NULL;size_t n=(size_t)w*h,mask=alpha?(n+7)/8:0;PJImage*i=pj_alloc(sizeof(*i)+n*2+mask);if(!i)return NULL;i->indices=NULL;i->palette=NULL;i->mutable=1;i->w=w;i->h=h;i->pixels=(uint16_t*)(i+1);i->alpha=alpha?(uint8_t*)(i->pixels+n):NULL;memset(i->pixels,255,n*2);if(i->alpha)memset(i->alpha,255,mask);return i;}
static PJImage *image_alloc_mutable(int w,int h){
    if(w<=0||h<=0||w>1024||h>1024||(size_t)w*h>PJ_ENTRY_LIMIT/3)return NULL;
    size_t n=(size_t)w*h;
    if(n<512)return image_alloc(w,h,0);
    PJImage*i=pj_alloc(sizeof(*i)+512+n);if(!i)return NULL;memset(i,0,sizeof(*i));
    i->w=w;i->h=h;i->mutable=1;i->colors=1;i->palette=(uint16_t*)(i+1);i->indices=(uint8_t*)(i->palette+256);i->palette[0]=0xffff;memset(i->indices,0,n);return i;
}
static int image_palette_index(PJImage*i,uint16_t c){
    for(unsigned k=0;k<i->colors;k++)if(i->palette[k]==c)return (int)k;
    if(i->colors<256){unsigned k=i->colors++;i->palette[k]=c;return (int)k;}
    return -1;
}
static PJImage *image_promote(Object*holder,PJImage*i){
    if(!i||!i->indices||!i->mutable)return i;
    size_t n=(size_t)i->w*i->h;uint16_t pal[256];memcpy(pal,i->palette,sizeof(pal));
    PJImage*r=pj_realloc(i,sizeof(*i)+n*2);if(!r)return NULL;
    uint8_t*idx=(uint8_t*)((uint16_t*)(r+1)+256);uint16_t*pix=(uint16_t*)(r+1);
    for(size_t k=n;k-->0;)pix[k]=pal[idx[k]];
    r->pixels=pix;r->indices=NULL;r->palette=NULL;r->alpha=NULL;r->colors=0;holder->native=r;return r;
}
static PJImage *image_compact_palette(PJImage*i){
    if(!i||!i->indices||i->mutable||!i->colors)return i;
    size_t n=(size_t)i->w*i->h,mask=i->alpha?(n+7)/8:0;uint8_t used[256]={0},map[256];unsigned count=0,oldcolors=i->colors;
    for(size_t k=0;k<n;k++)used[i->indices[k]]=1;
    for(unsigned k=0;k<oldcolors;k++)if(used[k])map[k]=(uint8_t)count++;
    if(!count)return i;
    uint16_t oldpal[256];memcpy(oldpal,i->palette,(size_t)oldcolors*2);
    for(unsigned k=0;k<oldcolors;k++)if(used[k])i->palette[map[k]]=oldpal[k];
    uint8_t*oldidx=i->indices,*oldalpha=i->alpha,*newidx=(uint8_t*)(i->palette+count);size_t ibytes=count<=16?(n+1)/2:n;
    if(count<=16){for(size_t k=0;k<n;k++){uint8_t v=map[oldidx[k]];if(!(k&1))newidx[k>>1]=(uint8_t)(v<<4);else newidx[k>>1]|=(uint8_t)(v&15);}}
    else {for(size_t k=0;k<n;k++)newidx[k]=map[oldidx[k]];}
    if(oldalpha)memmove(newidx+ibytes,oldalpha,mask);
    size_t bytes=sizeof(*i)+(size_t)count*2+ibytes+mask;PJImage*r=pj_realloc(i,bytes);if(!r)r=i;
    r->colors=(uint16_t)count;r->palette=(uint16_t*)(r+1);r->indices=(uint8_t*)(r->palette+count);r->packed4=count<=16;r->alpha=mask?r->indices+ibytes:NULL;return r;
}
#include "gif.inc"
static PJImage *png_decode(const uint8_t*d,size_t n,char*error){
    if(n>=6&&(!memcmp(d,"GIF87a",6)||!memcmp(d,"GIF89a",6)))return gif_decode(d,n,error);
    int w=0,h=0,depth=0,type=-1,channels=0;size_t pos=8,total=0,rowbytes=0,bpp=0;const uint8_t*single=NULL;uint8_t palette[256][4];unsigned colors=0;int tr=-1,tg=-1,tb=-1;
    if(n<33||memcmp(d,"\x89PNG\r\n\x1a\n",8)){strcpy(error,"Invalid PNG signature");return NULL;}
    memset(palette,255,sizeof(palette));
    while(pos+12<=n){uint32_t len=png32(d+pos);const uint8_t*tag=d+pos+4,*data=d+pos+8;if(len>n-pos-12){strcpy(error,"Truncated PNG chunk");return NULL;}if(!memcmp(tag,"IHDR",4)){if(len!=13||data[12]){strcpy(error,"PNG interlace unsupported");return NULL;}w=(int)png32(data);h=(int)png32(data+4);depth=data[8];type=data[9];}
        else if(!memcmp(tag,"PLTE",4)){colors=len/3;if(colors>256){strcpy(error,"PNG palette invalid");return NULL;}for(unsigned k=0;k<colors;k++)memcpy(palette[k],data+k*3,3);}
        else if(!memcmp(tag,"tRNS",4)){if(type==3){if(len>256){strcpy(error,"PNG transparency invalid");return NULL;}for(unsigned k=0;k<len;k++)palette[k][3]=data[k];}else if(type==0&&len>=2)tr=be16(data);else if(type==2&&len>=6){tr=be16(data);tg=be16(data+2);tb=be16(data+4);}}
        else if(!memcmp(tag,"IDAT",4)){if(!total)single=data;else single=NULL;total+=len;}
        pos+=(size_t)len+12;
    }
    channels=type==0||type==3?1:type==2?3:type==4?2:type==6?4:0;
    /* Font and sprite strips can be tall while using very few pixels. Bound
     * decoded area instead of rejecting small textures above 1024 pixels. */
    if(!channels||!(depth==1||depth==2||depth==4||depth==8)||(type!=0&&type!=3&&depth!=8)||(type==3&&!colors)||!total||w<=0||h<=0||w>32767||h>32767||(size_t)w*h>PJ_ENTRY_LIMIT/3){strcpy(error,"PNG format/dimensions unsupported");return NULL;}
    rowbytes=((size_t)w*channels*depth+7)/8;bpp=((size_t)channels*depth+7)/8;if(!bpp)bpp=1;
    int needs_alpha=type==4||type==6||((type==0||type==2)&&tr>=0);if(type==3)for(unsigned k=0;k<colors;k++)if(palette[k][3]<128){needs_alpha=1;break;}
    uint8_t*joined=NULL;if(!single){joined=pj_alloc(total);if(!joined){strcpy(error,"PNG compressed data RAM limit");return NULL;}size_t off=0;pos=8;while(pos+12<=n){size_t len=png32(d+pos);if(!memcmp(d+pos+4,"IDAT",4)){memcpy(joined+off,d+pos+8,len);off+=len;}pos+=len+12;}single=joined;}
    PJImage*image=NULL;if(type>=0){size_t count=(size_t)w*h,mask=needs_alpha?(count+7)/8:0;unsigned palcount=type==3?colors:0;image=pj_alloc(sizeof(*image)+(size_t)palcount*2+count+mask);if(image){memset(image,0,sizeof(*image));image->w=w;image->h=h;image->colors=(uint16_t)palcount;image->palette=palcount?(uint16_t*)(image+1):NULL;image->indices=(uint8_t*)(image+1)+(size_t)palcount*2;image->alpha=needs_alpha?image->indices+count:NULL;if(image->alpha)memset(image->alpha,255,mask);for(unsigned k=0;k<palcount;k++){unsigned r=palette[k][0],g=palette[k][1],b=palette[k][2];image->palette[k]=(uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));}}}else image=image_alloc(w,h,1);PNGInflate*inflate=pj_alloc(sizeof(*inflate));uint8_t*rows=pj_alloc(rowbytes*2);
    if(!image||!inflate||!rows){strcpy(error,"PNG decode RAM limit");
#ifndef MRE
        printf("PNG RAM: %dx%d used=%zu image=%d inflater=%d rows=%d\n",w,h,pj_memory(),!!image,!!inflate,!!rows);
#endif
        pj_free(image);pj_free(inflate);pj_free(rows);pj_free(joined);return NULL;}
    memset(inflate,0,sizeof(*inflate));tinfl_init(&inflate->state);inflate->input=single;inflate->remaining=total;memset(rows,0,rowbytes*2);
    uint8_t*previous=rows,*current=rows+rowbytes;int valid=1;
    for(int y=0;y<h&&valid;y++){int filter=png_byte(inflate);if(filter<0||filter>4){valid=0;break;}for(size_t x=0;x<rowbytes;x++){int byte=png_byte(inflate);if(byte<0){valid=0;break;}uint8_t left=x>=bpp?current[x-bpp]:0,up=previous[x],ul=x>=bpp?previous[x-bpp]:0;current[x]=(uint8_t)(byte+(filter==1?left:filter==2?up:filter==3?((unsigned)left+up)/2:filter==4?paeth(left,up,ul):0));}
        if(!valid)break;for(int x=0;x<w;x++){unsigned r=0,g=0,b=0,a=255,index=0;if(type==3||type==0){unsigned bit=(unsigned)x*depth,raw=(current[bit/8]>>(8-depth-bit%8))&((1u<<depth)-1);if(type==3){index=raw;if(raw>=colors){valid=0;break;}r=palette[raw][0];g=palette[raw][1];b=palette[raw][2];a=palette[raw][3];}else {r=g=b=raw*255/((1u<<depth)-1);if((int)raw==tr)a=0;}}
            else if(type==2){r=current[x*3];g=current[x*3+1];b=current[x*3+2];if((int)r==tr&&(int)g==tg&&(int)b==tb)a=0;}
            else if(type==4){r=g=b=current[x*2];a=current[x*2+1];}else {r=current[x*4];g=current[x*4+1];b=current[x*4+2];a=current[x*4+3];}
            size_t idx=(size_t)y*w+x;if(image->indices)image->indices[idx]=(uint8_t)(type==3?index:((r>>5)<<5|(g>>5)<<2|(b>>6)));else image->pixels[idx]=(uint16_t)((r>>3)<<11|(g>>2)<<5|(b>>3));if(a<128&&image->alpha)image->alpha[idx/8]&=(uint8_t)~(1u<<(idx%8));}
        uint8_t*temp=previous;previous=current;current=temp;
    }
    pj_free(inflate);pj_free(rows);pj_free(joined);if(!valid){pj_free(image);strcpy(error,"Invalid PNG scanline data");return NULL;}if(image&&image->indices)image=image_compact_palette(image);return image;
}
static uint32_t expand565(uint16_t c){return (uint32_t)((c>>11)*255/31)<<16|(uint32_t)(((c>>5)&63)*255/63)<<8|(c&31)*255/31;}
static void gfx_rect(PJ*v,int32_t graphics,int x,int y,int w,int h,uint32_t color){
    Object*g=obj(v,graphics);if(!g)return;PJImage*target=NULL;Object*holder=NULL;
    if(g->data[0]){holder=obj(v,g->data[0]);if(!holder)return;target=holder->native;}
    int width=target?target->w:240,height=target?target->h:320;int64_t right=(int64_t)x+w,bottom=(int64_t)y+h;
    if(x<v->cx)x=v->cx;if(y<v->cy)y=v->cy;if(right>(int64_t)v->cx+v->cw)right=(int64_t)v->cx+v->cw;if(bottom>(int64_t)v->cy+v->ch)bottom=(int64_t)v->cy+v->ch;
    if(x<0)x=0;if(y<0)y=0;if(right>width)right=width;if(bottom>height)bottom=height;if(right<=x||bottom<=y)return;
    if(!target){v->p.rect(x,y,(int)right-x,(int)bottom-y,color);return;}
    uint16_t c=(uint16_t)(((color>>19)&31)<<11|((color>>10)&63)<<5|((color>>3)&31));
    if(target->indices){
        int pi=image_palette_index(target,c);
        if(pi<0){target=image_promote(holder,target);if(!target){err(v,"Mutable image RAM limit");return;}}
        if(target->indices){for(int yy=y;yy<bottom;yy++)for(int xx=x;xx<right;xx++)target->indices[(size_t)yy*width+xx]=(uint8_t)pi;return;}
    }
    for(int yy=y;yy<bottom;yy++)for(int xx=x;xx<right;xx++){size_t idx=(size_t)yy*width+xx;target->pixels[idx]=c;if(target->alpha)target->alpha[idx/8]|=(uint8_t)(1u<<(idx%8));}
}
static void gfx_image(PJ*v,int32_t graphics,PJImage*img,int sx,int sy,int sw,int sh,int transform,int dx,int dy,int anchor){if(!img||sx<0||sy<0||sw<0||sh<0||(int64_t)sx+sw>img->w||(int64_t)sy+sh>img->h||transform<0||transform>7){throw_exception(v,"java/lang/IllegalArgumentException",NULL);return;}int width=transform>=4?sh:sw,height=transform>=4?sw:sh;
    int64_t ox=(int64_t)dx+v->tx,oy=(int64_t)dy+v->ty;
    if(anchor&8)ox-=width;else if(anchor&1)ox-=width/2;if(anchor&32)oy-=height;else if(anchor&2)oy-=height/2;
    Object*g=obj(v,graphics);if(!g)return;PJImage*target=NULL;if(g->data[0]){Object*holder=obj(v,g->data[0]);if(!holder)return;target=holder->native;}
    int tw=target?target->w:240,th=target?target->h:320;
    int64_t left=v->cx>0?v->cx:0,top=v->cy>0?v->cy:0,right=(int64_t)v->cx+v->cw,bottom=(int64_t)v->cy+v->ch;
    if(right>tw)right=tw;if(bottom>th)bottom=th;
    if(left<ox)left=ox;if(top<oy)top=oy;if(right>ox+width)right=ox+width;if(bottom>oy+height)bottom=oy+height;
    if(right<=left||bottom<=top)return;
    /* Only decode visible source pixels, preserving transform coordinates. */
    int x0=(int)(left-ox),xend=(int)(right-ox),y0=(int)(top-oy),yend=(int)(bottom-oy);
    for(int y=y0;y<yend;y++){int run=0;uint16_t last=0;for(int x=x0;x<=xend;x++){int opaque=0;uint16_t color=0;if(x<xend){int xx=x,yy=y;switch(transform){case 1:yy=sh-1-y;break;case 2:xx=sw-1-x;break;case 3:xx=sw-1-x;yy=sh-1-y;break;case 4:xx=y;yy=x;break;case 5:xx=y;yy=sh-1-x;break;case 6:xx=sw-1-y;yy=x;break;case 7:xx=sw-1-y;yy=sh-1-x;break;}size_t idx=(size_t)(sy+yy)*img->w+sx+xx;color=img->indices?(img->palette?img->palette[image_index(img,idx)]:rgb332_565(image_index(img,idx))):img->pixels[idx];opaque=!img->alpha||!!(img->alpha[idx/8]&(1u<<(idx%8)));}
            if(run&&(!opaque||color!=last)){gfx_rect(v,graphics,(int)(ox+x-run),(int)(oy+y),run,1,expand565(last));run=0;}if(opaque){last=color;run++;}}
    }
}
