/* Additional CLDC/MIDP calls; allocations remain under the tracked heap cap. */
    if(b==B_VECTOR){
        if(IS("<init>","()V")||IS("<init>","(I)V")||IS("<init>","(II)V")){int capacity=desc[1]=='I'?a[1]:10;if(capacity<0)return throw_exception(v,"java/lang/IllegalArgumentException","Negative Vector capacity");if((unsigned)capacity>PJ_ENTRY_LIMIT/4){err(v,"Collection capacity limit");return -1;}o=obj(v,a[0]);if(!o)return -1;void*buffer=NULL;if(capacity){buffer=native_buffer_alloc((size_t)capacity*4);if(!buffer){err(v,"Vector RAM limit");return -1;}memset((uint8_t*)buffer+sizeof(uint32_t),0,(size_t)capacity*4);}pj_free(o->native);o->native=buffer;o->data[0]=0;o->data[1]=desc[2]=='I'?a[2]:0;return 1;}
        if(IS("capacity","()I")){o=obj(v,a[0]);if(!o)return -1;*out=(int32_t)(native_buffer_size(o)/4);return 1;}
    }
    if(b==B_STRING&&IS("toString","()Ljava/lang/String;")){*out=a[0];return 1;}
    if(b==B_STRING&&IS("valueOf","(C)Ljava/lang/String;")){char text[4];size_t bytes=0;uint32_t c=(uint16_t)a[0];if(!c){text[bytes++]=(char)0xc0;text[bytes++]=(char)0x80;}else utf8_put(text,&bytes,c);*out=string(v,(const uint8_t*)text,bytes);return *out?1:-1;}
    if(b==B_STRINGBUFFER&&IS("setLength","(I)V")){
        if(a[1]<0)return throw_exception(v,"java/lang/StringIndexOutOfBoundsException",NULL);o=obj(v,a[0]);if(!o)return -1;char*base=(char*)o->data;size_t old=utf_length(base),wanted=(size_t)a[1];
        if(wanted<=old){size_t bytes=(size_t)(utf_at(base,wanted)-base)+1;base[bytes-1]=0;void*small=pj_realloc(o->data,bytes);if(small)o->data=small;o->len=(uint32_t)bytes;return 1;}
        size_t bytes=strlen(base),extra=wanted-old;if(bytes>=PJ_ENTRY_LIMIT||extra>(PJ_ENTRY_LIMIT-bytes-1)/2){err(v,"StringBuffer size limit");return -1;}size_t need=bytes+extra*2+1;void*grown=pj_realloc(o->data,need);if(!grown){gc(v);grown=pj_realloc(o->data,need);}if(!grown){err(v,"StringBuffer RAM limit");return -1;}o->data=grown;char*text=grown;for(size_t i=0;i<extra;i++){text[bytes++]=(char)0xc0;text[bytes++]=(char)0x80;}text[bytes]=0;o->len=(uint32_t)need;return 1;
    }
    if(b==B_EXCEPTION&&IS("getMessage","()Ljava/lang/String;")){o=obj(v,a[0]);if(!o)return -1;*out=o->data[0];return 1;}
    if((b==B_INPUT||b==B_BYTEINPUT||b==B_DATAINPUT)&&(IS("markSupported","()Z")||IS("mark","(I)V")||IS("reset","()V"))){
        o=obj(v,a[0]);if(!o)return -1;int guard=0,type=b;
        while(type==B_DATAINPUT&&guard++<16){if(!o->data[0])return throw_exception(v,"java/io/IOException","Stream is closed");o=obj(v,o->data[0]);if(!o)return -1;type=v->classes[o->cls]->builtin;}
        if(type==B_DATAINPUT)return throw_exception(v,"java/io/IOException","Stream wrapper depth limit");
        if(IS("markSupported","()Z")){*out=type==B_BYTEINPUT;return 1;}
        if(IS("mark","(I)V")){if(type==B_BYTEINPUT)o->data[3]=o->data[1];return 1;}
        if(type!=B_BYTEINPUT)return throw_exception(v,"java/io/IOException","mark/reset not supported");o->data[1]=o->data[3];return 1;
    }
    if(b==B_BYTEINPUT){
        if(IS("close","()V"))return 1;
        if(IS("<init>","([BII)V")){Object*arr=obj(v,a[1]);o=obj(v,a[0]);if(!arr||!o)return -1;if(arr->kind!=18||a[2]<0||a[3]<0||(uint32_t)a[2]>arr->len)return throw_exception(v,"java/lang/IndexOutOfBoundsException",NULL);uint32_t remain=arr->len-(uint32_t)a[2],count=(uint32_t)a[3];if(count>remain)count=remain;o->data[0]=a[1];o->data[1]=o->data[3]=a[2];o->data[2]=a[2]+(int32_t)count;return 1;}
    }
    if(b==B_STRINGBUFFER&&(IS("delete","(II)Ljava/lang/StringBuffer;")||IS("deleteCharAt","(I)Ljava/lang/StringBuffer;"))){
        o=obj(v,a[0]);if(!o)return -1;char*base=(char*)o->data;size_t units=utf_length(base);int start=a[1];int64_t end=name[6]=='C'?(int64_t)start+1:a[2];
        if(start<0||(size_t)start>units||end<start||(name[6]=='C'&&(size_t)start==units))return throw_exception(v,"java/lang/StringIndexOutOfBoundsException",NULL);
        if((size_t)end>units)end=(int)units;char*first=(char*)utf_at(base,(size_t)start);const char*last=utf_at(base,(size_t)end);size_t removed=(size_t)(last-first),bytes=strlen(base)+1;memmove(first,last,strlen(last)+1);o->len=(uint32_t)(bytes-removed);void*small=pj_realloc(o->data,o->len);if(small)o->data=small;*out=a[0];return 1;
    }
    if(b==B_GRAPHICS&&(IS("setFont","(Ljavax/microedition/lcdui/Font;)V")||IS("getFont","()Ljavax/microedition/lcdui/Font;"))){
        o=obj(v,a[0]);if(!o)return -1;if(name[0]=='s'){if(a[1]&&!obj(v,a[1]))return -1;o->data[1]=a[1];return 1;}
        if(!o->data[1]){int32_t font=builtin_object(v,"javax/microedition/lcdui/Font");o=obj(v,a[0]);if(!o||!font)return -1;o->data[1]=font;}*out=o->data[1];return 1;
    }
    if((b==B_CANVAS||b==B_GAMECANVAS)&&(IS("keyPressed","(I)V")||IS("keyReleased","(I)V")||IS("keyRepeated","(I)V")))return 1;
    if(b==B_STRINGBUFFER&&IS("charAt","(I)C")){o=obj(v,a[0]);if(!o)return -1;const char*p=(const char*)o->data;size_t length=utf_length(p);if(a[1]<0||(size_t)a[1]>=length)return throw_exception(v,"java/lang/StringIndexOutOfBoundsException",NULL);for(int i=0;i<a[1];i++)utf_unit(&p);*out=(int32_t)utf_unit(&p);return 1;}
    if(b==B_LONG){
        if(IS("<init>","(J)V")){o=obj(v,a[0]);if(!o)return -1;o->data[0]=a[1];o->data[1]=a[2];return 1;}
        if(IS("longValue","()J")||IS("intValue","()I")){o=obj(v,a[0]);if(!o)return -1;v->native_high=o->data[0];*out=o->data[1];return 1;}
        if(IS("hashCode","()I")){o=obj(v,a[0]);if(!o)return -1;*out=o->data[0]^o->data[1];return 1;}
        if(IS("equals","(Ljava/lang/Object;)Z")){o=obj(v,a[0]);if(!o)return -1;if(!a[1]){*out=0;return 1;}Object*other=obj(v,a[1]);if(!other)return -1;*out=other->cls&&v->classes[other->cls]->builtin==B_LONG&&o->data[0]==other->data[0]&&o->data[1]==other->data[1];return 1;}
        if(IS("toString","(J)Ljava/lang/String;")||IS("toString","()Ljava/lang/String;")){uint64_t bits;if(desc[1]=='J')bits=(uint64_t)(uint32_t)a[0]<<32|(uint32_t)a[1];else{o=obj(v,a[0]);if(!o)return -1;bits=(uint64_t)(uint32_t)o->data[0]<<32|(uint32_t)o->data[1];}int negative=(int64_t)bits<0;uint64_t value=negative?0ull-bits:bits;char digits[21];size_t pos=sizeof(digits);do{digits[--pos]=(char)('0'+value%10);value/=10;}while(value);if(negative)digits[--pos]='-';*out=string(v,(const uint8_t*)digits+pos,sizeof(digits)-pos);return *out?1:-1;}
        if(IS("parseLong","(Ljava/lang/String;)J")||IS("parseLong","(Ljava/lang/String;I)J")){if(!a[0])return throw_exception(v,"java/lang/NumberFormatException","null");int radix=desc[19]=='I'?a[1]:10;if(radix<2||radix>36)return throw_exception(v,"java/lang/NumberFormatException","Invalid radix");const unsigned char*p=(const unsigned char*)str(v,a[0]);int negative=*p=='-';if(*p=='-'||*p=='+')p++;if(!*p)return throw_exception(v,"java/lang/NumberFormatException","Invalid long");int64_t limit=negative?INT64_MIN:-INT64_MAX,multmin=limit/radix,result=0;
            for(;*p;p++){int digit=integer_digit(*p);if(digit<0||digit>=radix||result<multmin)return throw_exception(v,"java/lang/NumberFormatException","Invalid long");result*=radix;if(result<limit+digit)return throw_exception(v,"java/lang/NumberFormatException","Long overflow");result-=digit;}uint64_t bits=(uint64_t)(negative?result:-result);v->native_high=(int32_t)(bits>>32);*out=(int32_t)bits;return 1;}
    }
    if(b==B_IMAGE&&IS("createImage","(Ljava/io/InputStream;)Ljavax/microedition/lcdui/Image;")){
        if(!a[0])return throw_exception(v,"java/lang/NullPointerException",NULL);uint8_t*data=NULL;size_t size=0,cap=0;unsigned char chunk[512];int got;
        while((got=stream_read(v,a[0],chunk,sizeof(chunk)))>0){if((size_t)got>PJ_ENTRY_LIMIT-size){pj_free(data);return throw_exception(v,"java/io/IOException","Image stream size limit");}if(size+(size_t)got>cap){size_t next=cap?cap*2:1024;while(next<size+(size_t)got)next*=2;if(next>PJ_ENTRY_LIMIT)next=PJ_ENTRY_LIMIT;void*grown=pj_realloc(data,next);if(!grown){gc(v);grown=pj_realloc(data,next);}if(!grown){pj_free(data);return throw_exception(v,"java/io/IOException","Image stream RAM limit");}data=grown;cap=next;}memcpy(data+size,chunk,(size_t)got);size+=(size_t)got;}
        if(got<0){pj_free(data);return -1;}char message[160]={0};PJImage*image=png_decode(data,size,message);pj_free(data);if(!image)return throw_exception(v,"java/io/IOException",message[0]?message:"Invalid image stream");*out=builtin_object(v,"javax/microedition/lcdui/Image");if(!*out){pj_free(image);return -1;}obj(v,*out)->native=image;return 1;
    }
    if(b==B_DATAOUTPUT&&IS("writeLong","(J)V")){uint64_t value=(uint64_t)(uint32_t)a[1]<<32|(uint32_t)a[2];unsigned char bytes[8];for(int i=7;i>=0;i--){bytes[i]=(unsigned char)value;value>>=8;}return stream_write(v,a[0],bytes,8);}
    if(b==B_DATAINPUT&&IS("readLong","()J")){unsigned char bytes[8];int got=stream_read(v,a[0],bytes,8);if(got<0)return -1;if(got!=8)return throw_exception(v,"java/io/EOFException",NULL);uint64_t value=0;for(int i=0;i<8;i++)value=value<<8|bytes[i];v->native_high=(int32_t)(value>>32);*out=(int32_t)value;return 1;}
    if(b==B_SHORT&&(IS("parseShort","(Ljava/lang/String;)S")||IS("parseShort","(Ljava/lang/String;I)S"))){int32_t value;int ok=parse_integer(v,a[0],desc[19]=='I'?a[1]:10,&value);if(ok!=1)return ok;if(value<-32768||value>32767)return throw_exception(v,"java/lang/NumberFormatException","Short overflow");*out=value;return 1;}
    if(b==B_RANDOM&&IS("setSeed","(J)V")){o=obj(v,a[0]);if(!o)return -1;uint64_t seed=(uint64_t)(uint32_t)a[1]<<32|(uint32_t)a[2];o->data[0]=(int32_t)((uint32_t)seed^(uint32_t)(seed>>32)^0xDEECE66Du);return 1;}
    if(b==B_CANVAS||b==B_GAMECANVAS){
        if(IS("hasPointerEvents","()Z")||IS("hasPointerMotionEvents","()Z")){*out=0;return 1;}
        if(IS("hasRepeatEvents","()Z")){*out=1;return 1;}
        if(IS("getKeyCode","(I)I")){switch(a[1]){case 1:*out=50;break;case 6:*out=56;break;case 2:*out=52;break;case 5:*out=54;break;case 8:*out=53;break;case 9:*out=49;break;case 10:*out=51;break;case 11:*out=55;break;case 12:*out=57;break;default:return throw_exception(v,"java/lang/IllegalArgumentException","Invalid game action");}return 1;}
        if(IS("getGameAction","(I)I")){*out=game_action(a[1]);return 1;}
    }
    if(b==B_GAMECANVAS){
        if(IS("paint","(Ljavax/microedition/lcdui/Graphics;)V")){o=obj(v,a[0]);if(!o)return -1;if(o->data[0]){Object*holder=obj(v,o->data[0]);if(!holder||!holder->native)return -1;gfx_image(v,a[1],holder->native,0,0,240,320,0,0,0,20);v->rendered=1;}return 1;}
        if(IS("<init>","(Z)V")){o=obj(v,a[0]);if(!o)return -1;o->data[1]=a[1]!=0;return 1;}
        if(IS("getKeyStates","()I")){*out=a[0]==v->canvas?(int32_t)(v->key_held|v->key_latched):0;v->key_latched=0;return 1;}
        if(IS("getGraphics","()Ljavax/microedition/lcdui/Graphics;")){
            o=obj(v,a[0]);if(!o)return -1;if(!o->data[0]){int32_t image_ref=builtin_object(v,"javax/microedition/lcdui/Image");o=obj(v,a[0]);if(!o||!image_ref)return -1;o->data[0]=image_ref;PJImage*image=image_alloc_mutable(240,320);if(!image){gc(v);image=image_alloc_mutable(240,320);}if(!image){o->data[0]=0;err(v,"GameCanvas buffer RAM limit");return -1;}obj(v,o->data[0])->native=image;}
            int32_t image_ref=o->data[0];*out=builtin_object(v,"javax/microedition/lcdui/Graphics");if(!*out)return -1;obj(v,*out)->data[0]=image_ref;v->tx=v->ty=v->cx=v->cy=0;v->cw=240;v->ch=320;v->color=0;return 1;
        }
        if(IS("flushGraphics","()V")||IS("flushGraphics","(IIII)V")){
            o=obj(v,a[0]);if(!o)return -1;if(a[0]!=v->canvas||!o->data[0])return 1;Object*holder=obj(v,o->data[0]);if(!holder||!holder->native)return -1;
            int x=desc[1]=='I'?a[1]:0,y=desc[1]=='I'?a[2]:0,w=desc[1]=='I'?a[3]:240,h=desc[1]=='I'?a[4]:320;int64_t right=(int64_t)x+w,bottom=(int64_t)y+h;if(w<=0||h<=0)return 1;if(x<0)x=0;if(y<0)y=0;if(right>240)right=240;if(bottom>320)bottom=320;if(right<=x||bottom<=y)return 1;
            int tx=v->tx,ty=v->ty,cx=v->cx,cy=v->cy,cw=v->cw,ch=v->ch;v->tx=v->ty=0;v->cx=x;v->cy=y;v->cw=(int)right-x;v->ch=(int)bottom-y;gfx_image(v,v->graphics,holder->native,x,y,v->cw,v->ch,0,x,y,20);v->tx=tx;v->ty=ty;v->cx=cx;v->cy=cy;v->cw=cw;v->ch=ch;v->rendered=1;v->yield=1;return 1;
        }
    }
    if(b==B_GRAPHICS){
        if(IS("setGrayScale","(I)V")){if(a[1]<0||a[1]>255)return throw_exception(v,"java/lang/IllegalArgumentException","Invalid gray scale");v->color=(uint32_t)a[1]*0x010101u;return 1;}
        if(IS("getGrayScale","()I")){*out=(int32_t)((((v->color>>16)&255)*30+((v->color>>8)&255)*59+(v->color&255)*11)/100);return 1;}
        if(IS("drawChar","(CIII)V")){char text[4];size_t n=0;utf8_put(text,&n,(uint16_t)a[1]);text[n]=0;int64_t x=(int64_t)a[2]+v->tx,y=(int64_t)a[3]+v->ty;if(a[4]&8)x-=8;else if(a[4]&1)x-=4;if(a[4]&32)y-=16;else if(a[4]&64)y-=12;if(x>=0&&x<240&&y>=0&&y<320)v->p.text((int)x,(int)y,text,v->color);return 1;}
    }
    if(b==B_RUNTIME){
        if(IS("getRuntime","()Ljava/lang/Runtime;")){if(!v->runtime)v->runtime=builtin_object(v,"java/lang/Runtime");*out=v->runtime;return *out?1:-1;}
        if(IS("gc","()V")){gc(v);return 1;}
        if(IS("totalMemory","()J")||IS("freeMemory","()J")){size_t used=pj_memory();*out=(int32_t)(name[0]=='t'?PJ_MEMORY_LIMIT:used<PJ_MEMORY_LIMIT?PJ_MEMORY_LIMIT-used:0);v->native_high=0;return 1;}
        if(IS("exit","(I)V")){v->stopped=1;platform_tone_stop();return 1;}
    }
    if(b==B_THREAD){
        if(IS("currentThread","()Ljava/lang/Thread;")){Context*ctx=v->contexts[v->current];if(!ctx->thread){ctx->thread=builtin_object(v,"java/lang/Thread");if(!ctx->thread)return -1;obj(v,ctx->thread)->data[1]=5;}*out=ctx->thread;return 1;}
        if(IS("<init>","()V")||IS("<init>","(Ljava/lang/Runnable;)V")){o=obj(v,a[0]);if(!o)return -1;o->data[0]=desc[1]=='L'?a[1]:0;o->data[1]=5;Context*ctx=v->contexts[v->current];if(ctx->thread){Object*parent=obj(v,ctx->thread);if(parent&&parent->data[1])o->data[1]=parent->data[1];}return 1;}
        if(IS("setPriority","(I)V")){if(a[1]<1||a[1]>10)return throw_exception(v,"java/lang/IllegalArgumentException","Invalid thread priority");o=obj(v,a[0]);if(!o)return -1;o->data[1]=a[1];return 1;}
        if(IS("getPriority","()I")){o=obj(v,a[0]);if(!o)return -1;*out=o->data[1]?o->data[1]:5;return 1;}
    }
    if(b==B_SYSTEM&&IS("getProperty","(Ljava/lang/String;)Ljava/lang/String;")){
        if(!a[0])return throw_exception(v,"java/lang/NullPointerException",NULL);const char*key=str(v,a[0]);if(!*key)return throw_exception(v,"java/lang/IllegalArgumentException","Empty property key");const char*value=NULL;
        if(!strcmp(key,"microedition.platform"))value="PocketJava MRE Preview";
        else if(!strcmp(key,"microedition.encoding"))value="UTF-8";
        else if(!strcmp(key,"microedition.locale"))value="en-US";
        else if(!strcmp(key,"microedition.configuration"))value="CLDC-1.1";
        else if(!strcmp(key,"microedition.profiles"))value="MIDP-2.0";
        *out=value?string(v,(const uint8_t*)value,strlen(value)):0;return value&&!*out?-1:1;
    }
    if(b==B_STRING&&IS("getBytes","(Ljava/lang/String;)[B")){
        if(!a[1])return throw_exception(v,"java/lang/NullPointerException",NULL);const char*enc=str(v,a[1]);int utf8=charset_eq(enc,"UTF8"),latin=charset_eq(enc,"ISO88591"),ascii=charset_eq(enc,"USASCII"),be=charset_eq(enc,"UTF16BE"),le=charset_eq(enc,"UTF16LE"),bom=charset_eq(enc,"UTF16");
        if(!utf8&&!latin&&!ascii&&!be&&!le&&!bom)return throw_exception(v,"java/io/UnsupportedEncodingException",enc);
        const char*source=str(v,a[0]),*p=source;size_t n=bom?2:0;
        while(*p){uint32_t c=utf_unit(&p);n+=utf8?(c<128?1:c<2048?2:3):(be||le||bom)?2:1;}
        *out=allocobj(v,0,18,(uint32_t)n);if(!*out)return -1;uint8_t*bytes=(uint8_t*)obj(v,*out)->data;size_t pos=0;if(bom){bytes[pos++]=254;bytes[pos++]=255;}
        p=source;while(*p){uint32_t c=utf_unit(&p);if(utf8)utf8_put((char*)bytes,&pos,c);else if(be||le||bom){bytes[pos++]=(uint8_t)(le?c:c>>8);bytes[pos++]=(uint8_t)(le?c>>8:c);}else bytes[pos++]=(uint8_t)(c<=(latin?255u:127u)?c:'?');}return 1;
    }
    if(b==B_DATAOUTPUT&&IS("writeUTF","(Ljava/lang/String;)V")){
        if(!a[1])return throw_exception(v,"java/lang/NullPointerException",NULL);const char*source=str(v,a[1]),*p=source;size_t length=0;
        while(*p){uint32_t c=utf_unit(&p);length+=c&&c<128?1:c<2048?2:3;if(length>65535)return throw_exception(v,"java/io/UTFDataFormatException","UTF string too long");}
        unsigned char chunk[64];size_t used=2;chunk[0]=(unsigned char)(length>>8);chunk[1]=(unsigned char)length;p=source;
        while(*p){uint32_t c=utf_unit(&p);if(used>sizeof(chunk)-3){if(stream_write(v,a[0],chunk,used)!=1)return -1;used=0;}if(c&&c<128)chunk[used++]=(unsigned char)c;else if(c<2048){chunk[used++]=(unsigned char)(192|(c>>6));chunk[used++]=(unsigned char)(128|(c&63));}else{chunk[used++]=(unsigned char)(224|(c>>12));chunk[used++]=(unsigned char)(128|((c>>6)&63));chunk[used++]=(unsigned char)(128|(c&63));}}
        return stream_write(v,a[0],chunk,used);
    }
