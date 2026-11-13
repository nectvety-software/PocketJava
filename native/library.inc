/* Native CLDC/MIDP library shared by ARM and desktop builds. */
static int32_t builtin_object(PJ*v,const char*name){int c=loadclass(v,name);return c?allocobj(v,c,0,v->classes[c]->instance_fields):0;}
static int replace_bytes(PJ*v,Object*o,const void*data,size_t n,int kind){size_t need=n?n:1,oldn=(o->len?o->len:1)*element_size(o->kind);uint8_t*old=(uint8_t*)o->data;ptrdiff_t off=-1;if(data&&old&&(const uint8_t*)data>=old&&(const uint8_t*)data<old+oldn)off=(const uint8_t*)data-old;void*p=pj_realloc(o->data,need);if(!p){gc(v);p=pj_realloc(o->data,need);}if(!p)return err(v,"Memory limit in native object");if(n){if(off>=0)memmove(p,(uint8_t*)p+off,n);else memcpy(p,data,n);}if(kind==2&&o->native){pj_free(o->native);o->native=NULL;}o->data=p;o->len=(uint32_t)n;o->kind=kind;return 1;}
static int charset_eq(const char*a,const char*b){for(;;){while(*a&&(*a=='-'||*a=='_'||*a==' '))a++;while(*b&&(*b=='-'||*b=='_'||*b==' '))b++;unsigned ca=(unsigned char)*a,cb=(unsigned char)*b;if(ca>='A'&&ca<='Z')ca+=32;if(cb>='A'&&cb<='Z')cb+=32;if(ca!=cb)return 0;if(!ca)return 1;a++;b++;}}
static void utf8_put(char*out,size_t*pos,uint32_t c){if(c<128)out[(*pos)++]=(char)c;else if(c<2048){out[(*pos)++]=(char)(192|(c>>6));out[(*pos)++]=(char)(128|(c&63));}else{out[(*pos)++]=(char)(224|(c>>12));out[(*pos)++]=(char)(128|((c>>6)&63));out[(*pos)++]=(char)(128|(c&63));}}
static uint32_t upper_unit(uint32_t c){if(c>='a'&&c<='z')return c-32;if(c>=0x00e0&&c<=0x00f6&&c!=0x00f7)return c-32;if(c>=0x00f8&&c<=0x00fe)return c-32;if(c==0x0103)return 0x0102;if(c==0x0111)return 0x0110;if(c==0x01a1)return 0x01a0;if(c==0x01b0)return 0x01af;if(c>=0x1ea1&&c<=0x1ef9&&(c&1))return c-1;return c;}
static uint32_t lower_unit(uint32_t c){if(c>='A'&&c<='Z')return c+32;if(c>=0x00c0&&c<=0x00d6&&c!=0x00d7)return c+32;if(c>=0x00d8&&c<=0x00de)return c+32;if(c==0x0102)return 0x0103;if(c==0x0110)return 0x0111;if(c==0x01a0)return 0x01a1;if(c==0x01af)return 0x01b0;if(c>=0x1ea0&&c<=0x1ef8&&!(c&1))return c+1;return c;}
static int java_equal(PJ*v,int32_t a,int32_t b){if(a==b)return 1;if(!a||!b)return 0;Object*x=obj(v,a),*y=obj(v,b);if(!x||!y)return 0;if(x->kind==2&&y->kind==2)return !strcmp((char*)x->data,(char*)y->data);int type=v->classes[x->cls]->builtin;if((type==B_INTEGER||type==B_SHORT)&&type==v->classes[y->cls]->builtin)return x->data[0]==y->data[0];return 0;}
static int vector_reserve(PJ*v,Object*o,size_t n){size_t bytes=native_buffer_size(o),current=bytes/4;if(n<=current)return 1;int cls=o->cls,vector=0,guard=0;while(cls&&guard++<CLASSES){if(v->classes[cls]->builtin==B_VECTOR){vector=1;break;}cls=v->classes[cls]->parent;}size_t cap;if(vector){if(o->data[1]>0){size_t increment=(size_t)o->data[1];cap=current+((n-current+increment-1)/increment)*increment;}else{cap=current?current*2:1;if(cap<n)cap=n;}}else{cap=current?current*2:16;while(cap<n)cap*=2;}if(cap>PJ_ENTRY_LIMIT/4)return err(v,"Collection capacity limit");void*base=native_buffer_alloc(cap*4);if(!base)return err(v,"Collection RAM exhausted");void*p=(uint8_t*)base+sizeof(uint32_t);memset(p,0,cap*4);if(bytes)memcpy(p,native_buffer_data(o),bytes);pj_free(o->native);o->native=base;return 1;}

static int stream_read(PJ*v,int32_t ref,unsigned char*out,size_t n){if(!ref)return throw_exception(v,"java/lang/NullPointerException",NULL);Object*o=obj(v,ref);if(!o)return -1;int type=v->classes[o->cls]->builtin;if(type==B_DATAINPUT)return stream_read(v,o->data[0],out,n);if(type==B_BYTEINPUT){Object*arr=obj(v,o->data[0]);if(!arr)return -1;size_t pos=(size_t)o->data[1],remain=(size_t)o->data[2]-pos;if(n>remain)n=remain;memcpy(out,(uint8_t*)arr->data+pos,n);o->data[1]+=(int32_t)n;return (int)n;}if(type==B_INPUT){if(o->native){PJZipStream*s=(PJZipStream*)o->native;int got=jar_stream_read(s,out,n,v->error);if(got>=0&&!jar_stream_available(s)){jar_stream_close(s);o->native=NULL;o->data[0]=1;}return got;}if(o->data[0]==1)return 0;return throw_exception(v,"java/io/IOException","Stream is closed");}return throw_exception(v,"java/io/IOException","Stream is closed");}
static int stream_write(PJ*v,int32_t ref,const unsigned char*bytes,size_t n){Object*o=obj(v,ref);if(!o)return -1;int type=v->classes[o->cls]->builtin;if(type==B_DATAOUTPUT)return stream_write(v,o->data[0],bytes,n);if(type!=B_BYTEOUTPUT)return throw_exception(v,"java/io/IOException","Stream not writable");size_t used=(size_t)o->data[0];if(n>PJ_ENTRY_LIMIT-used)return throw_exception(v,"java/io/IOException","Output stream limit");size_t need=used+n,capacity=native_buffer_size(o);if(!o->native&&o->data[1]){Object*snapshot=obj(v,o->data[1]);if(!snapshot)return -1;gc(v);o->native=native_buffer_alloc(need);if(!o->native)return throw_exception(v,"java/io/IOException","Output snapshot RAM limit");memcpy(native_buffer_data(o),snapshot->data,used);capacity=need;o->data[1]=0;}if(need>capacity){size_t cap=need>8192?((need+4095)&~4095u):capacity?capacity*2:128;while(cap<need)cap*=2;if(cap>PJ_ENTRY_LIMIT)cap=PJ_ENTRY_LIMIT;void*p=native_buffer_realloc(o->native,cap);if(!p){gc(v);p=native_buffer_realloc(o->native,cap);}if(!p){err(v,"Output stream RAM exhausted");return -1;}o->native=p;}memcpy((uint8_t*)native_buffer_data(o)+used,bytes,n);o->data[0]=(int32_t)need;return 1;}
static int integer_digit(unsigned char c){if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='z')return c-'a'+10;if(c>='A'&&c<='Z')return c-'A'+10;return -1;}
static int parse_integer(PJ*v,int32_t ref,int radix,int32_t*out){
    if(radix<2||radix>36)return throw_exception(v,"java/lang/NumberFormatException","Invalid radix");
    if(!ref)return throw_exception(v,"java/lang/NumberFormatException","null");
    const unsigned char*s=(const unsigned char*)str(v,ref);int negative=0;
    if(*s=='-'||*s=='+'){negative=*s=='-';s++;}
    if(!*s)return throw_exception(v,"java/lang/NumberFormatException","Invalid integer string");
    int32_t limit=negative?INT32_MIN:-INT32_MAX,multmin=limit/radix,result=0;
    for(;*s;s++){int digit=integer_digit(*s);if(digit<0||digit>=radix||result<multmin)return throw_exception(v,"java/lang/NumberFormatException","Invalid integer string");result*=radix;if(result<limit+digit)return throw_exception(v,"java/lang/NumberFormatException","Integer overflow");result-=digit;}
    *out=negative?result:-result;return 1;
}
static int library_native(PJ*v,int b,const char*name,const char*desc,int32_t*a,int32_t*out){
#define IS(n,d) (!strcmp(name,n)&&!strcmp(desc,d))
    Object*o;
#include "compat.inc"
    if(b==B_OBJECT&&IS("equals","(Ljava/lang/Object;)Z")){*out=a[0]==a[1];return 1;}
    if(b==B_OBJECT&&IS("hashCode","()I")){*out=a[0];return 1;}
    if(b==B_RECORDSTORE){
        if(IS("openRecordStore","(Ljava/lang/String;Z)Ljavax/microedition/rms/RecordStore;")){uint32_t hash=2166136261u;const char*name=str(v,a[0]);for(const char*p=v->title;*p;p++)hash=(hash^(unsigned char)*p)*16777619u;for(const char*p=v->main;*p;p++)hash=(hash^(unsigned char)*p)*16777619u;for(;*name;name++)hash=(hash^(unsigned char)*name)*16777619u;*out=builtin_object(v,"javax/microedition/rms/RecordStore");o=*out?obj(v,*out):NULL;if(!o)return -1;o->data[0]=(int32_t)hash;size_t meta_size;unsigned char*meta=platform_record_read(hash,0,&meta_size);if(meta){if(meta_size!=4||meta[0]>32){pj_free(meta);return throw_exception(v,"javax/microedition/rms/RecordStoreException","Invalid store metadata");}o->data[1]=meta[0];pj_free(meta);}else{if(!a[1])return throw_exception(v,"javax/microedition/rms/RecordStoreNotFoundException",NULL);unsigned char empty[4]={0};if(!platform_record_write(hash,0,empty,4))return throw_exception(v,"javax/microedition/rms/RecordStoreException","Cannot create store");}return 1;}
        o=obj(v,a[0]);if(!o)return -1;if(IS("closeRecordStore","()V")){o->data[2]=1;return 1;}if(o->data[2])return throw_exception(v,"javax/microedition/rms/RecordStoreNotOpenException",NULL);
        if(IS("getNumRecords","()I")){*out=o->data[1];return 1;}
        if(IS("getRecord","(I)[B")){size_t n;unsigned char*d=platform_record_read((uint32_t)o->data[0],a[1],&n);if(!d)return throw_exception(v,"javax/microedition/rms/InvalidRecordIDException",NULL);*out=allocobj(v,0,18,(uint32_t)n);if(*out)memcpy(obj(v,*out)->data,d,n);pj_free(d);return *out?1:-1;}
        if(IS("addRecord","([BII)I")||IS("setRecord","(I[BII)V")){int add=name[0]=='a',id=add?o->data[1]+1:a[1],base=add?1:2;Object*data=obj(v,a[base]);if(!data)return -1;int off=a[base+1],len=a[base+2];if(id<1||id>32||(!add&&id>o->data[1])||off<0||len<0||len>65536||(uint32_t)off>data->len||(uint32_t)len>data->len-(uint32_t)off)return throw_exception(v,"javax/microedition/rms/RecordStoreException","Invalid record");if(!platform_record_write((uint32_t)o->data[0],id,(uint8_t*)data->data+off,(size_t)len))return throw_exception(v,"javax/microedition/rms/RecordStoreException","Save failed");if(add){unsigned char meta[4]={(unsigned char)id,0,0,0};if(!platform_record_write((uint32_t)o->data[0],0,meta,4))return throw_exception(v,"javax/microedition/rms/RecordStoreException","Cannot commit store");o->data[1]=id;*out=id;}return 1;}
    if(IS("getNextRecordID","()I")){*out=o->data[1]+1;if(*out<1)*out=1;if(*out>32)*out=1;return 1;}
    if(IS("getLastModified","()J")){v->native_high=0;*out=0;return 1;}
    if(IS("getRecordSize","(I)I")){size_t n;unsigned char*d=platform_record_read((uint32_t)o->data[0],a[1],&n);if(!d)return throw_exception(v,"javax/microedition/rms/InvalidRecordIDException",NULL);pj_free(d);*out=(int32_t)n;return 1;}
    if(IS("deleteRecord","(I)V")){if(a[1]<1||a[1]>(int)o->data[1])return throw_exception(v,"javax/microedition/rms/InvalidRecordIDException",NULL);platform_record_remove((uint32_t)o->data[0],a[1]);return 1;}
    if(IS("getSizeAvailable","()I")){*out=100000;return 1;}
    if(IS("getVersion","()I")){*out=1;return 1;}
    if(IS("getSize","()I")){*out=(int)o->data[1]*64;return 1;}
    if(IS("enumerateRecords","(Ljavax/microedition/rms/RecordFilter;Ljavax/microedition/rms/RecordComparator;Z)Ljavax/microedition/rms/RecordEnumeration;")){*out=builtin_object(v,"javax/microedition/rms/RecordEnumeration");if(*out){Object*e=obj(v,*out);if(e){e->data[0]=a[0];e->data[1]=0;}}return *out?1:-1;}
    }
    if(b==B_RECORDENUMERATION){
        o=obj(v,a[0]);if(!o)return -1;
        Object*rso=o->data[0]?obj(v,o->data[0]):NULL;
        if(IS("hasMoreElements","()Z")||IS("hasNextElement","()Z")){*out=(rso&&(int)rso->data[1]>(int)o->data[1])?1:0;return 1;}
        if(IS("hasPreviousElement","()Z")){*out=((int)o->data[1]>0)?1:0;return 1;}
        if(IS("previousRecordId","()I")){if((int)o->data[1]<1)return throw_exception(v,"java/util/NoSuchElementException",NULL);*out=o->data[1];o->data[1]--;return 1;}
        if(IS("previousRecord","()[B")){if((int)o->data[1]<1)return throw_exception(v,"java/util/NoSuchElementException",NULL);int id=o->data[1];o->data[1]--;size_t n;unsigned char*d=platform_record_read((uint32_t)rso->data[0],id,&n);if(!d)return throw_exception(v,"javax/microedition/rms/InvalidRecordIDException",NULL);*out=allocobj(v,0,18,(uint32_t)n);if(*out)memcpy(obj(v,*out)->data,d,n);pj_free(d);return *out?1:-1;}
        if(IS("nextElement","()[B")){if(!rso||(int)o->data[1]>=(int)rso->data[1])return throw_exception(v,"java/util/NoSuchElementException",NULL);o->data[1]++;int id=o->data[1];size_t n;unsigned char*d=platform_record_read((uint32_t)rso->data[0],id,&n);if(!d)return throw_exception(v,"javax/microedition/rms/InvalidRecordIDException",NULL);*out=allocobj(v,0,18,(uint32_t)n);if(*out)memcpy(obj(v,*out)->data,d,n);pj_free(d);return *out?1:-1;}
        if(IS("numRecords","()I")){*out=rso?rso->data[1]:0;return 1;}
        if(IS("nextRecordId","()I")){if(!rso||(int)o->data[1]>=(int)rso->data[1])return throw_exception(v,"java/util/NoSuchElementException",NULL);o->data[1]++;*out=o->data[1];return 1;}
        if(IS("nextRecord","()[B")){if(!rso||(int)o->data[1]>=(int)rso->data[1])return throw_exception(v,"java/util/NoSuchElementException",NULL);o->data[1]++;int id=o->data[1];size_t n;unsigned char*d=platform_record_read((uint32_t)rso->data[0],id,&n);if(!d)return throw_exception(v,"javax/microedition/rms/InvalidRecordIDException",NULL);*out=allocobj(v,0,18,(uint32_t)n);if(*out)memcpy(obj(v,*out)->data,d,n);pj_free(d);return *out?1:-1;}
        if(IS("reset","()V")){o->data[1]=0;return 1;}
        if(IS("destroy","()V")){o->data[0]=0;return 1;}
        if(IS("rebuild","()V")){return 1;}
    }
    if(b==B_DATE){
        o=obj(v,a[0]);if(!o)return -1;
        if(IS("<init>","()V")){o->data[0]=0;o->data[1]=0;return 1;}
        if(IS("<init>","(J)V")){o->data[0]=a[2];o->data[1]=a[1];return 1;}
        if(IS("getTime","()J")){v->native_high=o->data[1];*out=o->data[0];return 1;}
        if(IS("setTime","(J)V")){o->data[0]=a[2];o->data[1]=a[1];return 1;}
        if(IS("toString","()Ljava/lang/String;")){char buf[40];uint64_t t=(uint64_t)(uint32_t)o->data[0]|(uint64_t)(uint32_t)o->data[1]<<32;snprintf(buf,sizeof(buf),"%llu",(unsigned long long)t);*out=string(v,(const unsigned char*)buf,strlen(buf));return *out?1:-1;}
    }
    if(b==B_CALENDAR){
        if(IS("getInstance","()Ljava/util/Calendar;")){*out=builtin_object(v,"java/util/Calendar");if(*out){Object*co=obj(v,*out);if(co){co->data[0]=0;co->data[1]=0;}}return *out?1:-1;}
        o=obj(v,a[0]);if(!o)return -1;
        if(IS("getTime","()Ljava/util/Date;")){*out=builtin_object(v,"java/util/Date");if(*out){Object*do2=obj(v,*out);if(do2){do2->data[0]=o->data[0];do2->data[1]=o->data[1];}}return *out?1:-1;}
        if(IS("getTimeInMillis","()J")){v->native_high=o->data[1];*out=o->data[0];return 1;}
        if(IS("setTime","(Ljava/util/Date;)V")){Object*d=o?obj(v,a[1]):NULL;if(d){o->data[0]=d->data[0];o->data[1]=d->data[1];}return 1;}
        if(IS("setTimeInMillis","(J)V")){o->data[0]=a[2];o->data[1]=a[1];return 1;}
        if(IS("get","(I)I")){*out=0;return 1;}
        if(IS("add","(II)V")){return 1;}
        if(IS("roll","(II)V")){return 1;}
        if(IS("clear","()V")){o->data[0]=0;o->data[1]=0;return 1;}
        if(IS("clear","(I)V")){return 1;}
    }
    if(b==B_TIMER){
        o=obj(v,a[0]); /* may be null for static? Timer is instance; methods instance. getInstance not static. */
        if(IS("<init>","()V")||IS("<init>","(Z)V")){return 1;}
        if(IS("schedule","(Ljava/util/TimerTask;J)V")){
            if(!a[0]||!a[1])return throw_exception(v,"java/lang/NullPointerException",NULL);
            if(v->timer_n>=8)return throw_exception(v,"java/lang/IllegalStateException","Timer queue full");
            v->timers[v->timer_n].timer=a[0];v->timers[v->timer_n].task=a[1];
            v->timers[v->timer_n].due=v->p.clock_ms()+(uint32_t)(a[2]>0x7fffffff?0x7fffffff:a[2]);
            v->timers[v->timer_n].period=0;v->timers[v->timer_n].periodic=0;v->timers[v->timer_n].cancelled=0;v->timer_n++;
            return 1;
        }
        if(IS("schedule","(Ljava/util/TimerTask;JJ)V")){
            if(!a[0]||!a[1])return throw_exception(v,"java/lang/NullPointerException",NULL);
            if(v->timer_n>=8)return throw_exception(v,"java/lang/IllegalStateException","Timer queue full");
            uint32_t delay=a[2]>0x7fffffff?0x7fffffff:(uint32_t)a[2];
            uint32_t period=a[3]>0x7fffffff?0x7fffffff:(uint32_t)a[3];
            v->timers[v->timer_n].timer=a[0];v->timers[v->timer_n].task=a[1];
            v->timers[v->timer_n].due=v->p.clock_ms()+delay;
            v->timers[v->timer_n].period=period;v->timers[v->timer_n].periodic=1;v->timers[v->timer_n].cancelled=0;v->timer_n++;
            return 1;
        }
        if(IS("scheduleAtFixedRate","(Ljava/util/TimerTask;JJ)V")){ /* delegate to periodic */ 
            if(!a[0]||!a[1])return throw_exception(v,"java/lang/NullPointerException",NULL);
            if(v->timer_n>=8)return throw_exception(v,"java/lang/IllegalStateException","Timer queue full");
            uint32_t delay=a[2]>0x7fffffff?0x7fffffff:(uint32_t)a[2];
            uint32_t period=a[3]>0x7fffffff?0x7fffffff:(uint32_t)a[3];
            v->timers[v->timer_n].timer=a[0];v->timers[v->timer_n].task=a[1];
            v->timers[v->timer_n].due=v->p.clock_ms()+delay;
            v->timers[v->timer_n].period=period;v->timers[v->timer_n].periodic=1;v->timers[v->timer_n].cancelled=0;v->timer_n++;
            return 1;
        }
        if(IS("cancel","()V")){for(int i=0;i<v->timer_n;i++)if(v->timers[i].timer==a[0])v->timers[i].cancelled=1;return 1;}
        if(IS("purge","()I")){*out=0;return 1;}
    }
    if(b==B_TIMERTASK){
        if(IS("<init>","()V")){return 1;}
        if(IS("cancel","()Z")){int found=0;for(int i=0;i<v->timer_n;i++)if(v->timers[i].task==a[0]&&!v->timers[i].cancelled){v->timers[i].cancelled=1;found=1;}*out=found;return 1;}
        if(IS("scheduledExecutionTime","()J")){v->native_high=0;*out=0;return 1;}
    }
    if(b==B_INPUTSTREAMREADER){
        o=obj(v,a[0]);if(!o)return -1;
        if(IS("<init>","(Ljava/io/InputStream;)V")||IS("<init>","(Ljava/io/InputStream;Ljava/lang/String;)V")){o->data[0]=a[1];return 1;}
        if(IS("read","()I")){
            int32_t stream=o->data[0];if(!stream){*out=-1;return 1;}
            Object*so=obj(v,stream);if(!so){*out=-1;return 1;}
            int sb=v->classes[so->cls]->builtin;int32_t args[2]={stream,0},r=0;
            if(native(v,sb,"read","()I",args,&r))*out=r;else *out=-1;
            return 1;
        }
        if(IS("read","([CII)I")){
            Object*arr=obj(v,a[1]);if(!arr)return -1;int off=a[2],len=a[3];if(off<0||len<0||(uint32_t)off>arr->len||(uint32_t)len>arr->len-(uint32_t)off){return throw_exception(v,"java/lang/IndexOutOfBoundsException",NULL);}
            int read=0;
            for(int i=0;i<len;i++){
                int32_t stream=o->data[0];if(!stream){if(read==0)*out=-1;else *out=read;return 1;}
                Object*so=obj(v,stream);if(!so){if(read==0)*out=-1;else *out=read;return 1;}
                int sb=v->classes[so->cls]->builtin;int32_t args[2]={stream,0},r=0;
                if(!native(v,sb,"read","()I",args,&r)){if(read==0)*out=-1;else *out=read;return 1;}
                if(r<0){if(read==0)*out=-1;else *out=read;return 1;}
                array_put(arr,(uint32_t)(off+i),(int32_t)(uint16_t)r);read++;
            }
            *out=read;return 1;
        }
        if(IS("close","()V")){int32_t stream=o->data[0];if(stream){Object*so=obj(v,stream);if(so){int sb=v->classes[so->cls]->builtin;int32_t args[2]={stream,0};native(v,sb,"close","()V",args,&a[0]);}o->data[0]=0;}return 1;}
    }
    if(b==B_BUFFEREDREADER){
        o=obj(v,a[0]);if(!o)return -1;
        if(IS("<init>","(Ljava/io/Reader;)V")||IS("<init>","(Ljava/io/Reader;I)V")){o->data[0]=a[1];return 1;}
        if(IS("read","()I")){
            int32_t r=o->data[0];if(!r){*out=-1;return 1;}
            Object*ro=obj(v,r);if(!ro){*out=-1;return 1;}
            int rb=v->classes[ro->cls]->builtin;int32_t args[2]={r,0},res=0;
            if(native(v,rb,"read","()I",args,&res))*out=res;else *out=-1;
            return 1;
        }
        if(IS("read","([CII)I")){
            Object*arr=obj(v,a[1]);if(!arr)return -1;int off=a[2],len=a[3];if(off<0||len<0||(uint32_t)off>arr->len||(uint32_t)len>arr->len-(uint32_t)off){return throw_exception(v,"java/lang/IndexOutOfBoundsException",NULL);}
            int read=0;
            for(int i=0;i<len;i++){
                int32_t r=o->data[0];if(!r){if(read==0)*out=-1;else *out=read;return 1;}
                Object*ro=obj(v,r);if(!ro){if(read==0)*out=-1;else *out=read;return 1;}
                int rb=v->classes[ro->cls]->builtin;int32_t args[2]={r,0},res=0;
                if(!native(v,rb,"read","()I",args,&res)){if(read==0)*out=-1;else *out=read;return 1;}
                if(res<0){if(read==0)*out=-1;else *out=read;return 1;}
                array_put(arr,(uint32_t)(off+i),(int32_t)(uint16_t)res);read++;
            }
            *out=read;return 1;
        }
        if(IS("readLine","()Ljava/lang/String;")){
            size_t cap=256,pos=0;char*buf=pj_alloc(cap);if(!buf){err(v,"Reader RAM limit");return -1;}
            for(;;){
                int32_t r=o->data[0];if(!r)break;
                Object*ro=obj(v,r);if(!ro)break;
                int rb=v->classes[ro->cls]->builtin;int32_t args[2]={r,0},res=0;
                if(!native(v,rb,"read","()I",args,&res))break;
                if(res<0)break;
                if(res=='\n')break;
                if(res=='\r')continue;
                if(pos+4>=cap){cap*=2;char*nbuf=pj_realloc(buf,cap);if(!nbuf){pj_free(buf);err(v,"Reader RAM limit");return -1;}buf=nbuf;}
                utf8_put(buf,&pos,(uint32_t)res);
            }
            buf[pos]=0;*out=string(v,(const unsigned char*)buf,pos);pj_free(buf);return *out?1:-1;
        }
        if(IS("close","()V")){int32_t r=o->data[0];if(r){Object*ro=obj(v,r);if(ro){int rb=v->classes[ro->cls]->builtin;int32_t args[2]={r,0};native(v,rb,"close","()V",args,&a[0]);}o->data[0]=0;}return 1;}
    }
    if(b==B_BYTE){
        o=obj(v,a[0]);
        if(IS("valueOf","(B)Ljava/lang/Byte;")){int32_t ref=builtin_object(v,"java/lang/Byte");if(ref){obj(v,ref)->data[0]=a[0];}*out=ref;return 1;}
        if(IS("valueOf","(Ljava/lang/String;)Ljava/lang/Byte;")){int32_t number;int parsed=parse_integer(v,a[0],10,&number);if(parsed!=1)return parsed;int32_t ref=builtin_object(v,"java/lang/Byte");if(ref){obj(v,ref)->data[0]=number;}*out=ref;return 1;}
        if(IS("parseByte","(Ljava/lang/String;)B")||IS("parseByte","(Ljava/lang/String;I)B")){int32_t number;int radix=IS("parseByte","(Ljava/lang/String;I)B")?a[1]:10;int parsed=parse_integer(v,a[0],radix,&number);if(parsed!=1)return parsed;*out=number;return 1;}
        if(o){
            if(IS("<init>","(B)V")||IS("<init>","(I)V")){o->data[0]=a[1];return 1;}
            if(IS("<init>","(Ljava/lang/String;)V")){int32_t number;parse_integer(v,a[1],10,&number);o->data[0]=number;return 1;}
            if(IS("byteValue","()B")){*out=o->data[0];return 1;}
            if(IS("intValue","()I")){*out=o->data[0];return 1;}
            if(IS("toString","()Ljava/lang/String;")){char buf[16];snprintf(buf,sizeof(buf),"%ld",(long)(int8_t)o->data[0]);*out=string(v,(const unsigned char*)buf,strlen(buf));return *out?1:-1;}
        }
    }
    if(b==B_CHARACTER){
        if(IS("valueOf","(C)Ljava/lang/Character;")){int32_t ref=builtin_object(v,"java/lang/Character");if(ref){obj(v,ref)->data[0]=a[0];}*out=ref;return 1;}
        if(IS("toLowerCase","(C)C")){*out=lower_unit((uint32_t)(uint16_t)a[0]);return 1;}
        if(IS("toUpperCase","(C)C")){*out=upper_unit((uint32_t)(uint16_t)a[0]);return 1;}
        if(IS("isDigit","(C)Z")){*out=((uint16_t)a[0]>='0'&&(uint16_t)a[0]<='9')?1:0;return 1;}
        if(IS("isLetter","(C)Z")){uint16_t c=(uint16_t)a[0];*out=((c>='a'&&c<='z')||(c>='A'&&c<='Z'))?1:0;return 1;}
        if(IS("isLetterOrDigit","(C)Z")){uint16_t c=(uint16_t)a[0];*out=(((c>='a'&&c<='z')||(c>='A'&&c<='Z'))||(c>='0'&&c<='9'))?1:0;return 1;}
        if(IS("isWhitespace","(C)Z")){uint16_t c=(uint16_t)a[0];*out=(c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f')?1:0;return 1;}
        if(IS("isLowerCase","(C)Z")){uint16_t c=(uint16_t)a[0];*out=(c>='a'&&c<='z')?1:0;return 1;}
        if(IS("isUpperCase","(C)Z")){uint16_t c=(uint16_t)a[0];*out=(c>='A'&&c<='Z')?1:0;return 1;}
        if(IS("digit","(CI)I")){*out=-1;if(a[1]>=2&&a[1]<=36){uint16_t c=(uint16_t)a[0];if(c>='0'&&c<='9')*out=c-'0';else if(c>='a'&&c<='z')*out=c-'a'+10;else if(c>='A'&&c<='Z')*out=c-'A'+10;if(*out>=a[1])*out=-1;}return 1;}
        if(IS("forDigit","(II)C")){*out=0;if(a[0]>=0&&a[0]<36&&a[1]>=2&&a[1]<=36){*out=(a[0]<10)?('0'+a[0]):('a'+a[0]-10);}return 1;}
        o=obj(v,a[0]);
        if(o){
            if(IS("<init>","(C)V")){o->data[0]=a[1];return 1;}
            if(IS("charValue","()C")){*out=(uint16_t)o->data[0];return 1;}
            if(IS("toString","()Ljava/lang/String;")){char buf[8];size_t pos=0;utf8_put(buf,&pos,(uint32_t)(uint16_t)o->data[0]);buf[pos]=0;*out=string(v,(const unsigned char*)buf,pos);return *out?1:-1;}
        }
    }
    if(b==B_DATEFIELD){
        if(IS("<init>","(Ljava/lang/String;I)V")||IS("<init>","(Ljava/lang/String;ILjava/util/Date;)V")){return 1;}
        if(IS("getDate","()Ljava/util/Date;")){*out=builtin_object(v,"java/util/Date");return *out?1:-1;}
        if(IS("setDate","(Ljava/util/Date;)V")){return 1;}
        if(IS("isInputModeEditable","()Z")){*out=0;return 1;}
        if(IS("getInputMode","()I")){*out=0;return 1;}
        if(IS("setInputMode","(I)V")){return 1;}
    }
    if(b==B_COMMAND){
        o=obj(v,a[0]);
        if(IS("<init>","(Ljava/lang/String;II)V")){if(o){o->data[0]=a[1];o->data[1]=a[2];o->data[2]=a[3];}return 1;}
        if(o){
            if(IS("getLabel","()Ljava/lang/String;")){*out=o->data[0];return 1;}
            if(IS("getCommandType","()I")){*out=o->data[1];return 1;}
            if(IS("getPriority","()I")){*out=o->data[2];return 1;}
            if(IS("toString","()Ljava/lang/String;")){*out=o->data[0];return 1;}
        }
    }
    if(b==B_DIRECTUTILS&&IS("getDirectGraphics","(Ljavax/microedition/lcdui/Graphics;)Lcom/nokia/mid/ui/DirectGraphics;")){if(!a[0]||!obj(v,a[0]))return throw_exception(v,"java/lang/NullPointerException",NULL);*out=builtin_object(v,"com/nokia/mid/ui/DirectGraphics");if(!*out)return -1;o=obj(v,*out);if(o)o->data[0]=a[0];return o?1:-1;}
    if(b==B_DIRECTGRAPHICS&&IS("drawPixels","([SZIIIIIIII)V")){o=obj(v,a[0]);Object*arr=obj(v,a[1]);if(!o||!arr)return -1;if(arr->kind!=19)return throw_exception(v,"java/lang/IllegalArgumentException","drawPixels requires short[]");int transparent=a[2],off=a[3],scan=a[4],x=a[5],y=a[6],w=a[7],h=a[8],manip=a[9],format=a[10];if(w<=0||h<=0)return 1;if(manip!=0)return throw_exception(v,"java/lang/IllegalArgumentException","DirectGraphics manipulation unsupported");if(scan==0||scan==INT32_MIN||(scan<0?-scan:scan)<w)return throw_exception(v,"java/lang/IllegalArgumentException","Invalid DirectGraphics scanlength");int64_t first=off,last=(int64_t)off+(int64_t)(h-1)*scan,lo=first<last?first:last,hi=(first>last?first:last)+w-1;if(lo<0||hi<0||(uint64_t)hi>=arr->len)return throw_exception(v,"java/lang/ArrayIndexOutOfBoundsException",NULL);int dx=x+v->tx,dy=y+v->ty;for(int yy=0;yy<h;yy++){int run=0;uint32_t lastc=0;int64_t base=(int64_t)off+(int64_t)yy*scan;for(int xx=0;xx<=w;xx++){int opaque=0;uint32_t c=0;if(xx<w){uint16_t p=(uint16_t)array_get(arr,(uint32_t)(base+xx));if(format==4444){unsigned an=(p>>12)&15,r=(p>>8)&15,g=(p>>4)&15,b8=p&15;opaque=!transparent||an>=8;c=(r*17u)<<16|(g*17u)<<8|b8*17u;}else if(format==565){opaque=1;c=expand565(p);}else if(format==555){unsigned r=(p>>10)&31,g=(p>>5)&31,b8=p&31;opaque=1;c=(r*255u/31)<<16|(g*255u/31)<<8|b8*255u/31;}else if(format==444){unsigned r=(p>>8)&15,g=(p>>4)&15,b8=p&15;opaque=1;c=(r*17u)<<16|(g*17u)<<8|b8*17u;}else if(format==1555){unsigned r=(p>>10)&31,g=(p>>5)&31,b8=p&31;opaque=!transparent||(p&0x8000);c=(r*255u/31)<<16|(g*255u/31)<<8|b8*255u/31;}else return throw_exception(v,"java/lang/IllegalArgumentException","DirectGraphics pixel format unsupported");}if(run&&(!opaque||c!=lastc)){gfx_rect(v,o->data[0],dx+xx-run,dy+yy,run,1,lastc);run=0;}if(opaque){lastc=c;run++;}}}return 1;}
    if(b==B_DIRECTGRAPHICS&&IS("fillPolygon","([II[IIII)V")){o=obj(v,a[0]);Object*xa=obj(v,a[1]),*ya=obj(v,a[3]);if(!o||!xa||!ya)return -1;int xo=a[2],yo=a[4],n=a[5];if(xa->kind!=20||ya->kind!=20||xo<0||yo<0||n<3||(uint32_t)xo>xa->len||(uint32_t)yo>ya->len||(uint32_t)n>xa->len-(uint32_t)xo||(uint32_t)n>ya->len-(uint32_t)yo)return throw_exception(v,"java/lang/IllegalArgumentException","Invalid polygon arrays");int miny=array_get(ya,(uint32_t)yo),maxy=miny;for(int i=1;i<n;i++){int py=array_get(ya,(uint32_t)(yo+i));if(py<miny)miny=py;if(py>maxy)maxy=py;}int*cross=pj_alloc((size_t)n*sizeof(int));if(!cross){err(v,"Polygon RAM limit");return -1;}uint32_t color=(uint32_t)a[6]&0xffffffu;for(int y=miny;y<=maxy;y++){int count=0;for(int i=0,j=n-1;i<n;j=i++){int xi=array_get(xa,(uint32_t)(xo+i)),yi=array_get(ya,(uint32_t)(yo+i)),xj=array_get(xa,(uint32_t)(xo+j)),yj=array_get(ya,(uint32_t)(yo+j));if((yi<=y&&yj>y)||(yj<=y&&yi>y)){int64_t num=(int64_t)(y-yi)*(xj-xi);cross[count++]=xi+(int)(num/(yj-yi));}}for(int i=1;i<count;i++){int value=cross[i],j=i-1;while(j>=0&&cross[j]>value){cross[j+1]=cross[j];j--;}cross[j+1]=value;}for(int i=0;i+1<count;i+=2){int x1=cross[i],x2=cross[i+1];if(x2<x1){int t=x1;x1=x2;x2=t;}gfx_rect(v,o->data[0],x1+v->tx,y+v->ty,x2-x1+1,1,color);}}pj_free(cross);return 1;}
    if(b==B_MANAGER){
        if(IS("playTone","(III)V")){if(a[0]<0||a[0]>127||a[1]<=0||a[1]>0x0fffffff)return throw_exception(v,"java/lang/IllegalArgumentException","Invalid tone note/duration");int volume=a[2];if(volume<0)volume=0;if(volume>100)volume=100;if(!platform_tone(a[0],a[1],volume))return throw_exception(v,"javax/microedition/media/MediaException","MRE tone playback failed");return 1;}
        if(IS("createPlayer","(Ljava/io/InputStream;Ljava/lang/String;)Ljavax/microedition/media/Player;")){if(!a[0])return throw_exception(v,"java/io/IOException","Null media stream");if(!a[1])return throw_exception(v,"java/lang/IllegalArgumentException","Null media type");*out=builtin_object(v,"javax/microedition/media/Player");if(!*out)return -1;o=obj(v,*out);if(!o)return -1;o->data[0]=100;o->data[1]=a[0];o->data[2]=1;return 1;}
    }
    if(b==B_PLAYER||b==B_CONTROL){
        o=obj(v,a[0]);if(!o)return -1;
        if(IS("realize","()V")){if(o->data[0]&&o->data[0]<200)o->data[0]=200;return 1;}
        if(IS("prefetch","()V")){if(o->data[0]&&o->data[0]<300)o->data[0]=300;return 1;}
        if(IS("start","()V")){if(o->data[0])o->data[0]=400;return 1;}
        if(IS("stop","()V")){if(o->data[0]>=400)o->data[0]=300;return 1;}
        if(IS("deallocate","()V")){if(o->data[0]>200)o->data[0]=200;return 1;}
        if(IS("close","()V")){o->data[0]=0;o->data[1]=0;o->data[3]=0;return 1;}
        if(IS("getState","()I")){*out=o->data[0];return 1;}
        if(IS("setLoopCount","(I)V")){if(!a[1])return throw_exception(v,"java/lang/IllegalArgumentException","Loop count cannot be zero");o->data[2]=a[1];return 1;}
        if(IS("getControl","(Ljava/lang/String;)Ljavax/microedition/media/Control;")){const char*control=str(v,a[1]);if(strstr(control,"VolumeControl")){if(!o->data[3]){o->data[3]=builtin_object(v,"javax/microedition/media/control/VolumeControl");if(!o->data[3])return -1;Object*volume=obj(v,o->data[3]);if(volume)volume->data[0]=100;}*out=o->data[3];}else *out=0;return 1;}
    }
    if(b==B_VOLUME){
        o=obj(v,a[0]);if(!o)return -1;
        if(IS("setLevel","(I)I")){int level=a[1];if(level<0)level=0;if(level>100)level=100;o->data[0]=level;*out=level;return 1;}
        if(IS("getLevel","()I")){*out=o->data[0];return 1;}
        if(IS("setMute","(Z)V")){o->data[1]=a[1]!=0;return 1;}
        if(IS("isMuted","()Z")){*out=o->data[1]!=0;return 1;}
    }
    if(b==B_IMAGE){
        if(IS("createImage","(Ljava/lang/String;)Ljavax/microedition/lcdui/Image;")){if(!a[0])return throw_exception(v,"java/lang/NullPointerException",NULL);const char*path=str(v,a[0]);if(*path=='/')path++;char resource_error[160]={0};size_t n;gc(v);unsigned char*d=jar_extract(&v->jar,path,&n,resource_error);if(!d)return throw_exception(v,"java/io/IOException",resource_error[0]?resource_error:"Image resource not found");char message[160]={0};PJImage*img=png_decode(d,n,message);pj_free(d);if(!img)return throw_exception(v,"java/io/IOException",message[0]?message:"Invalid image resource");*out=builtin_object(v,"javax/microedition/lcdui/Image");if(!*out){pj_free(img);return -1;}obj(v,*out)->native=img;return 1;}
        if(IS("createImage","([BII)Ljavax/microedition/lcdui/Image;")){Object*arr=obj(v,a[0]);if(!arr)return -1;if(arr->kind!=18||a[1]<0||a[2]<0||(uint32_t)a[1]>arr->len||(uint32_t)a[2]>arr->len-(uint32_t)a[1])return throw_exception(v,"java/lang/IllegalArgumentException",NULL);gc(v);char message[160]={0};PJImage*img=png_decode((uint8_t*)arr->data+a[1],(size_t)a[2],message);if(!img)return throw_exception(v,"java/lang/IllegalArgumentException",message);*out=builtin_object(v,"javax/microedition/lcdui/Image");if(!*out){pj_free(img);return -1;}obj(v,*out)->native=img;return 1;}
        if(IS("createImage","(II)Ljavax/microedition/lcdui/Image;")){gc(v);PJImage*img=image_alloc_mutable(a[0],a[1]);if(!img)return throw_exception(v,"java/lang/IllegalArgumentException","Image size/RAM limit");*out=builtin_object(v,"javax/microedition/lcdui/Image");if(!*out){pj_free(img);return -1;}obj(v,*out)->native=img;return 1;}
        if(IS("createRGBImage","([IIIZ)Ljavax/microedition/lcdui/Image;")){Object*arr=obj(v,a[0]);if(!arr)return -1;int w=a[1],h=a[2],alpha=a[3];if(arr->kind!=20||w<=0||h<=0||(size_t)w*h>arr->len)return throw_exception(v,"java/lang/IllegalArgumentException",NULL);gc(v);PJImage*img=image_alloc(w,h,alpha!=0);if(!img)return throw_exception(v,"java/lang/IllegalArgumentException","Image size/RAM limit");img->mutable=0;size_t count=(size_t)w*h;for(size_t n=0;n<count;n++){uint32_t c=(uint32_t)array_get(arr,(uint32_t)n);unsigned r=(c>>16)&255,g=(c>>8)&255,b8=c&255;img->pixels[n]=(uint16_t)((r>>3)<<11|(g>>2)<<5|(b8>>3));if(img->alpha){if(alpha&&((c>>24)&255)<128)img->alpha[n/8]&=(uint8_t)~(1u<<(n%8));else img->alpha[n/8]|=(uint8_t)(1u<<(n%8));}}*out=builtin_object(v,"javax/microedition/lcdui/Image");if(!*out){pj_free(img);return -1;}obj(v,*out)->native=img;return 1;}
        if(IS("getWidth","()I")||IS("getHeight","()I")){o=obj(v,a[0]);if(!o||!o->native)return -1;PJImage*img=o->native;*out=name[3]=='W'?img->w:img->h;return 1;}
        if(IS("getGraphics","()Ljavax/microedition/lcdui/Graphics;")){o=obj(v,a[0]);if(!o||!o->native)return -1;PJImage*img=o->native;if(!img->mutable)return throw_exception(v,"java/lang/IllegalStateException","Immutable image");*out=builtin_object(v,"javax/microedition/lcdui/Graphics");if(*out)obj(v,*out)->data[0]=a[0];v->tx=v->ty=v->cx=v->cy=0;v->cw=img->w;v->ch=img->h;return 1;}
    }
    if(b==B_GRAPHICS&&(IS("drawImage","(Ljavax/microedition/lcdui/Image;III)V")||IS("drawRegion","(Ljavax/microedition/lcdui/Image;IIIIIIII)V"))){o=obj(v,a[1]);if(!o||!o->native)return -1;PJImage*img=o->native;if(name[4]=='I')gfx_image(v,a[0],img,0,0,img->w,img->h,0,a[2],a[3],a[4]);else gfx_image(v,a[0],img,a[2],a[3],a[4],a[5],a[6],a[7],a[8],a[9]);return 1;}
    if(b==B_CLASS&&IS("getResourceAsStream","(Ljava/lang/String;)Ljava/io/InputStream;")){const char*path=str(v,a[1]);if(*path=='/')path++;int32_t saved=v->temporary[7];*out=builtin_object(v,"java/io/InputStream");if(!*out)return -1;v->temporary[7]=*out;PJZipStream*stream=jar_stream_open(v,&v->jar,path,v->error);v->temporary[7]=saved;if(!stream){*out=0;return v->error[0]?-1:1;}Object*input=obj(v,*out);if(!input){jar_stream_close(stream);*out=0;return -1;}input->native=stream;return 1;}
    if(b==B_CLASS&&IS("forName","(Ljava/lang/String;)Ljava/lang/Class;")){if(!a[0])return throw_exception(v,"java/lang/NullPointerException",NULL);int cls=loadclass(v,str(v,a[0]));if(!cls)return throw_exception(v,"java/lang/ClassNotFoundException",str(v,a[0]));*out=builtin_object(v,"java/lang/Class");if(*out)obj(v,*out)->data[0]=cls;return *out?1:-1;}
    if(b==B_INPUT||b==B_BYTEINPUT||b==B_DATAINPUT){
        if(b==B_DATAINPUT&&IS("skipBytes","(I)I")){
            /* Bounded scratch space: never allocate an array as large as n. */
            unsigned char scratch[64];int32_t left=a[1],skipped=0;
            while(left>0){size_t n=left>64?64:(size_t)left;int got=stream_read(v,a[0],scratch,n);if(got<0)return -1;if(!got)break;left-=got;skipped+=got;}
            *out=skipped;return 1;
        }
        if(IS("<init>","([B)V")){Object*arr=obj(v,a[1]);o=obj(v,a[0]);if(!arr||!o)return -1;o->data[0]=a[1];o->data[1]=0;o->data[2]=(int32_t)arr->len;return 1;}
        if(IS("<init>","(Ljava/io/InputStream;)V")){o=obj(v,a[0]);if(o)o->data[0]=a[1];return 1;}
        if(IS("close","()V")){o=obj(v,a[0]);if(!o)return -1;int guard=0;while(o&&guard++<16){int type=v->classes[o->cls]->builtin;if(type==B_DATAINPUT){int32_t next=o->data[0];o->data[0]=0;o=next?obj(v,next):NULL;}else{if(type==B_INPUT){jar_stream_close((PJZipStream*)o->native);o->native=NULL;o->data[0]=0;}break;}}return 1;}
        if(IS("available","()I")){o=obj(v,a[0]);if(!o)return -1;int guard=0;while(o&&guard++<16){int type=v->classes[o->cls]->builtin;if(type==B_DATAINPUT){o=o->data[0]?obj(v,o->data[0]):NULL;continue;}if(type==B_BYTEINPUT){if(!o->data[0]){*out=0;return 1;}Object*arr=obj(v,o->data[0]);if(!arr)return -1;int32_t remain=o->data[2]-o->data[1];*out=remain>0?remain:0;return 1;}if(type==B_INPUT){*out=o->native?(int32_t)jar_stream_available((PJZipStream*)o->native):0;return 1;}break;}*out=0;return 1;}
        if(IS("read","()I")||IS("readByte","()B")||IS("readUnsignedByte","()I")||IS("readBoolean","()Z")||IS("readShort","()S")||IS("readUnsignedShort","()I")||IS("readInt","()I")){
            unsigned char data[4];int size=!strcmp(name,"readInt")?4:(!strcmp(name,"readShort")||!strcmp(name,"readUnsignedShort")?2:1);int got=stream_read(v,a[0],data,size);if(got<0)return -1;if(got!=size){if(!strcmp(name,"read")){*out=-1;return 1;}return throw_exception(v,"java/io/EOFException",NULL);}uint32_t value=0;for(int i=0;i<size;i++)value=value<<8|data[i];*out=!strcmp(name,"readByte")?(int8_t)value:!strcmp(name,"readShort")?(int16_t)value:!strcmp(name,"readBoolean")?value!=0:(int32_t)value;return 1;
        }
        if(IS("read","([B)I")||IS("read","([BII)I")||IS("readFully","([B)V")||IS("readFully","([BII)V")){
            Object*arr=obj(v,a[1]);if(!arr)return -1;int full=name[4]=='F',offset=strstr(desc,"II")?a[2]:0,length=strstr(desc,"II")?a[3]:(int)arr->len;if(arr->kind!=18||offset<0||length<0||(uint32_t)offset>arr->len||(uint32_t)length>arr->len-(uint32_t)offset)return throw_exception(v,"java/lang/IndexOutOfBoundsException",NULL);
            /* Firmware streams may return fewer bytes than requested, so readFully loops. */
            int total=0,got;while(total<length){got=stream_read(v,a[0],(uint8_t*)arr->data+offset+total,(size_t)(length-total));if(got<0)return -1;if(!got)break;total+=got;}
            if(full&&total!=length)return throw_exception(v,"java/io/EOFException",NULL);
            *out=!total&&length?-1:total;return 1;
        }
        if(b==B_DATAINPUT&&IS("readChar","()C")){unsigned char bytes[2];int got=stream_read(v,a[0],bytes,2);if(got<0)return -1;if(got!=2)return throw_exception(v,"java/io/EOFException",NULL);*out=(int32_t)((uint32_t)bytes[0]<<8|bytes[1]);return 1;}
        if(b==B_DATAINPUT&&IS("readFloat","()F")){unsigned char bytes[4];int got=stream_read(v,a[0],bytes,4);if(got<0)return -1;if(got!=4)return throw_exception(v,"java/io/EOFException",NULL);uint32_t bits=0;for(int i=0;i<4;i++)bits=bits<<8|bytes[i];*out=(int32_t)bits;return 1;}
        if(b==B_DATAINPUT&&IS("readDouble","()D")){unsigned char bytes[8];int got=stream_read(v,a[0],bytes,8);if(got<0)return -1;if(got!=8)return throw_exception(v,"java/io/EOFException",NULL);uint64_t bits=0;for(int i=0;i<8;i++)bits=bits<<8|bytes[i];v->native_high=(int32_t)(bits>>32);*out=(int32_t)bits;return 1;}
        if(b==B_DATAINPUT&&IS("readLine","()Ljava/lang/String;")){
            /* Deprecated CLDC method: keep it bounded and never allocate per byte. */
            size_t cap=128,used=0;char*line=pj_alloc(cap);int any=0;
            if(!line){err(v,"Line buffer RAM limit");return -1;}
            for(;;){unsigned char ch;int got=stream_read(v,a[0],&ch,1);if(got<0){pj_free(line);return -1;}if(!got)break;any=1;if(ch=='\n')break;
                if(used+1>=cap){size_t next=cap*2;if(next>PJ_ENTRY_LIMIT){pj_free(line);return throw_exception(v,"java/io/IOException","Line too long");}char*grown=pj_realloc(line,next);if(!grown){gc(v);grown=pj_realloc(line,next);}if(!grown){pj_free(line);err(v,"Line buffer RAM limit");return -1;}line=grown;cap=next;}
                line[used++]=(char)ch;}
            if(!any){pj_free(line);*out=0;return 1;}
            if(used&&line[used-1]=='\r')used--;
            *out=string(v,(const unsigned char*)line,used);pj_free(line);return *out?1:-1;
        }
        if(b==B_DATAINPUT&&IS("readUTF","(Ljava/io/DataInput;)Ljava/lang/String;")){
            unsigned char len[2];if(stream_read(v,a[0],len,2)!=2)return throw_exception(v,"java/io/EOFException",NULL);size_t n=(size_t)len[0]<<8|len[1];unsigned char*buf=pj_alloc(n+1);if(!buf){err(v,"UTF string RAM limit");return -1;}int got=stream_read(v,a[0],buf,n);if(got==(int)n)*out=string(v,buf,n);pj_free(buf);if(got!=(int)n)return throw_exception(v,"java/io/EOFException",NULL);return 1;
        }
        if(IS("skip","(J)J")){uint64_t n=(uint64_t)(uint32_t)a[1]<<32|(uint32_t)a[2];if((int64_t)n<0)n=0;uint64_t skipped=0;unsigned char buffer[1024];while(skipped<n){size_t want=n-skipped>1024?1024:(size_t)(n-skipped);int got=stream_read(v,a[0],buffer,want);if(got<0)return -1;if(!got)break;skipped+=(unsigned)got;}*out=(int32_t)skipped;v->native_high=(int32_t)(skipped>>32);return 1;}
        if(IS("readUTF","()Ljava/lang/String;")){unsigned char len[2];if(stream_read(v,a[0],len,2)!=2)return throw_exception(v,"java/io/EOFException",NULL);size_t n=(size_t)len[0]<<8|len[1];unsigned char*buf=pj_alloc(n+1);if(!buf){err(v,"UTF string RAM limit");return -1;}int got=stream_read(v,a[0],buf,n);if(got==(int)n)*out=string(v,buf,n);pj_free(buf);if(got!=(int)n)return throw_exception(v,"java/io/EOFException",NULL);return 1;}
    }
    if(b==B_OUTPUT||b==B_BYTEOUTPUT||b==B_DATAOUTPUT){
        if(IS("<init>","()V")||IS("<init>","(I)V"))return 1;
        if(IS("<init>","(Ljava/io/OutputStream;)V")){o=obj(v,a[0]);if(o)o->data[0]=a[1];return 1;}
        if(IS("close","()V")||IS("flush","()V"))return 1;
    if(IS("reset","()V")){o=obj(v,a[0]);if(o)o->data[0]=0;return 1;}
        if(IS("toByteArray","()[B")){o=obj(v,a[0]);if(!o)return -1;*out=allocobj(v,0,18,(uint32_t)o->data[0]);if(*out&&o->data[0])memcpy(obj(v,*out)->data,native_buffer_data(o),(size_t)o->data[0]);return *out?1:-1;}
        if(IS("write","([B)V")||IS("write","([BII)V")){Object*arr=obj(v,a[1]);if(!arr)return -1;int off=strstr(desc,"II")?a[2]:0,n=strstr(desc,"II")?a[3]:(int)arr->len;if(off<0||n<0||(uint32_t)off>arr->len||(uint32_t)n>arr->len-(uint32_t)off)return throw_exception(v,"java/lang/IndexOutOfBoundsException",NULL);return stream_write(v,a[0],(uint8_t*)arr->data+off,n);}
        if(IS("write","(I)V")||IS("writeByte","(I)V")||IS("writeBoolean","(Z)V")||IS("writeShort","(I)V")||IS("writeInt","(I)V")){unsigned char data[4];int n=!strcmp(name,"writeInt")?4:!strcmp(name,"writeShort")?2:1;uint32_t value=(uint32_t)a[1];for(int i=n-1;i>=0;i--){data[i]=(unsigned char)value;value>>=8;}return stream_write(v,a[0],data,n);}
    }
    if(b==B_STRING||b==B_STRINGBUFFER){
        if(!strcmp(name,"<init>")){
            o=obj(v,a[0]);if(!o)return -1;const char*s="";char*temp=NULL;
            if(!strcmp(desc,"(Ljava/lang/String;)V"))s=str(v,a[1]);
            else if(b==B_STRING&&(!strcmp(desc,"([BIILjava/lang/String;)V")||!strcmp(desc,"([BLjava/lang/String;)V"))){Object*arr=obj(v,a[1]);if(!arr)return -1;int shortform=!strcmp(desc,"([BLjava/lang/String;)V");int offset=shortform?0:a[2],length=shortform?(int)arr->len:a[3];int32_t encRef=shortform?a[2]:a[4];if(!encRef)return throw_exception(v,"java/lang/NullPointerException",NULL);if(arr->kind!=18||offset<0||length<0||(uint32_t)offset>arr->len||(uint32_t)length>arr->len-(uint32_t)offset)return throw_exception(v,"java/lang/IndexOutOfBoundsException",NULL);const uint8_t*bytes=(const uint8_t*)arr->data+offset;const char*enc=str(v,encRef);size_t cap=(size_t)length*3+1;temp=pj_alloc(cap);if(!temp){err(v,"String RAM limit");return -1;}size_t pos=0;if(charset_eq(enc,"UTF8")){memcpy(temp,bytes,(size_t)length);pos=(size_t)length;}else if(charset_eq(enc,"ISO88591")){for(int i=0;i<length;i++)utf8_put(temp,&pos,bytes[i]);}else if(charset_eq(enc,"USASCII")){for(int i=0;i<length;i++)utf8_put(temp,&pos,bytes[i]<128?bytes[i]:'?');}else if(charset_eq(enc,"UTF16")||charset_eq(enc,"UTF16BE")||charset_eq(enc,"UTF16LE")){int little=charset_eq(enc,"UTF16LE"),start=0;if(charset_eq(enc,"UTF16")&&length>=2){if(bytes[0]==0xff&&bytes[1]==0xfe){little=1;start=2;}else if(bytes[0]==0xfe&&bytes[1]==0xff)start=2;}for(int i=start;i+1<length;i+=2){uint32_t c=little?((uint32_t)bytes[i]|(uint32_t)bytes[i+1]<<8):((uint32_t)bytes[i]<<8|bytes[i+1]);utf8_put(temp,&pos,c);}}else{pj_free(temp);return throw_exception(v,"java/io/UnsupportedEncodingException",enc);}temp[pos]=0;s=temp;
            }else if(!strcmp(desc,"([B)V")||!strcmp(desc,"([C)V")||!strcmp(desc,"([BII)V")||!strcmp(desc,"([CII)V")){
                Object*arr=obj(v,a[1]);if(!arr)return -1;int offset=strlen(desc)>5?a[2]:0,length=strlen(desc)>5?a[3]:(int)arr->len;
                if(offset<0||length<0||(uint32_t)offset>arr->len||(uint32_t)length>arr->len-(uint32_t)offset){err(v,"String constructor bounds");return -1;}
                temp=pj_alloc((size_t)length*3+1);if(!temp){err(v,"String RAM limit");return -1;}size_t pos=0;for(int i=0;i<length;i++){uint32_t c=(uint32_t)array_get(arr,offset+i)&(arr->kind==15?65535:255);if(c<128)temp[pos++]=(char)c;else if(c<2048){temp[pos++]=(char)(192|(c>>6));temp[pos++]=(char)(128|(c&63));}else{temp[pos++]=(char)(224|(c>>12));temp[pos++]=(char)(128|((c>>6)&63));temp[pos++]=(char)(128|(c&63));}}temp[pos]=0;s=temp;
            }else if(strcmp(desc,"()V")&&strcmp(desc,"(I)V"))return 0;
            int ok=replace_bytes(v,o,s,strlen(s)+1,2);pj_free(temp);return ok?1:-1;
        }
        if(b==B_STRINGBUFFER&&!strcmp(name,"append")){
            o=obj(v,a[0]);if(!o)return -1;if(o->kind!=2){char empty=0;if(!replace_bytes(v,o,&empty,1,2))return -1;}
            char number[40];const char*s;
            if(strstr(desc,"Ljava/lang/String;"))s=a[1]?str(v,a[1]):"null";
            else if(desc[1]=='Z')s=a[1]?"true":"false";
            else if(desc[1]=='C'){unsigned c=(uint16_t)a[1];int pos=0;if(c&&c<128)number[pos++]=(char)c;else if(c<2048){number[pos++]=(char)(192|(c>>6));number[pos++]=(char)(128|(c&63));}else{number[pos++]=(char)(224|(c>>12));number[pos++]=(char)(128|((c>>6)&63));number[pos++]=(char)(128|(c&63));}number[pos]=0;s=number;}
            else if(desc[1]=='J'){uint64_t bits=(uint64_t)(uint32_t)a[1]<<32|(uint32_t)a[2];int negative=(int64_t)bits<0;uint64_t value=negative?0ull-bits:bits;size_t pos=sizeof(number)-1;number[pos]=0;do{number[--pos]=(char)('0'+value%10);value/=10;}while(value);if(negative)number[--pos]='-';s=number+pos;}
            else if(desc[1]=='I'){snprintf(number,sizeof(number),"%ld",(long)a[1]);s=number;}
            else return 0;
            size_t old=strlen((char*)o->data),add=strlen(s);char*buf=pj_alloc(old+add+1);if(!buf){err(v,"StringBuffer RAM limit");return -1;}memcpy(buf,o->data,old);memcpy(buf+old,s,add+1);pj_free(o->data);o->data=(int32_t*)buf;o->len=(uint32_t)(old+add+1);*out=a[0];return 1;
        }
        if(b==B_STRINGBUFFER&&IS("toString","()Ljava/lang/String;")){o=obj(v,a[0]);if(!o)return -1;*out=string(v,(uint8_t*)o->data,strlen((char*)o->data));return 1;}
        if(b==B_STRINGBUFFER&&IS("length","()I")){o=obj(v,a[0]);if(o)*out=(int)utf_length((char*)o->data);return 1;}
        if(b==B_STRING){
            if(IS("valueOf","(I)Ljava/lang/String;")){char number[24];int n=snprintf(number,sizeof(number),"%ld",(long)a[0]);*out=string(v,(const uint8_t*)number,(size_t)n);return *out?1:-1;}
            if(IS("valueOf","(Ljava/lang/Object;)Ljava/lang/String;")){if(!a[0]){*out=string(v,(const uint8_t*)"null",4);return *out?1:-1;}Object*x=obj(v,a[0]);if(!x)return -1;int type=v->classes[x->cls]->builtin;if(type==B_STRING){*out=a[0];return 1;}if(type==B_STRINGBUFFER){*out=string(v,(const uint8_t*)x->data,strlen((char*)x->data));return *out?1:-1;}if(type==B_INTEGER||type==B_SHORT){char number[24];int n=snprintf(number,sizeof(number),"%ld",(long)x->data[0]);*out=string(v,(const uint8_t*)number,(size_t)n);return *out?1:-1;}return 0;}
            if(IS("equals","(Ljava/lang/Object;)Z")){*out=java_equal(v,a[0],a[1]);return 1;}
            if(IS("compareTo","(Ljava/lang/String;)I")){if(!a[1])return throw_exception(v,"java/lang/NullPointerException",NULL);const char*s=str(v,a[0]),*t=str(v,a[1]);while(*s&&*t){int cs=(int)utf_unit(&s),ct=(int)utf_unit(&t);if(cs!=ct){*out=cs-ct;return 1;}}*out=(int32_t)utf_length(s)-(int32_t)utf_length(t);return 1;}
            if(IS("startsWith","(Ljava/lang/String;)Z")||IS("endsWith","(Ljava/lang/String;)Z")){const char*s=str(v,a[0]),*t=str(v,a[1]);size_t ns=strlen(s),nt=strlen(t);*out=ns>=nt&&!memcmp(s+(name[0]=='e'?ns-nt:0),t,nt);return 1;}
            if(IS("substring","(I)Ljava/lang/String;")||IS("substring","(II)Ljava/lang/String;")){const char*s=str(v,a[0]);size_t n=utf_length(s);int end=desc[2]=='I'?a[2]:(int)n;if(a[1]<0||end<a[1]||(size_t)end>n){err(v,"Substring bounds");return -1;}const char*start=utf_at(s,(size_t)a[1]),*stop=utf_at(start,(size_t)(end-a[1]));*out=string(v,(const uint8_t*)start,(size_t)(stop-start));return 1;}
            if(IS("trim","()Ljava/lang/String;")){const char*s=str(v,a[0]);size_t n=strlen(s),start=0;while(start<n&&(uint8_t)s[start]<=32)start++;while(n>start&&(uint8_t)s[n-1]<=32)n--;*out=string(v,(uint8_t*)s+start,n-start);return 1;}
            if(IS("toUpperCase","()Ljava/lang/String;")||IS("toLowerCase","()Ljava/lang/String;")){const char*s=str(v,a[0]),*p=s;size_t cap=utf_length(s)*3+1,pos=0;char*buf=pj_alloc(cap);if(!buf){err(v,"String RAM limit");return -1;}int changed=0,upper=name[2]=='U';while(*p){uint32_t c=utf_unit(&p),mapped=upper?upper_unit(c):lower_unit(c);if(mapped!=c)changed=1;utf8_put(buf,&pos,mapped);}if(!changed){pj_free(buf);*out=a[0];return 1;}*out=string(v,(const uint8_t*)buf,pos);pj_free(buf);return *out?1:-1;}
            if(IS("concat","(Ljava/lang/String;)Ljava/lang/String;")){if(!a[1])return throw_exception(v,"java/lang/NullPointerException",NULL);const char*s=str(v,a[0]),*t=str(v,a[1]);size_t ns=strlen(s),nt=strlen(t);if(!nt){*out=a[0];return 1;}char*buf=pj_alloc(ns+nt);if(!buf){err(v,"String RAM limit");return -1;}memcpy(buf,s,ns);memcpy(buf+ns,t,nt);*out=string(v,(uint8_t*)buf,ns+nt);pj_free(buf);return *out?1:-1;}
            if(IS("replace","(CC)Ljava/lang/String;")){const char*s=str(v,a[0]);uint32_t from=(uint16_t)a[1],to=(uint16_t)a[2];if(from==to){*out=a[0];return 1;}const char*p=s;int found=0;while(*p){if(utf_unit(&p)==from){found=1;break;}}if(!found){*out=a[0];return 1;}size_t cap=utf_length(s)*3+1,pos=0;char*buf=pj_alloc(cap);if(!buf){err(v,"String RAM limit");return -1;}p=s;while(*p){uint32_t c=utf_unit(&p);utf8_put(buf,&pos,c==from?to:c);}*out=string(v,(const uint8_t*)buf,pos);pj_free(buf);return *out?1:-1;}
            if(IS("indexOf","(Ljava/lang/String;)I")||IS("indexOf","(Ljava/lang/String;I)I")){if(!a[1])return throw_exception(v,"java/lang/NullPointerException",NULL);const char*s=str(v,a[0]),*needle=str(v,a[1]);size_t n=utf_length(s);int start=IS("indexOf","(Ljava/lang/String;I)I")?a[2]:0;if(start<0)start=0;if(!*needle){*out=start>(int)n?(int)n:start;return 1;}if((size_t)start>=n){*out=-1;return 1;}const char*base=utf_at(s,(size_t)start),*found=strstr(base,needle);if(!found){*out=-1;return 1;}int index=start;const char*p=base;while(p<found&&*p){utf_unit(&p);index++;}*out=index;return 1;}
            if(IS("indexOf","(I)I")||IS("indexOf","(II)I")){const char*s=str(v,a[0]);int start=IS("indexOf","(II)I")?a[2]:0;if(start<0)start=0;size_t n=utf_length(s);if((size_t)start>=n||a[1]<0||a[1]>0xffff){*out=-1;return 1;}s=utf_at(s,(size_t)start);*out=-1;for(int idx=start;*s;idx++){int ch=(int)utf_unit(&s);if(ch==a[1]){*out=idx;break;}}return 1;}
            if(IS("lastIndexOf","(I)I")||IS("lastIndexOf","(II)I")){const char*s=str(v,a[0]);size_t n=utf_length(s);int from=IS("lastIndexOf","(II)I")?a[2]:(int)n-1;if(from>=(int)n)from=(int)n-1;*out=-1;if(from<0||a[1]<0||a[1]>0xffff)return 1;for(int idx=0;*s&&idx<=from;idx++){int ch=(int)utf_unit(&s);if(ch==a[1])*out=idx;}return 1;}
            if(IS("getChars","(II[CI)V")){const char*s=str(v,a[0]);int begin=a[1],end=a[2],dstoff=a[4];Object*arr=obj(v,a[3]);if(!arr)return -1;size_t n=utf_length(s);if(begin<0||end<begin||(size_t)end>n)return throw_exception(v,"java/lang/StringIndexOutOfBoundsException",NULL);int count=end-begin;if(arr->kind!=15||dstoff<0||(uint32_t)dstoff>arr->len||(uint32_t)count>arr->len-(uint32_t)dstoff)return throw_exception(v,"java/lang/ArrayIndexOutOfBoundsException",NULL);s=utf_at(s,(size_t)begin);for(int i=0;i<count;i++)array_put(arr,(uint32_t)(dstoff+i),(int32_t)utf_unit(&s));return 1;}
            if(IS("getBytes","()[B")){const char*s=str(v,a[0]);size_t n=strlen(s);*out=allocobj(v,0,18,(uint32_t)n);if(*out&&n)memcpy(obj(v,*out)->data,s,n);return *out?1:-1;}
            if(IS("toCharArray","()[C")){const char*s=str(v,a[0]);size_t n=utf_length(s);*out=allocobj(v,0,15,(uint32_t)n);if(*out){Object*arr=obj(v,*out);for(size_t i=0;i<n;i++)array_put(arr,(uint32_t)i,(int32_t)utf_unit(&s));}return 1;}
        }
    }
    if(b==B_INTEGER||b==B_SHORT){
        if(b==B_INTEGER&&(IS("toHexString","(I)Ljava/lang/String;")||IS("toOctalString","(I)Ljava/lang/String;")||IS("toBinaryString","(I)Ljava/lang/String;"))){
            char digits[32];size_t pos=sizeof(digits);uint32_t value=(uint32_t)a[0];unsigned shift=name[2]=='H'?4:name[2]=='O'?3:1,mask=(1u<<shift)-1;
            do{digits[--pos]="0123456789abcdef"[value&mask];value>>=shift;}while(value);
            *out=string(v,(const uint8_t*)digits+pos,sizeof(digits)-pos);return *out?1:-1;
        }
        if(IS("<init>","(I)V")||IS("<init>","(S)V")){o=obj(v,a[0]);if(o)o->data[0]=a[1];return 1;}
        if(IS("intValue","()I")||IS("shortValue","()S")||IS("byteValue","()B")){o=obj(v,a[0]);if(o)*out=IS("byteValue","()B")?(int32_t)(int8_t)o->data[0]:IS("shortValue","()S")?(int32_t)(int16_t)o->data[0]:o->data[0];return 1;}
        if(IS("parseInt","(Ljava/lang/String;)I")||IS("parseInt","(Ljava/lang/String;I)I")||IS("valueOf","(Ljava/lang/String;)Ljava/lang/Integer;")){int32_t number;int radix=IS("parseInt","(Ljava/lang/String;I)I")?a[1]:10;int parsed=parse_integer(v,a[0],radix,&number);if(parsed!=1)return parsed;*out=number;if(name[0]=='v'){*out=builtin_object(v,"java/lang/Integer");if(*out)obj(v,*out)->data[0]=number;}return 1;}
    }
    if(b==B_VECTOR||b==B_HASHTABLE){
        o=obj(v,a[0]);if(!o)return -1;
        if(IS("<init>","()V")||IS("<init>","(I)V")){o->data[0]=0;return vector_reserve(v,o,16)?1:-1;}
        int32_t*items=(int32_t*)native_buffer_data(o);int count=o->data[0];
        if(b==B_VECTOR){
            if(IS("size","()I")){*out=count;return 1;}
            if(IS("isEmpty","()Z")){*out=count==0;return 1;}
            if(IS("addElement","(Ljava/lang/Object;)V")||IS("insertElementAt","(Ljava/lang/Object;I)V")){int pos=name[0]=='i'?a[2]:count;if(pos<0||pos>count){err(v,"Vector insertion bounds");return -1;}if(!vector_reserve(v,o,count+1))return -1;items=(int32_t*)native_buffer_data(o);memmove(items+pos+1,items+pos,(size_t)(count-pos)*4);items[pos]=a[1];o->data[0]++;return 1;}
            if(IS("elementAt","(I)Ljava/lang/Object;")){if(a[1]<0||a[1]>=count){err(v,"Vector index bounds");return -1;}*out=items[a[1]];return 1;}
            if(IS("contains","(Ljava/lang/Object;)Z")||IS("indexOf","(Ljava/lang/Object;)I")||IS("removeElement","(Ljava/lang/Object;)Z")){int idx=-1;for(int i=0;i<count;i++)if(java_equal(v,items[i],a[1])){idx=i;break;}*out=name[0]=='i'?idx:idx>=0;if(name[0]=='r'&&idx>=0){memmove(items+idx,items+idx+1,(size_t)(count-idx-1)*4);items[count-1]=0;o->data[0]--;}return 1;}
            if(IS("removeElementAt","(I)V")){int idx=a[1];if(idx<0||idx>=count){err(v,"Vector removal bounds");return -1;}memmove(items+idx,items+idx+1,(size_t)(count-idx-1)*4);items[count-1]=0;o->data[0]--;return 1;}
            if(IS("removeAllElements","()V")){if(items)memset(items,0,native_buffer_size(o));o->data[0]=0;return 1;}
            if(IS("copyInto","([Ljava/lang/Object;)V")){Object*arr=obj(v,a[1]);if(!arr)return -1;if(arr->kind!=1||arr->len<(uint32_t)count){err(v,"Vector copy bounds");return -1;}for(int i=0;i<count;i++)array_put(arr,(uint32_t)i,items[i]);return 1;}
            if(IS("elements","()Ljava/util/Enumeration;")){*out=builtin_object(v,"java/util/Enumeration");if(*out){Object*en=obj(v,*out);en->data[0]=a[0];en->data[1]=0;en->data[2]=0;}return 1;}
        }else{
            if(IS("size","()I")){*out=count;return 1;}
            if(IS("isEmpty","()Z")){*out=count==0;return 1;}
            if(IS("clear","()V")){if(items)memset(items,0,native_buffer_size(o));o->data[0]=0;return 1;}
            if(IS("containsKey","(Ljava/lang/Object;)Z")){*out=0;for(int i=0;i<count;i++)if(java_equal(v,items[i*2],a[1])){*out=1;break;}return 1;}
            if(IS("get","(Ljava/lang/Object;)Ljava/lang/Object;")||IS("put","(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;")){for(int i=0;i<count;i++)if(java_equal(v,items[i*2],a[1])){*out=items[i*2+1];if(name[0]=='p')items[i*2+1]=a[2];return 1;}if(name[0]=='p'){if(!vector_reserve(v,o,(count+1)*2))return -1;items=(int32_t*)native_buffer_data(o);items[count*2]=a[1];items[count*2+1]=a[2];o->data[0]++;}return 1;}
            if(IS("remove","(Ljava/lang/Object;)Ljava/lang/Object;")){for(int i=0;i<count;i++)if(java_equal(v,items[i*2],a[1])){*out=items[i*2+1];memmove(items+i*2,items+(i+1)*2,(size_t)(count-i-1)*8);items[(count-1)*2]=0;items[(count-1)*2+1]=0;o->data[0]--;return 1;}return 1;}
            if(IS("keys","()Ljava/util/Enumeration;")||IS("elements","()Ljava/util/Enumeration;")){*out=builtin_object(v,"java/util/Enumeration");if(*out){Object*en=obj(v,*out);en->data[0]=a[0];en->data[1]=0;en->data[2]=name[0]=='k'?1:2;}return 1;}
        }
    }
    if(b==B_ENUM){o=obj(v,a[0]);if(!o)return -1;Object*src=obj(v,o->data[0]);if(!src)return -1;int idx=o->data[1],count=src->data[0],mode=o->data[2];if(IS("hasMoreElements","()Z")){*out=idx<count;return 1;}if(IS("nextElement","()Ljava/lang/Object;")){if(idx>=count){err(v,"Enumeration exhausted");return -1;}int32_t*items=(int32_t*)native_buffer_data(src);if(!items){err(v,"Enumeration source missing");return -1;}*out=mode==1?items[idx*2]:mode==2?items[idx*2+1]:items[idx];o->data[1]=idx+1;return 1;}}
    if(b==B_FONT){
        if(IS("getFont","(III)Ljavax/microedition/lcdui/Font;")||IS("getDefaultFont","()Ljavax/microedition/lcdui/Font;")){int requested=name[3]=='F';if(requested&&((a[0]!=0&&a[0]!=32&&a[0]!=64)||(a[1]&~7)||(a[2]!=0&&a[2]!=8&&a[2]!=16)))return throw_exception(v,"java/lang/IllegalArgumentException","Invalid font");*out=builtin_object(v,"javax/microedition/lcdui/Font");if(!*out)return -1;o=obj(v,*out);if(requested){o->data[0]=a[0];o->data[1]=a[1];o->data[2]=a[2];}return 1;}
        if(IS("getSize","()I")||IS("getFace","()I")||IS("getStyle","()I")){o=obj(v,a[0]);if(!o)return -1;*out=o->data[name[3]=='F'?0:name[4]=='t'?1:2];return 1;}
        if(IS("isPlain","()Z")||IS("isBold","()Z")||IS("isItalic","()Z")||IS("isUnderlined","()Z")){o=obj(v,a[0]);if(!o)return -1;*out=name[2]=='P'?o->data[1]==0:!!(o->data[1]&(name[2]=='B'?1:name[2]=='I'?2:4));return 1;}
        if(IS("getHeight","()I")){*out=16;return 1;}
        if(IS("getBaselinePosition","()I")){*out=12;return 1;}
        if(IS("charWidth","(C)I")){*out=8;return 1;}
        if(IS("stringWidth","(Ljava/lang/String;)I")){*out=(int32_t)strlen(str(v,a[1]))*8;return 1;}
        if(IS("substringWidth","(Ljava/lang/String;II)I")){*out=a[3]*8;return 1;}
        if(IS("charsWidth","([CII)I")){*out=a[3]*8;return 1;}
    }
    if(b==B_OBJECT&&IS("getClass","()Ljava/lang/Class;")){o=obj(v,a[0]);if(!o)return -1;int cls=o->cls;*out=builtin_object(v,"java/lang/Class");if(*out)obj(v,*out)->data[0]=cls;return 1;}
    if(b==B_SYSTEM){
        if(IS("currentTimeMillis","()J")){uint64_t t=v->p.clock_ms();v->native_high=(int32_t)(t>>32);*out=(int32_t)t;return 1;}
        if(IS("gc","()V")){gc(v);return 1;}
        if(IS("arraycopy","(Ljava/lang/Object;ILjava/lang/Object;II)V")){Object*src=obj(v,a[0]),*dst=obj(v,a[2]);if(!src||!dst)return -1;if(a[1]<0||a[3]<0||a[4]<0||(uint32_t)a[1]>src->len||(uint32_t)a[3]>dst->len||(uint32_t)a[4]>src->len-(uint32_t)a[1]||(uint32_t)a[4]>dst->len-(uint32_t)a[3]||src->kind!=dst->kind){err(v,"Invalid arraycopy");return -1;}size_t es=element_size(src->kind);memmove((uint8_t*)dst->data+(size_t)a[3]*es,(uint8_t*)src->data+(size_t)a[1]*es,(size_t)a[4]*es);return 1;}
    }
    if(b==B_THREAD){
        if(IS("<init>","(Ljava/lang/Runnable;)V")){o=obj(v,a[0]);if(o)o->data[0]=a[1];return 1;}
        if(IS("start","()V")){o=obj(v,a[0]);if(!o)return -1;for(int t=1;t<4;t++)if(!v->contexts[t]){Context*ctx=context_new(8);if(!ctx){err(v,"Thread context RAM limit");return -1;}ctx->runnable=o->data[0]?o->data[0]:a[0];ctx->thread=a[0];v->contexts[t]=ctx;return 1;}err(v,"Thread count limit");return -1;}
        if(IS("isAlive","()Z")){*out=0;for(int t=1;t<4;t++){Context*ctx=v->contexts[t];if(ctx&&ctx->thread==a[0]&&ctx->runnable&&(!ctx->started||(t==v->current?v->depth:ctx->depth)>0)){*out=1;break;}}return 1;}
        if(IS("sleep","(J)V")){int64_t delay=(int64_t)((uint64_t)(uint32_t)a[0]<<32|(uint32_t)a[1]);if(delay<0){err(v,"Negative thread sleep");return -1;}if(delay>0x7fffffff)delay=0x7fffffff;v->contexts[v->current]->wake=v->p.clock_ms()+(uint32_t)delay;v->yield=1;return 1;}
        if(IS("yield","()V")){v->yield=1;return 1;}
    }
    if(b==B_PRINTSTREAM){if((!strcmp(name,"print")||!strcmp(name,"println"))&&(!strcmp(desc,"()V")||!strcmp(desc,"(Ljava/lang/String;)V")||!strcmp(desc,"(Ljava/lang/Object;)V")||!strcmp(desc,"(I)V")||!strcmp(desc,"(J)V")||!strcmp(desc,"(Z)V")||!strcmp(desc,"(C)V")||!strcmp(desc,"([C)V")))return 1;if(IS("flush","()V")||IS("close","()V"))return 1;if(IS("checkError","()Z")){*out=0;return 1;}}
    if(b==B_EXCEPTION){if(IS("<init>","()V"))return 1;if(IS("<init>","(Ljava/lang/String;)V")){o=obj(v,a[0]);if(o)o->data[0]=a[1];return 1;}if(IS("printStackTrace","()V"))return 1;}
    if(b==B_MIDLET){if(IS("platformRequest","(Ljava/lang/String;)Z")){*out=0;return 1;}if(IS("getAppProperty","(Ljava/lang/String;)Ljava/lang/String;")){char value[256];size_t n;const char*key=str(v,a[1]);if(v->jad&&jar_property(v->jad,v->jad_size,key,value,sizeof(value))==1){*out=string(v,(unsigned char*)value,strlen(value));return 1;}gc(v);unsigned char*d=jar_extract(&v->jar,"META-INF/MANIFEST.MF",&n,v->error);if(d){int found=jar_property(d,n,str(v,a[1]),value,sizeof(value));pj_free(d);if(found==1)*out=string(v,(unsigned char*)value,strlen(value));}return 1;}}
    if((b==B_CANVAS||b==B_GAMECANVAS)&&IS("serviceRepaints","()V")){v->dirty=1;v->yield=1;return 1;}
    if(b==B_GRAPHICS){
        if(IS("drawRGB","([IIIIIIIZ)V")){Object*arr=obj(v,a[1]);if(!arr)return -1;int off=a[2],scan=a[3],x=a[4],y=a[5],w=a[6],h=a[7],alpha=a[8];if(arr->kind!=20)return throw_exception(v,"java/lang/IllegalArgumentException",NULL);if(w<=0||h<=0)return 1;if(scan==0||scan==INT32_MIN||(scan<0?-scan:scan)<w)return throw_exception(v,"java/lang/IllegalArgumentException",NULL);int64_t first=off,last=(int64_t)off+(int64_t)(h-1)*scan;int64_t lo=first<last?first:last,hi=first>last?first:last;hi+=w-1;if(lo<0||hi<0||(uint64_t)hi>=arr->len)return throw_exception(v,"java/lang/ArrayIndexOutOfBoundsException",NULL);int dx=x+v->tx,dy=y+v->ty;for(int yy=0;yy<h;yy++){int run=0;uint32_t lastc=0;int64_t base=(int64_t)off+(int64_t)yy*scan;for(int xx=0;xx<=w;xx++){int opaque=0;uint32_t c=0;if(xx<w){c=(uint32_t)array_get(arr,(uint32_t)(base+xx));opaque=!alpha||((c>>24)&255)>=128;c&=0xffffffu;}if(run&&(!opaque||c!=lastc)){gfx_rect(v,a[0],dx+xx-run,dy+yy,run,1,lastc);run=0;}if(opaque){lastc=c;run++;}}}return 1;}
        if(IS("setFont","(Ljavax/microedition/lcdui/Font;)V"))return 1;
        if(IS("setClip","(IIII)V")){v->cx=a[1]+v->tx;v->cy=a[2]+v->ty;v->cw=a[3];v->ch=a[4];return 1;}
        if(IS("getClipX","()I")){*out=v->cx-v->tx;return 1;}if(IS("getClipY","()I")){*out=v->cy-v->ty;return 1;}if(IS("getClipWidth","()I")){*out=v->cw;return 1;}if(IS("getClipHeight","()I")){*out=v->ch;return 1;}
        if(IS("clipRect","(IIII)V")){int x=a[1]+v->tx,y=a[2]+v->ty,r=x+a[3],bot=y+a[4];int oldr=v->cx+v->cw,oldbot=v->cy+v->ch;if(x<v->cx)x=v->cx;if(y<v->cy)y=v->cy;if(r>oldr)r=oldr;if(bot>oldbot)bot=oldbot;v->cx=x;v->cy=y;v->cw=r>x?r-x:0;v->ch=bot>y?bot-y:0;return 1;}
    }
    return 0;
#undef IS
}
