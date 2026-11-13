/* PocketJava: bounded, cooperative Java integer/reference interpreter.
 * Unsupported APIs/opcodes stop explicitly. This is not a full CLDC VM. */
#include "pocket.h"
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <math.h>
#include "miniz_tinfl.h"
#define CLASSES 160
#define FRAMES 48
#define SLOTS 4096
#define OBJECT_PAGE_SHIFT 6
#define OBJECT_PAGE_SIZE (1u<<OBJECT_PAGE_SHIFT)
#define OBJECT_PAGES 128
#define OBJECTS (OBJECT_PAGE_SIZE*OBJECT_PAGES)
#define REF 0x40000000u
#define STATIC 8
#define B_RUNTIME 40
#define B_LONG 39
enum {B_OBJECT=1,B_MIDLET,B_DISPLAYABLE,B_CANVAS,B_GAMECANVAS,B_GRAPHICS,B_DISPLAY,B_STRING,B_MATH,B_SYSTEM,B_RANDOM,B_FONT,B_THREAD,B_RUNNABLE,B_CLASS,B_EXCEPTION,B_STRINGBUFFER,B_INTEGER,B_SHORT,B_VECTOR,B_ENUM,B_HASHTABLE,B_INPUT,B_OUTPUT,B_BYTEINPUT,B_BYTEOUTPUT,B_DATAINPUT,B_DATAOUTPUT,B_IMAGE,B_COMMANDLISTENER,B_RECORDSTORE,B_MANAGER,B_PLAYER,B_CONTROL,B_VOLUME,B_PRINTSTREAM,B_DIRECTUTILS,B_DIRECTGRAPHICS,B_DEVICECONTROL,B_RECORDENUMERATION,B_DATE,B_CALENDAR,B_TIMER,B_TIMERTASK,B_READER,B_INPUTSTREAMREADER,B_BUFFEREDREADER,B_BYTE,B_CHARACTER,B_DATEFIELD,B_COMMAND};
typedef struct {uint32_t text_tag,payload;} CP;
#define CP_TAG(p) ((unsigned)((p)->text_tag>>28))
#define CP_TEXT(p) ((p)->text_tag&0x0fffffffu)
#define CP_SET_TAG(p,t) ((p)->text_tag=((p)->text_tag&0x0fffffffu)|((uint32_t)(t)<<28))
#define CP_SET_TEXT(p,x) ((p)->text_tag=((p)->text_tag&0xf0000000u)|((uint32_t)(x)&0x0fffffffu))
#define CP_VALUE(p) ((p)->payload)
#define CP_SET_VALUE(p,x) ((p)->payload=(uint32_t)(x))
#define CP_A(p) ((uint16_t)((p)->payload&0xffffu))
#define CP_B(p) ((uint16_t)((p)->payload>>16))
#define CP_SET_A(p,x) ((p)->payload=((p)->payload&0xffff0000u)|((uint16_t)(x)))
#define CP_SET_B(p,x) ((p)->payload=((p)->payload&0x0000ffffu)|((uint32_t)(uint16_t)(x)<<16))
typedef struct {uint16_t name,desc,flags,offset,wide,constant;int32_t value,high;} Field;
typedef struct {uint32_t code,len;uint16_t name,desc,flags,locals,stack,nh;} Method;
typedef struct {char *name;int parent,builtin,state,instance_fields,loading;unsigned char *bytes;CP *cp;Field *fields;Method *methods;uint16_t nc,nf,nm;} Class;
typedef struct {uint16_t cls;uint8_t kind,mark;uint32_t len;int32_t *data;void *native;} Object;
typedef struct {uint16_t cls,method,base,sp,end;int16_t init;uint16_t pc,lastpc;} Frame;
typedef struct {Frame *frames;int32_t *slots;int depth,started,slot_peak,slot_cap,frame_cap;int32_t runnable,thread,monitor_wait;uint32_t wake;} Context;
struct PJ {
    PJPlatform p; PJJar jar;unsigned char *jad;size_t jad_size;char error[PJ_ERROR_CAP],title[96],main[128];
    Class *classes[CLASSES];int nc,loading_depth;
    Object *object_pages[OBJECT_PAGES];uint8_t object_small_pages[OBJECT_PAGES/8];Frame *frames;int depth;int32_t *slots;Context *contexts[4];int current,yield,prefer_worker,worker_cursor;int32_t thrown,temporary[8];
    int32_t midlet,canvas,graphics,display,system_out;int dirty,rendered,paused,stopped,start_pending;int32_t serially[4];int serially_n;struct{int32_t timer,task;uint32_t due,period;int cancelled,periodic;} timers[8];int timer_n;
    int32_t runtime;
    uint32_t key_held,key_latched;
    struct {int32_t ref;uint16_t depth;uint8_t owner,pad;} monitors[32];
    struct {int key,release;} keys[16];unsigned kr,kw;
    uint32_t color;int tx,ty,cx,cy,cw,ch,native_high;uint64_t instructions;uint32_t random;
};
static unsigned object_page_cap(PJ*v,unsigned p){if(p>=OBJECT_PAGES||!v->object_pages[p])return 0;return (v->object_small_pages[p>>3]&(uint8_t)(1u<<(p&7)))?16u:OBJECT_PAGE_SIZE;}
static void object_page_small(PJ*v,unsigned p,int small){uint8_t bit=(uint8_t)(1u<<(p&7));if(small)v->object_small_pages[p>>3]|=bit;else v->object_small_pages[p>>3]&=(uint8_t)~bit;}
static Object *object_slot(PJ*v,unsigned i,int create){
    unsigned page=i>>OBJECT_PAGE_SHIFT,slot=i&(OBJECT_PAGE_SIZE-1),cap;Object*base;
    if(i>=OBJECTS||page>=OBJECT_PAGES)return NULL;base=v->object_pages[page];cap=object_page_cap(v,page);
    if(!base&&create){cap=OBJECT_PAGE_SIZE;base=pj_alloc(sizeof(Object)*cap);if(!base){cap=16;base=pj_alloc(sizeof(Object)*cap);}if(!base)return NULL;memset(base,0,sizeof(Object)*cap);v->object_pages[page]=base;object_page_small(v,page,cap<OBJECT_PAGE_SIZE);}
    if(!base||slot>=cap)return NULL;return base+slot;
}
static size_t native_buffer_size(Object*o){return o&&o->native?*(uint32_t*)o->native:0;}
static void *native_buffer_data(Object*o){return o&&o->native?(uint8_t*)o->native+sizeof(uint32_t):NULL;}
static void *native_buffer_alloc(size_t n){if(n>PJ_ENTRY_LIMIT)return NULL;uint8_t*p=pj_alloc(sizeof(uint32_t)+n);if(!p)return NULL;*(uint32_t*)p=(uint32_t)n;return p;}
static void *native_buffer_realloc(void*base,size_t n){if(n>PJ_ENTRY_LIMIT)return NULL;uint8_t*p=pj_realloc(base,sizeof(uint32_t)+n);if(!p)return NULL;*(uint32_t*)p=(uint32_t)n;return p;}
static unsigned object_free_slot(PJ*v,int create){
    unsigned p,s,start,cap;
    for(p=0;p<OBJECT_PAGES;p++)if(v->object_pages[p]){start=p?0:1;cap=object_page_cap(v,p);for(s=start;s<cap;s++)if(!v->object_pages[p][s].data)return (p<<OBJECT_PAGE_SHIFT)+s;}
    if(create)for(p=0;p<OBJECT_PAGES;p++)if(!v->object_pages[p]){cap=OBJECT_PAGE_SIZE;Object*base=pj_alloc(sizeof(Object)*cap);if(!base){cap=16;base=pj_alloc(sizeof(Object)*cap);}if(!base)return 0;memset(base,0,sizeof(Object)*cap);v->object_pages[p]=base;object_page_small(v,p,cap<OBJECT_PAGE_SIZE);return (p<<OBJECT_PAGE_SHIFT)+(p?0:1);}
    return 0;
}
typedef struct {PJ *v;const unsigned char*d;size_t n,pos;} Reader;
static int err(PJ*v,const char*s){if(!v->error[0]){size_t n=sizeof(v->error);strncpy(v->error,s,n-1);v->error[n-1]=0;}return 0;}
static int need(Reader*r,size_t n){if(r->pos>r->n||n>r->n-r->pos)return err(r->v,"Truncated class file");return 1;}
static unsigned u1(Reader*r){if(!need(r,1))return 0;return r->d[r->pos++];}
static unsigned u2(Reader*r){unsigned a=u1(r);return a<<8|u1(r);}
static uint32_t u4(Reader*r){uint32_t a=u2(r);return a<<16|u2(r);}
static int skip(Reader*r,size_t n){if(!need(r,n))return 0;r->pos+=n;return 1;}
static CP *cp(PJ*v,Class*c,unsigned i,unsigned tag){if(!i||i>=c->nc||(tag&&CP_TAG(c->cp+i)!=tag)){err(v,"Invalid constant pool reference");return NULL;}return c->cp+i;}
static int eq(PJ*v,Class*c,unsigned i,const char*s){CP*p=cp(v,c,i,1);return p&&CP_A(p)==strlen(s)&&!memcmp(c->bytes+CP_TEXT(p),s,CP_A(p));}
static int utf(PJ*v,Class*c,unsigned i,char*out,size_t n){CP*p=cp(v,c,i,1);if(!p)return 0;unsigned len=CP_A(p);if(len>=n)return err(v,"Class/member name too long");memcpy(out,c->bytes+CP_TEXT(p),len);out[len]=0;return 1;}
static int cname(PJ*v,Class*c,unsigned i,char*out){CP*p=cp(v,c,i,7);return p&&utf(v,c,CP_A(p),out,128);}
static int loadclass_inner(PJ*v,const char*name);
static int loadclass(PJ*v,const char*name){if(v->loading_depth>=8){err(v,"Class inheritance depth limit");return 0;}v->loading_depth++;int cls=loadclass_inner(v,name);v->loading_depth--;return cls;}
static void gc(PJ*v);
static int builtin_name(const char*s){
    if(!strcmp(s,"java/lang/Long"))return B_LONG;
    if(!strcmp(s,"java/lang/Runtime"))return B_RUNTIME;
    static const char *names[]={"","java/lang/Object","javax/microedition/midlet/MIDlet","javax/microedition/lcdui/Displayable","javax/microedition/lcdui/Canvas","javax/microedition/lcdui/game/GameCanvas","javax/microedition/lcdui/Graphics","javax/microedition/lcdui/Display","java/lang/String","java/lang/Math","java/lang/System","java/util/Random","javax/microedition/lcdui/Font","java/lang/Thread","java/lang/Runnable","java/lang/Class","java/lang/Exception","java/lang/StringBuffer","java/lang/Integer","java/lang/Short","java/util/Vector","java/util/Enumeration","java/util/Hashtable","java/io/InputStream","java/io/OutputStream","java/io/ByteArrayInputStream","java/io/ByteArrayOutputStream","java/io/DataInputStream","java/io/DataOutputStream","javax/microedition/lcdui/Image","javax/microedition/lcdui/CommandListener","javax/microedition/rms/RecordStore","javax/microedition/media/Manager","javax/microedition/media/Player","javax/microedition/media/Control","javax/microedition/media/control/VolumeControl","java/io/PrintStream","com/nokia/mid/ui/DirectUtils","com/nokia/mid/ui/DirectGraphics","com/nokia/mid/ui/DeviceControl","javax/microedition/rms/RecordEnumeration","java/util/Date","java/util/Calendar","java/util/Timer","java/util/TimerTask","java/io/Reader","java/io/InputStreamReader","java/io/BufferedReader","java/lang/Byte","java/lang/Character","javax/microedition/lcdui/DateField","javax/microedition/lcdui/Command"};
    size_t n=strlen(s);int i;for(i=1;i<=B_COMMAND;i++)if(!strcmp(s,names[i]))return i;
    if((n>=9&&!strcmp(s+n-9,"Exception"))||!strcmp(s,"java/lang/Throwable")||(n>=5&&!strcmp(s+n-5,"Error")))return B_EXCEPTION;
    if(!strcmp(s,"java/lang/StringBuilder"))return B_STRINGBUFFER;
    if(!strcmp(s,"javax/microedition/media/Controllable"))return B_CONTROL;
    if(!strcmp(s,"com/nokia/mid/ui/FullCanvas"))return B_CANVAS;
    return 0;
}
static void freeclass(Class*c){if(c){pj_free(c->name);pj_free(c->bytes);pj_free(c->cp);pj_free(c->fields);pj_free(c->methods);pj_free(c);}}
static int attrs(Reader*r,Class*c,Method*m,Field*f){
    unsigned n=u2(r),i;
    for(i=0;i<n&&!r->v->error[0];i++){
        unsigned name=u2(r);uint32_t len=u4(r);size_t end;
        if(!need(r,len))return 0;end=r->pos+len;
        if(m&&eq(r->v,c,name,"Code")){
            if(m->code)return err(r->v,"Duplicate Code attribute");
            m->stack=u2(r);m->locals=u2(r);m->len=u4(r);
            if(m->len>65535||!need(r,m->len)||r->pos>UINT32_MAX)return err(r->v,"Invalid bytecode size");
            m->code=(uint32_t)r->pos;skip(r,m->len);
            m->nh=u2(r);if(!skip(r,(size_t)m->nh*8))return 0;
            if(!attrs(r,c,NULL,NULL))return 0;
        }else if(f&&eq(r->v,c,name,"ConstantValue")){
            unsigned ci=u2(r);CP*p=cp(r->v,c,ci,0);unsigned t;
            if(!p)return 0;
            t=CP_TAG(p);
            if(t==3||t==4)f->value=(int32_t)CP_VALUE(p);
            else if(t==5||t==6){if(ci+1>=c->nc)return err(r->v,"Invalid wide ConstantValue");f->value=(int32_t)CP_VALUE(p);f->high=(int32_t)CP_VALUE(c->cp+ci+1);}
            else if(t==8)f->constant=(uint16_t)ci; /* String literal: materialize during initialize(). */
            else return err(r->v,"Unsupported ConstantValue type");
        }
        if(r->pos>end)return err(r->v,"Invalid attribute length");r->pos=end;
    }
    return !r->v->error[0];
}
static void compact_class_bytes(Class*c,size_t original){
    if(!c||!c->bytes||!original)return;
    size_t cursor=1;c->bytes[0]=0;
    for(unsigned i=1;i<c->nc;i++)if(CP_TAG(c->cp+i)==1){
        CP*p=c->cp+i;size_t src=CP_TEXT(p),len=CP_A(p);
        if(src>original||len>original-src)return;
        if(len)memmove(c->bytes+cursor,c->bytes+src,len);
        CP_SET_TEXT(p,(uint32_t)cursor);cursor+=len;
    }
    for(unsigned i=0;i<c->nm;i++)if(c->methods[i].code){
        Method*m=c->methods+i;size_t src=m->code,keep=(size_t)m->len+2u+(size_t)m->nh*8u;
        if(src>original||keep>original-src)return;
        memmove(c->bytes+cursor,c->bytes+src,keep);m->code=(uint32_t)cursor;cursor+=keep;
    }
    unsigned char*trim=pj_realloc(c->bytes,cursor);if(trim)c->bytes=trim;
}
static int loadclass_inner(PJ*v,const char*name){
    int i,id,b;Class*c;char path[140],parent[128];Reader r;size_t size;
    for(i=1;i<=v->nc;i++)if(!strcmp(v->classes[i]->name,name)){if(v->classes[i]->loading){err(v,"Cyclic class inheritance");return 0;}return i;}
    gc(v);
    if(v->nc+1>=CLASSES){err(v,"Class limit reached");return 0;}
    if(strlen(name)>=128){err(v,"Class name too long");return 0;}
    c=pj_alloc(sizeof(*c));if(!c){err(v,"Memory limit loading class");return 0;}memset(c,0,sizeof(*c));size_t name_size=strlen(name)+1;c->name=pj_alloc(name_size);if(!c->name){pj_free(c);err(v,"Memory limit loading class name");return 0;}memcpy(c->name,name,name_size);
    id=++v->nc;v->classes[id]=c;b=builtin_name(name);c->builtin=b;
    if(name[0]=='['){c->state=2;c->parent=loadclass(v,"java/lang/Object");return id;}
    if(b){c->state=2;c->instance_fields=8;if(b!=B_OBJECT)c->parent=loadclass(v,"java/lang/Object");if(b==B_OBJECT)c->instance_fields=0;if(b==B_CANVAS)c->parent=loadclass(v,!strcmp(name,"com/nokia/mid/ui/FullCanvas")?"javax/microedition/lcdui/Canvas":"javax/microedition/lcdui/Displayable");if(b==B_GAMECANVAS)c->parent=loadclass(v,"javax/microedition/lcdui/Canvas");if(b==B_BYTEINPUT||b==B_DATAINPUT)c->parent=loadclass(v,"java/io/InputStream");if(b==B_BYTEOUTPUT||b==B_DATAOUTPUT)c->parent=loadclass(v,"java/io/OutputStream");if(b==B_INPUTSTREAMREADER||b==B_BUFFEREDREADER)c->parent=loadclass(v,"java/io/Reader");return id;}
    strcpy(path,name);strcat(path,".class");c->bytes=jar_extract(&v->jar,path,&size,v->error);
    if(!c->bytes){if(!v->error[0])snprintf(v->error,sizeof(v->error),"Missing class: %.80s",name);return 0;}
    c->loading=1;r=(Reader){v,c->bytes,size,0};
    if(u4(&r)!=0xcafebabeu){err(v,"Invalid class magic");return 0;}u2(&r);i=u2(&r);
    /* Modern test fixtures can use Java 7/8 class containers; modern opcodes still fail. */
    if(i<45||i>52){err(v,"Class version unsupported (45..52)");return 0;}
    c->nc=u2(&r);if(c->nc<1){err(v,"Invalid constant pool count");return 0;}
    {
        size_t cp_bytes=(size_t)c->nc*sizeof(CP);
        c->cp=pj_alloc(cp_bytes);
        if(!c->cp){err(v,"Memory limit: constant pool");return 0;}
        memset(c->cp,0,cp_bytes);
    }
    for(i=1;i<c->nc&&!v->error[0];i++){
        CP*p=c->cp+i;unsigned cptag=u1(&r);CP_SET_TAG(p,cptag);
        switch(cptag){
        case 1:{unsigned len=u2(&r);CP_SET_A(p,len);if(!need(&r,len)||r.pos>0x0fffffffu)return err(v,"Invalid UTF constant offset");CP_SET_TEXT(p,r.pos);skip(&r,len);break;}
        case 3:case 4:CP_SET_VALUE(p,u4(&r));break;
        case 5:case 6:CP_SET_VALUE(p,u4(&r));if(i+1>=c->nc){err(v,"Invalid wide constant");return 0;}CP_SET_VALUE(c->cp+(++i),u4(&r));break;
        case 7:case 8:CP_SET_A(p,u2(&r));break;
        case 9:case 10:case 11:case 12:CP_SET_A(p,u2(&r));CP_SET_B(p,u2(&r));break;
        default:err(v,"Constant type unsupported (float/long/dynamic)");return 0;
        }
    }
    u2(&r);i=u2(&r);if(!cname(v,c,i,parent)||strcmp(parent,name)){err(v,"Class name mismatch");return 0;}
    i=u2(&r);if(i){if(!cname(v,c,i,parent))return 0;c->parent=loadclass(v,parent);if(!c->parent)return 0;if(c->parent==id){err(v,"Cyclic class inheritance");return 0;}c->instance_fields=v->classes[c->parent]->instance_fields;}
    i=u2(&r);if(!skip(&r,(size_t)i*2))return 0;
    c->nf=u2(&r);
    c->fields=pj_alloc(sizeof(Field)*c->nf);if(!c->fields){err(v,"Memory limit: fields");return 0;}memset(c->fields,0,sizeof(Field)*c->nf);
    for(i=0;i<c->nf;i++){Field*f=c->fields+i;char desc[128];f->flags=u2(&r);f->name=u2(&r);f->desc=u2(&r);if(!utf(v,c,f->desc,desc,sizeof(desc)))return 0;f->wide=desc[0]=='J'||desc[0]=='D';if(!(f->flags&STATIC)){int slots=f->wide?2:1;if(c->instance_fields>UINT16_MAX-slots)return err(v,"Instance field layout too large");f->offset=(uint16_t)c->instance_fields;c->instance_fields+=slots;}if(!attrs(&r,c,NULL,f))return 0;}
    c->nm=u2(&r);
    c->methods=pj_alloc(sizeof(Method)*c->nm);if(!c->methods){snprintf(v->error,sizeof(v->error),"Memory limit: methods %.70s (%u)",c->name,(unsigned)c->nm);return 0;}memset(c->methods,0,sizeof(Method)*c->nm);
    for(i=0;i<c->nm;i++){Method*m=c->methods+i;m->flags=u2(&r);m->name=u2(&r);m->desc=u2(&r);if(!attrs(&r,c,m,NULL))return 0;}
    if(!attrs(&r,c,NULL,NULL)||r.pos!=r.n){err(v,"Invalid class trailing data");return 0;}
    compact_class_bytes(c,size);c->loading=0;return id;
}
static Object *obj(PJ*v,int32_t ref){unsigned i=(uint32_t)ref-REF;Object*o=object_slot(v,i,0);if(!i||!o||!o->data){if(v->depth){Frame*fr=v->frames+v->depth-1;Class*cc=v->classes[fr->cls];char mn[64];mn[0]=0;if(cc&&fr->method<cc->nm){utf(v,cc,cc->methods[fr->method].name,mn,sizeof(mn));}snprintf(v->error,sizeof(v->error),"Invalid / null object reference (%s.%s pc=%u)",cc?cc->name:"?",mn[0]?mn:"?",fr->pc);}else err(v,"Invalid / null object reference");return NULL;}return o;}
static void mark(PJ*v,int32_t ref){unsigned i=(uint32_t)ref-REF;Object*o=object_slot(v,i,0);if(i&&o&&o->data&&!o->mark)o->mark=1;}
static void gc(PJ*v){
    int i,j,changed;unsigned p,s;Object*o;
    for(p=0;p<OBJECT_PAGES;p++)if(v->object_pages[p])for(s=p?0:1;s<object_page_cap(v,p);s++)v->object_pages[p][s].mark=0;
    mark(v,v->midlet);mark(v,v->canvas);mark(v,v->graphics);mark(v,v->display);mark(v,v->system_out);mark(v,v->thrown);for(i=0;i<8;i++)mark(v,v->temporary[i]);
    mark(v,v->runtime);
    for(i=0;i<32;i++)mark(v,v->monitors[i].ref);
    for(int t=0;t<4;t++)if(v->contexts[t]){Context*ctx=v->contexts[t];int depth=t==v->current?v->depth:ctx->depth;mark(v,ctx->runnable);mark(v,ctx->thread);for(i=0;i<depth;i++){Frame*f=ctx->frames+i;for(j=f->base;j<f->sp;j++)mark(v,ctx->slots[j]);}}
    for(i=1;i<=v->nc;i++){Class*c=v->classes[i];for(j=0;j<c->nf;j++)if(c->fields[j].flags&STATIC)mark(v,c->fields[j].value);for(j=1;j<c->nc;j++)if(CP_TAG(c->cp+j)==8){uint32_t slot=CP_TEXT(c->cp+j);if(slot)mark(v,(int32_t)(REF+slot));}}
    do{changed=0;for(p=0;p<OBJECT_PAGES;p++)if(v->object_pages[p])for(s=p?0:1;s<object_page_cap(v,p);s++){o=v->object_pages[p]+s;if(o->data&&o->mark==1){o->mark=2;changed=1;if(o->kind==0)for(j=0;j<(int)o->len;j++)mark(v,o->data[j]);else if(o->kind==1)for(j=0;j<(int)o->len;j++){uint16_t slot=((uint16_t*)o->data)[j];if(slot)mark(v,(int32_t)(REF+slot));}if(o->cls&&o->native&&(v->classes[o->cls]->builtin==B_VECTOR||v->classes[o->cls]->builtin==B_HASHTABLE)){int type=v->classes[o->cls]->builtin,count=o->data[0];size_t refs=(size_t)(count<0?0:count)*(type==B_HASHTABLE?2u:1u);int32_t*items=(int32_t*)native_buffer_data(o);for(size_t n=0;items&&n<refs;n++)mark(v,items[n]);}}}}while(changed);
    for(p=0;p<OBJECT_PAGES;p++)if(v->object_pages[p])for(s=p?0:1;s<object_page_cap(v,p);s++){o=v->object_pages[p]+s;if(o->data&&!o->mark){pj_free(o->data);if(o->native&&o->cls&&v->classes[o->cls]->builtin==B_INPUT)jar_stream_close((PJZipStream*)o->native);else pj_free(o->native);memset(o,0,sizeof(Object));}}
    for(p=1;p<OBJECT_PAGES;p++)if(v->object_pages[p]){unsigned cap=object_page_cap(v,p);for(s=0;s<cap;s++)if(v->object_pages[p][s].data)break;if(s==cap){pj_free(v->object_pages[p]);v->object_pages[p]=NULL;object_page_small(v,p,0);}}
    for(int t=0;t<4;t++)if(v->contexts[t]){Context*ctx=v->contexts[t];int depth=t==v->current?v->depth:ctx->depth,need=0;for(i=0;i<depth;i++)if(ctx->frames[i].end>need)need=ctx->frames[i].end;int cap=need<8?8:need;if(cap<ctx->slot_cap){int32_t*slots=pj_realloc(ctx->slots,(size_t)cap*sizeof(int32_t));if(slots){ctx->slots=slots;ctx->slot_cap=cap;if(t==v->current)v->slots=slots;}}}
}
void pj_collect(PJ*v){if(v)gc(v);}
static size_t element_size(int kind){return kind==14||kind==18||kind==2?1:kind==1||kind==15||kind==19?2:kind==21?8:4;}
static int32_t array_get(Object*o,uint32_t i){if(o->kind==1){uint16_t slot=((uint16_t*)o->data)[i];return slot?(int32_t)(REF+slot):0;}if(element_size(o->kind)==1)return o->kind==14?((uint8_t*)o->data)[i]:((int8_t*)o->data)[i];if(element_size(o->kind)==2)return o->kind==15?((uint16_t*)o->data)[i]:((int16_t*)o->data)[i];return o->data[i];}
static void array_put(Object*o,uint32_t i,int32_t x){if(o->kind==1)((uint16_t*)o->data)[i]=x?(uint16_t)((uint32_t)x-REF):0;else if(element_size(o->kind)==1)((uint8_t*)o->data)[i]=(uint8_t)x;else if(element_size(o->kind)==2)((uint16_t*)o->data)[i]=(uint16_t)x;else o->data[i]=x;}
static int32_t allocobj(PJ*v,int cls,int kind,uint32_t n){
    unsigned i;int pass;int32_t *data=NULL;Object*slot=NULL;
    size_t size=(n?n:1)*element_size(kind);if(size>PJ_ENTRY_LIMIT){err(v,"Object/array size limit");return 0;}
    for(pass=0;pass<3;pass++){
        i=object_free_slot(v,pass>0);
        if(i){slot=object_slot(v,i,0);if(slot)data=pj_alloc(size);}
        if(data){memset(data,0,size);*slot=(Object){(uint16_t)cls,(uint8_t)kind,0,n,data,NULL};return (int32_t)(REF+i);}
        gc(v);
    }
    i=object_free_slot(v,0);
    if(!i){int grow=0;for(unsigned p=0;p<OBJECT_PAGES;p++)if(!v->object_pages[p]){grow=1;break;}if(!grow)snprintf(v->error,sizeof(v->error),"Java object table exhausted (%u)",(unsigned)(OBJECTS-1));else snprintf(v->error,sizeof(v->error),"Java heap exhausted growing object table");}
    else {int contexts=0,peak0=v->contexts[0]?v->contexts[0]->slot_peak:0,peakw=0;for(int t=0;t<4;t++)if(v->contexts[t]){contexts++;if(t&&v->contexts[t]->slot_peak>peakw)peakw=v->contexts[t]->slot_peak;}snprintf(v->error,sizeof(v->error),"Java heap exhausted (%lu/%lu, need %lu k%d n%lu ctx%d slots%d/%d)",(unsigned long)pj_memory(),(unsigned long)PJ_MEMORY_LIMIT,(unsigned long)size,kind,(unsigned long)n,contexts,peak0,peakw);}
    return 0;
}
static int array_kind(const char*name){if(name[0]!='[')return 0;switch(name[1]){case 'Z':return 14;case 'C':return 15;case 'F':return 16;case 'D':return 17;case 'B':return 18;case 'S':return 19;case 'I':return 20;case 'J':return 21;default:return 1;}}
static int32_t multi_array(PJ*v,const char*name,const int32_t*sizes,int depth,int dims){int cls=loadclass(v,name);if(!cls)return 0;int32_t ref=allocobj(v,cls,array_kind(name),(uint32_t)sizes[depth]);if(!ref)return 0;v->temporary[depth]=ref;if(depth+1<dims){Object*parent=obj(v,ref);for(int i=0;i<sizes[depth]&&!v->error[0];i++){int32_t child=multi_array(v,name+1,sizes,depth+1,dims);if(!child){v->temporary[depth]=0;return 0;}array_put(parent,(uint32_t)i,child);}}v->temporary[depth]=0;return ref;}
static int32_t string(PJ*v,const unsigned char*s,size_t n){int32_t ref;Object*o;int cls=loadclass(v,"java/lang/String");if(!cls)return 0;ref=allocobj(v,cls,2,(uint32_t)n+1);if(!ref)return 0;o=obj(v,ref);memcpy(o->data,s,n);return ref;}
static const char *str(PJ*v,int32_t ref){Object*o=obj(v,ref);if(!o)return "";if(o->kind!=2){err(v,"Expected Java String");return "";}return (const char*)o->data;}
static int method(PJ*v,int *cls,const char*name,const char*desc){int guard=0;while(*cls&&guard++<CLASSES){Class*c=v->classes[*cls];int i;for(i=0;i<c->nm;i++)if(eq(v,c,c->methods[i].name,name)&&eq(v,c,c->methods[i].desc,desc))return i;if(!strcmp(name,"<init>"))break;*cls=c->parent;}return -1;}
static Context *context_new(int cap){Context*ctx;if(cap<8)cap=8;if(cap>SLOTS)cap=SLOTS;ctx=pj_alloc(sizeof(*ctx));if(!ctx)return NULL;memset(ctx,0,sizeof(*ctx));ctx->frame_cap=8;ctx->frames=pj_alloc((size_t)ctx->frame_cap*sizeof(Frame));if(!ctx->frames){pj_free(ctx);return NULL;}memset(ctx->frames,0,(size_t)ctx->frame_cap*sizeof(Frame));ctx->slots=pj_alloc((size_t)cap*sizeof(int32_t));if(!ctx->slots){pj_free(ctx->frames);pj_free(ctx);return NULL;}memset(ctx->slots,0,(size_t)cap*sizeof(int32_t));ctx->slot_cap=cap;return ctx;}
static int context_frame_reserve(PJ*v,int need){Context*ctx=v->contexts[v->current];int oldcap,cap;Frame*p;if(!ctx)return err(v,"Missing thread context");if(need<=ctx->frame_cap)return 1;if(need>FRAMES)return err(v,"Java frame limit");oldcap=ctx->frame_cap;cap=need;p=pj_realloc(ctx->frames,(size_t)cap*sizeof(Frame));if(!p){gc(v);ctx=v->contexts[v->current];if(!ctx)return err(v,"Missing thread context");if(need<=ctx->frame_cap){v->frames=ctx->frames;return 1;}oldcap=ctx->frame_cap;p=pj_realloc(ctx->frames,(size_t)cap*sizeof(Frame));}if(!p)return err(v,"Java frame RAM limit");if(cap>oldcap)memset(p+oldcap,0,(size_t)(cap-oldcap)*sizeof(Frame));ctx->frames=p;ctx->frame_cap=cap;v->frames=p;return 1;}
static int context_reserve(PJ*v,int need){Context*ctx=v->contexts[v->current];int cap,oldcap;int32_t*p;if(!ctx)return err(v,"Missing thread context");if(need<=ctx->slot_cap)return 1;if(need>SLOTS)return err(v,"Java stack limit");cap=need;if(cap>SLOTS)cap=SLOTS;oldcap=ctx->slot_cap;p=pj_realloc(ctx->slots,(size_t)cap*sizeof(int32_t));if(!p){gc(v);ctx=v->contexts[v->current];if(!ctx)return err(v,"Missing thread context");if(need<=ctx->slot_cap){v->slots=ctx->slots;return 1;}cap=need;if(cap>SLOTS)cap=SLOTS;oldcap=ctx->slot_cap;p=pj_realloc(ctx->slots,(size_t)cap*sizeof(int32_t));}if(!p)return err(v,"Java stack RAM limit");if(cap>oldcap)memset(p+oldcap,0,(size_t)(cap-oldcap)*sizeof(int32_t));ctx->slots=p;ctx->slot_cap=cap;v->slots=p;return 1;}
static int pushframe(PJ*v,int cls,int mi,const int32_t*args,int argc,int init){
    Frame*f;Method*m=v->classes[cls]->methods+mi;int base=v->depth?v->frames[v->depth-1].end:0;int i;
    if(!m->code)return err(v,"Abstract/native application method unsupported");
    if(v->depth>=FRAMES||m->locals<argc||base+m->locals+m->stack>SLOTS)return err(v,"Java stack limit");
    if(!context_frame_reserve(v,v->depth+1))return 0;
    if(!context_reserve(v,base+m->locals+m->stack))return 0;
    f=v->frames+v->depth++;*f=(Frame){cls,mi,base,base+m->locals,base+m->locals+m->stack,init,0,0};
    if(v->contexts[v->current]&&f->end>v->contexts[v->current]->slot_peak)v->contexts[v->current]->slot_peak=f->end;
    memset(v->slots+base,0,(m->locals+m->stack)*sizeof(int32_t));for(i=0;i<argc;i++)v->slots[base+i]=args[i];return 1;
}
static int initialize(PJ*v,int cls){int mi,target=cls;Class*c=v->classes[cls];if(c->state)return 1;if(c->parent&&!initialize(v,c->parent))return 0;
    for(unsigned n=0;n<c->nf;n++){Field*f=c->fields+n;CP*p,*t;int32_t ref;if(!(f->flags&STATIC)||!f->constant)continue;p=cp(v,c,f->constant,8);if(!p)return 0;t=cp(v,c,CP_A(p),1);if(!t)return 0;ref=string(v,c->bytes+CP_TEXT(t),CP_A(t));if(!ref)return 0;f->value=ref;f->constant=0;}
    c->state=1;mi=method(v,&target,"<clinit>","()V");if(mi<0){c->state=2;return 1;}if(!pushframe(v,target,mi,NULL,0,cls))return 0;return 0;}
static int signature(PJ*v,const char*s,int*argc,int*ret){int n=0;const char*p=s;if(*p++!='(')return err(v,"Invalid method descriptor");while(*p&&*p!=')'){int array=0;while(*p=='['){array=1;p++;}if(!*p)return err(v,"Invalid array descriptor");if(*p=='L'){p++;while(*p&&*p!=';')p++;if(*p!=';')return err(v,"Invalid reference descriptor");p++;}else if(strchr("BCISZJDF",*p)){if(!array&&(*p=='J'||*p=='D'))n++;p++;}else return err(v,"Invalid argument descriptor");if(++n>32)return err(v,"Too many method arguments");}if(*p!=')')return err(v,"Invalid method descriptor");p++;if(!*p)return err(v,"Missing return descriptor");*ret=*p=='V'?0:(*p=='J'||*p=='D'?2:1);*argc=n;return 1;}
static int push(PJ*v,Frame*f,int32_t x){if(f->sp>=f->end)return err(v,"Operand stack overflow");v->slots[f->sp++]=x;return 1;}
static int32_t pop(PJ*v,Frame*f){Method*m=v->classes[f->cls]->methods+f->method;if(f->sp<=f->base+m->locals){err(v,"Operand stack underflow");return 0;}return v->slots[--f->sp];}
static int32_t local(PJ*v,Frame*f,unsigned i){if(i>=v->classes[f->cls]->methods[f->method].locals){err(v,"Invalid local index");return 0;}return v->slots[f->base+i];}
static void setlocal(PJ*v,Frame*f,unsigned i,int32_t x){if(i>=v->classes[f->cls]->methods[f->method].locals){err(v,"Invalid local index");return;}v->slots[f->base+i]=x;}
static void switch_context(PJ*v,int index){if(v->contexts[v->current])v->contexts[v->current]->depth=v->depth;v->current=index;v->frames=v->contexts[index]->frames;v->slots=v->contexts[index]->slots;v->depth=v->contexts[index]->depth;}
static uint64_t popwide(PJ*v,Frame*f){uint32_t low=(uint32_t)pop(v,f),high=(uint32_t)pop(v,f);return (uint64_t)high<<32|low;}
static void pushwide(PJ*v,Frame*f,uint64_t x){push(v,f,(int32_t)(x>>32));push(v,f,(int32_t)x);}
static double double_bits(uint64_t bits){double d;memcpy(&d,&bits,sizeof(d));return d;}
static float float_bits(int32_t bits){float f;memcpy(&f,&bits,4);return f;}
static int32_t bits_float(float f){int32_t bits;memcpy(&bits,&f,4);return bits;}
static int game_action(int key){switch(key){case -1:case 50:return 1;case -2:case 56:return 6;case -3:case 52:return 2;case -4:case 54:return 5;case -5:case 53:return 8;case 49:return 9;case 51:return 10;case 55:return 11;case 57:return 12;default:return 0;}}
static uint64_t bits_double(double d){uint64_t bits;memcpy(&bits,&d,sizeof(bits));return bits;}
static double popdouble(PJ*v,Frame*f){return double_bits(popwide(v,f));}
static void pushdouble(PJ*v,Frame*f,double d){pushwide(v,f,bits_double(d));}
static double slots_double(int32_t high,int32_t low){return double_bits((uint64_t)(uint32_t)high<<32|(uint32_t)low);}
static void native_double(PJ*v,double d,int32_t*out){uint64_t bits=bits_double(d);v->native_high=(int32_t)(bits>>32);*out=(int32_t)bits;}
static double pj_sin(double x){const double pi=3.14159265358979323846,tau=6.28318530717958647692,half=1.57079632679489661923;while(x>pi)x-=tau;while(x<-pi)x+=tau;if(x>half)x=pi-x;else if(x<-half)x=-pi-x;double q=x*x;return x*(1.0+q*(-1.0/6.0+q*(1.0/120.0+q*(-1.0/5040.0+q*(1.0/362880.0+q*(-1.0/39916800.0))))));}
static unsigned utf_unit(const char**text){const unsigned char*p=(const unsigned char*)*text;unsigned c=*p++;if(c>=192&&c<224&&(*p&192)==128){c=(c&31)<<6|(*p++&63);}else if(c>=224&&c<240&&(p[0]&192)==128&&p[1]&&(p[1]&192)==128){c=(c&15)<<12|(p[0]&63)<<6|(p[1]&63);p+=2;}*text=(const char*)p;return c;}
static size_t utf_length(const char*s){size_t n=0;while(*s){utf_unit(&s);n++;}return n;}
static const char*utf_at(const char*s,size_t index){while(index--&&*s)utf_unit(&s);return s;}
#define STRING_META_VALID 0x80000000u
#define STRING_META_ASCII 0x40000000u
#define STRING_META_LEN_MASK 0x3fffffffu
typedef struct {uint32_t length,index,offset;} StringCursor;
#define STRING_STORAGE_ASCII 0x80000000u
#define STRING_STORAGE_LEN_MASK 0x7fffffffu
static uint32_t string_meta(Object*o){if(o->native){StringCursor*m=(StringCursor*)o->native;return STRING_META_VALID|(m->length&STRING_META_LEN_MASK);}if(o->len&STRING_STORAGE_ASCII){uint32_t bytes=o->len&STRING_STORAGE_LEN_MASK;return STRING_META_VALID|STRING_META_ASCII|((bytes?bytes-1:0)&STRING_META_LEN_MASK);}const char*s=(const char*)o->data;uint32_t n=0;int ascii=1;while(*s){if((unsigned char)*s>=128)ascii=0;utf_unit(&s);n++;}if(ascii)o->len|=STRING_STORAGE_ASCII;return STRING_META_VALID|(ascii?STRING_META_ASCII:0)|(n&STRING_META_LEN_MASK);}
static StringCursor*string_cursor(Object*o,uint32_t length){StringCursor*m=(StringCursor*)o->native;if(m)return m;m=pj_alloc(sizeof(*m));if(!m)return NULL;m->length=length;m->index=0;m->offset=0;o->native=m;return m;}
static int throw_exception(PJ*v,const char*name,const char*message){
#ifdef PJ_TRACE_EXCEPTIONS
    printf("EXCEPTION %s: %s\n",name,message?message:"");
#endif
    int cls=loadclass(v,name);if(!cls)return -1;v->thrown=allocobj(v,cls,0,8);if(v->thrown&&message)obj(v,v->thrown)->data[0]=string(v,(const unsigned char*)message,strlen(message));return -1;}
static unsigned be16(const unsigned char*p){return (unsigned)p[0]<<8|p[1];}
static void propagate(PJ*v){while(v->thrown&&v->depth&&!v->error[0]){Frame*f=v->frames+v->depth-1;Class*c=v->classes[f->cls];Method*m=c->methods+f->method;for(unsigned i=0;i<m->nh;i++){const unsigned char*h=c->bytes+m->code+m->len+2+i*8;unsigned start=be16(h),end=be16(h+2),target=be16(h+4),type=be16(h+6);if(f->lastpc<start||f->lastpc>=end)continue;char name[128];Object*ex=obj(v,v->thrown);if(!ex)return;int matches=!type;if(type&&cname(v,c,type,name)){const char*actual=v->classes[ex->cls]->name;matches=!strcmp(name,actual)||!strcmp(name,"java/lang/Exception")||!strcmp(name,"java/lang/Throwable");}if(matches){if(target>=m->len){err(v,"Invalid exception handler");return;}int32_t ref=v->thrown;v->thrown=0;f->sp=f->base+m->locals;f->pc=target;push(v,f,ref);return;}}v->depth--;}
    if(v->thrown&&!v->error[0]){Object*ex=obj(v,v->thrown);if(ex)snprintf(v->error,sizeof(v->error),"Uncaught %.100s",v->classes[ex->cls]->name);}}
#include "png.inc"
static int native(PJ*v,int b,const char*name,const char*desc,int32_t*a,int32_t*out);
#include "library.inc"
static int native(PJ*v,int b,const char*name,const char*desc,int32_t*a,int32_t*out){
    int handled;*out=0;v->native_high=0;handled=library_native(v,b,name,desc,a,out);if(handled)return handled;
#define IS(n,d) (!strcmp(name,n)&&!strcmp(desc,d))
    if(IS("<init>","()V")&&(b==B_OBJECT||b==B_MIDLET||b==B_CANVAS))return 1;
    if(b==B_RANDOM&&IS("<init>","()V")){Object*o=obj(v,a[0]);if(!o)return -1;v->random=v->random*1664525u+1013904223u;o->data[0]=(int32_t)v->random;return 1;}
    if(b==B_RANDOM&&IS("<init>","(J)V")){Object*o=obj(v,a[0]);if(!o)return -1;uint64_t seed=(uint64_t)(uint32_t)a[1]<<32|(uint32_t)a[2];uint32_t mixed=(uint32_t)seed^(uint32_t)(seed>>32)^0xDEECE66Du;o->data[0]=(int32_t)mixed;return 1;}
    if(b==B_GAMECANVAS&&IS("<init>","(Z)V"))return 1;
    if(b==B_MIDLET&&IS("notifyDestroyed","()V")){v->stopped=1;return 1;}
    if(b==B_DISPLAY&&IS("getDisplay","(Ljavax/microedition/midlet/MIDlet;)Ljavax/microedition/lcdui/Display;")){*out=v->display;return 1;}
    if(b==B_DISPLAY&&IS("getCurrent","()Ljavax/microedition/lcdui/Displayable;")){*out=v->canvas;return 1;}
    if(b==B_DISPLAY&&IS("setCurrent","(Ljavax/microedition/lcdui/Displayable;)V")){if(!obj(v,a[1]))return -1;v->canvas=a[1];v->dirty=1;return 1;}
    if(b==B_DISPLAY&&IS("callSerially","(Ljava/lang/Runnable;)V")){if(!a[0])return throw_exception(v,"java/lang/NullPointerException",NULL);if(v->serially_n<4){v->serially[v->serially_n++]=a[0];}else{int32_t r=a[0];int cls=obj(v,r)?obj(v,r)->cls:0;if(cls){int mi=method(v,&cls,"run","()V");if(mi>=0)pushframe(v,cls,mi,NULL,0,0);}}return 1;}
    if(b==B_DISPLAY&&IS("flashBacklight","(I)V"))return 1;
    if(b==B_DISPLAY&&IS("vibrate","(I)Z")){*out=1;return 1;}
    if(b==B_DEVICECONTROL&&(IS("setLights","(II)V")||IS("flashLights","(II)V")||IS("vibrate","(I)V")||IS("startVibra","(I)V")||IS("stopVibra","()V")))return 1;
    if(b==B_DEVICECONTROL&&IS("getLights","()I")){*out=1;return 1;}
    if((b==B_CANVAS||b==B_GAMECANVAS||b==B_DISPLAYABLE)&&IS("getWidth","()I")){*out=240;return 1;}
    if((b==B_CANVAS||b==B_GAMECANVAS||b==B_DISPLAYABLE)&&IS("getHeight","()I")){*out=320;return 1;}
    if((b==B_CANVAS||b==B_GAMECANVAS||b==B_DISPLAYABLE)&&IS("isShown","()Z")){*out=a[0]&&a[0]==v->canvas;return 1;}
    if((b==B_CANVAS||b==B_GAMECANVAS)&&(IS("repaint","()V")||IS("repaint","(IIII)V"))){v->dirty=1;return 1;}
    if((b==B_CANVAS||b==B_GAMECANVAS)&&IS("setFullScreenMode","(Z)V"))return 1;
    if((b==B_CANVAS||b==B_GAMECANVAS)&&IS("isDoubleBuffered","()Z")){*out=0;return 1;}
    if((b==B_CANVAS||b==B_GAMECANVAS)&&IS("getGameAction","(I)I")){switch(a[1]){case -1:case 50:*out=1;break;case -2:case 56:*out=6;break;case -3:case 52:*out=2;break;case -4:case 54:*out=5;break;case -5:case 53:*out=8;break;}return 1;}
    if((b==B_CANVAS||b==B_GAMECANVAS)&&(IS("hasPointerEvents","()Z")||IS("hasPointerMotionEvents","()Z")||IS("hasRepeatEvents","()Z"))){*out=0;return 1;}
    if((b==B_CANVAS||b==B_GAMECANVAS)&&IS("isPointerPressed","(I)Z")){*out=0;return 1;}
    if((b==B_CANVAS||b==B_GAMECANVAS)&&(IS("getPointerX","(I)I")||IS("getPointerY","(I)I"))){*out=0;return 1;}
    if((b==B_CANVAS||b==B_GAMECANVAS||b==B_DISPLAYABLE)&&IS("getKeyName","(I)Ljava/lang/String;")){
        char buf[16];const char*s=buf;
        switch(a[1]){case -1:case 50:s="UP";break;case -2:case 56:s="DOWN";break;case -3:case 52:s="LEFT";break;case -4:case 54:s="RIGHT";break;case -5:case 53:s="SELECT";break;case -6:case 49:s="SOFT1";break;case -7:case 55:s="SOFT2";break;case -8:case 42:s="STAR";break;case -9:case 51:s="BACK";break;case 35:s="POUND";break;default:snprintf(buf,sizeof(buf),"%d",(int)a[1]);s=buf;break;}
        int32_t ref=string(v,(const unsigned char*)s,strlen(s));if(!ref)return -1;*out=ref;return 1;
    }
    if(b==B_GRAPHICS){
        if(IS("setColor","(I)V")){v->color=(uint32_t)a[1]&0xffffffu;return 1;}
        if(IS("setColor","(III)V")){v->color=((uint32_t)a[1]&255)<<16|((uint32_t)a[2]&255)<<8|((uint32_t)a[3]&255);return 1;}
        if(IS("getColor","()I")){*out=(int32_t)v->color;return 1;}
        if(IS("translate","(II)V")){v->tx=(int32_t)((uint32_t)v->tx+(uint32_t)a[1]);v->ty=(int32_t)((uint32_t)v->ty+(uint32_t)a[2]);return 1;}
        if(IS("getTranslateX","()I")){*out=v->tx;return 1;}if(IS("getTranslateY","()I")){*out=v->ty;return 1;}
        if(IS("fillRect","(IIII)V")||IS("drawRect","(IIII)V")){
            int64_t x=(int64_t)a[1]+v->tx,y=(int64_t)a[2]+v->ty,w=a[3],h=a[4],right,bottom;
            if(w<=0||h<=0)return 1;right=x+w;bottom=y+h;
            if(x<0)x=0;if(y<0)y=0;if(right>240)right=240;if(bottom>320)bottom=320;if(right<=x||bottom<=y)return 1;
            if(IS("fillRect","(IIII)V"))gfx_rect(v,a[0],(int)x,(int)y,(int)(right-x),(int)(bottom-y),v->color);
            else {gfx_rect(v,a[0],(int)x,(int)y,(int)(right-x),1,v->color);gfx_rect(v,a[0],(int)x,(int)bottom-1,(int)(right-x),1,v->color);gfx_rect(v,a[0],(int)x,(int)y,1,(int)(bottom-y),v->color);gfx_rect(v,a[0],(int)right-1,(int)y,1,(int)(bottom-y),v->color);}return 1;
        }
        if(IS("drawLine","(IIII)V")){int x0=a[1]+v->tx,y0=a[2]+v->ty,x1=a[3]+v->tx,y1=a[4]+v->ty;int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1,dy=y1>y0?y0-y1:y1-y0,sy=y0<y1?1:-1,er=dx+dy;for(;;){gfx_rect(v,a[0],x0,y0,1,1,v->color);if(x0==x1&&y0==y1)break;int e2=er<<1;if(e2>=dy){er+=dy;x0+=sx;}if(e2<=dx){er+=dx;y0+=sy;}}return 1;}
        if(IS("drawArc","(IIIIII)V")||IS("fillArc","(IIIIII)V")||IS("drawOval","(IIII)V")||IS("fillOval","(IIII)V")){int x,y,w,h,start,extent;if(IS("drawOval","(IIII)V")||IS("fillOval","(IIII)V")){x=a[1];y=a[2];w=a[3];h=a[4];start=0;extent=360;}else{x=a[1];y=a[2];w=a[3];h=a[4];start=a[5];extent=a[6];}if(w<=0||h<=0)return 1;int tx=v->tx,ty=v->ty;int X=x+tx,Y=y+ty;double cx=X+(double)w/2.0,cy=Y+(double)h/2.0,rx=w/2.0,ry=h/2.0;int fill=IS("fillArc","(IIIIII)V")||IS("fillOval","(IIII)V");if(fill){for(int yy=Y;yy<Y+h;yy++)for(int xx=X;xx<X+w;xx++){double nx=(xx+0.5-cx)/rx,ny=(yy+0.5-cy)/ry;if(nx*nx+ny*ny>1.0)continue;double ang=atan2(yy+0.5-cy,xx+0.5-cx)*180.0/3.14159265358979323846;if(ang<0)ang+=360.0;int inarc=0;if(extent>=360||extent<=-360)inarc=1;else{int a1=start,a2=start+extent;if(extent<0){a1=start+extent;a2=start;}double lo=a1%360;if(lo<0)lo+=360;double hi=a2%360;if(hi<0)hi+=360;if(extent>=0){if(lo<=hi)inarc=ang>=lo&&ang<=hi;else inarc=ang>=lo||ang<=hi;}else{if(lo<=hi)inarc=ang>=lo&&ang<=hi;else inarc=ang>=lo||ang<=hi;}}if(inarc)gfx_rect(v,a[0],xx,yy,1,1,v->color);}}else{int total=extent>=0?extent:-extent;if(total>360)total=360;if(total<1)total=1;int nsteps=total*4;if(nsteps<36)nsteps=36;if(nsteps>720)nsteps=720;int prevx=0,prevy=0,first=1;for(int i=0;i<=nsteps;i++){double ang=(start+(extent>=0?1:-1)*(double)i*total/nsteps)*3.14159265358979323846/180.0;int px=(int)lround(cx+rx*cos(ang));int py=(int)lround(cy+ry*sin(ang));if(!first){int x0=prevx,y0=prevy,x1=px,y1=py;int dx=x1>x0?x1-x0:x0-x1,sx=x0<x1?1:-1,dy=y1>y0?y0-y1:y1-y0,sy=y0<y1?1:-1,er=dx+dy;for(;;){gfx_rect(v,a[0],x0,y0,1,1,v->color);if(x0==x1&&y0==y1)break;int e2=er<<1;if(e2>=dy){er+=dy;x0+=sx;}if(e2<=dx){er+=dx;y0+=sy;}}}prevx=px;prevy=py;first=0;}}return 1;}
        if(IS("setClip","(IIII)V")){v->cx=a[1]+v->tx;v->cy=a[2]+v->ty;v->cw=a[3];v->ch=a[4];if(v->cw<0)v->cw=0;if(v->ch<0)v->ch=0;return 1;}
        if(IS("clipRect","(IIII)V")){int nx=a[1]+v->tx,ny=a[2]+v->ty,nw=a[3],nh=a[4];if(nx>v->cx)v->cx=nx;if(ny>v->cy)v->cy=ny;if(nx+nw<v->cx+v->cw)v->cw=nx+nw-v->cx;if(ny+nh<v->cy+v->ch)v->ch=ny+nh-v->cy;if(v->cw<0)v->cw=0;if(v->ch<0)v->ch=0;return 1;}
        if(IS("getClipX","()I")){*out=v->cx-v->tx;return 1;}
        if(IS("getClipY","()I")){*out=v->cy-v->ty;return 1;}
        if(IS("getClipWidth","()I")){*out=v->cw;return 1;}
        if(IS("getClipHeight","()I")){*out=v->ch;return 1;}
        if(IS("fillRoundRect","(IIIIII)V")){int x=a[1]+v->tx,y=a[2]+v->ty,w=a[3],h=a[4],aw=a[5],ah=a[6];if(w<=0||h<=0)return 1;if(aw<0)aw=0;if(ah<0)ah=0;if(aw>w)aw=w;if(ah>h)ah=h;aw/=2;ah/=2;if(aw==0||ah==0){/* fall back to rect */gfx_rect(v,a[0],x,y,w,h,v->color);return 1;}for(int row=0;row<h;row++){int inset=0;if(row<ah){double t=(double)(ah-row)/ah;inset=(int)((1.0-sqrt(1.0-t*t))*aw+0.5);}else if(row>=h-ah){double t=(double)(row-(h-ah))/ah;inset=(int)((1.0-sqrt(1.0-t*t))*aw+0.5);}if(inset<0)inset=0;if(inset*2>w)inset=w/2;gfx_rect(v,a[0],x+inset,y+row,w-inset*2,1,v->color);}return 1;}
        if(IS("drawRoundRect","(IIIIII)V")){int x=a[1]+v->tx,y=a[2]+v->ty,w=a[3],h=a[4],aw=a[5],ah=a[6];if(w<=0||h<=0)return 1;if(aw<0)aw=0;if(ah<0)ah=0;if(aw>w)aw=w;if(ah>h)ah=h;aw/=2;ah/=2;if(aw==0||ah==0){gfx_rect(v,a[0],x,y,w,1,v->color);gfx_rect(v,a[0],x,y+h-1,w,1,v->color);gfx_rect(v,a[0],x,y,1,h,v->color);gfx_rect(v,a[0],x+w-1,y,1,h,v->color);return 1;}for(int row=0;row<h;row++){int inset=0;if(row<ah){double t=(double)(ah-row)/ah;inset=(int)((1.0-sqrt(1.0-t*t))*aw+0.5);}else if(row>=h-ah){double t=(double)(row-(h-ah))/ah;inset=(int)((1.0-sqrt(1.0-t*t))*aw+0.5);}if(inset<0)inset=0;if(inset*2>w)inset=w/2;int top=(row==0||row==h-1);int leftin=inset,rightin=w-1-inset;if(top){gfx_rect(v,a[0],x+inset,y+row,w-inset*2,1,v->color);}else{gfx_rect(v,a[0],x+inset,y+row,1,1,v->color);gfx_rect(v,a[0],x+w-1-inset,y+row,1,1,v->color);}}return 1;}
        if(IS("drawString","(Ljava/lang/String;III)V")){
            const char*s=str(v,a[1]);int64_t x=(int64_t)a[2]+v->tx,y=(int64_t)a[3]+v->ty;unsigned anchor=(unsigned)a[4];size_t width=utf_length(s)*8;
            if(anchor&8)x-=(int64_t)width;else if(anchor&1)x-=(int64_t)width/2;if(anchor&32)y-=16;else if(anchor&64)y-=12;
            if(x>=0&&x<240&&y>=0&&y<320)v->p.text((int)x,(int)y,s,v->color);return 1;
        }
    }
    if(b==B_MATH&&IS("abs","(I)I")){*out=a[0]<0?(int32_t)(0u-(uint32_t)a[0]):a[0];return 1;}
    if(b==B_MATH&&IS("sin","(D)D")){native_double(v,pj_sin(slots_double(a[0],a[1])),out);return 1;}
    if(b==B_MATH&&(IS("min","(II)I")||IS("max","(II)I"))){*out=IS("min","(II)I")?(a[0]<a[1]?a[0]:a[1]):(a[0]>a[1]?a[0]:a[1]);return 1;}
    if(b==B_MATH&&(IS("min","(JJ)J")||IS("max","(JJ)J"))){int64_t x=(int64_t)(((uint64_t)(uint32_t)a[0]<<32)|(uint32_t)a[1]);int64_t y=(int64_t)(((uint64_t)(uint32_t)a[2]<<32)|(uint32_t)a[3]);int64_t z=IS("min","(JJ)J")?(x<y?x:y):(x>y?x:y);uint64_t r=(uint64_t)z;v->native_high=(int32_t)(r>>32);*out=(int32_t)r;return 1;}
    if(b==B_RANDOM&&(IS("nextInt","()I")||IS("nextInt","(I)I"))){Object*o=obj(v,a[0]);if(!o)return -1;uint32_t state=(uint32_t)o->data[0];state=state*1664525u+1013904223u;o->data[0]=(int32_t)state;*out=(int32_t)(state>>1);if(IS("nextInt","(I)I")){if(a[1]<=0){err(v,"Random bound must be positive");return -1;}*out%=a[1];}return 1;}
    if(b==B_STRING&&IS("length","()I")){Object*o=obj(v,a[0]);if(!o)return -1;*out=(int32_t)(string_meta(o)&STRING_META_LEN_MASK);return 1;}
    if(b==B_STRING&&IS("charAt","(I)C")){Object*o=obj(v,a[0]);if(!o)return -1;uint32_t meta=string_meta(o),n=meta&STRING_META_LEN_MASK,target=(uint32_t)a[1];if(a[1]<0||target>=n){err(v,"String index out of bounds");return -1;}const char*base=(const char*)o->data;if(meta&STRING_META_ASCII){*out=(unsigned char)base[target];return 1;}StringCursor*m=string_cursor(o,n);if(!m){err(v,"String cursor RAM limit");return -1;}uint32_t idx=target>=m->index?m->index:0,off=target>=m->index?m->offset:0;const char*s=base+off;while(idx<target&&*s){utf_unit(&s);idx++;}m->index=target;m->offset=(uint32_t)(s-base);*out=(int32_t)utf_unit(&s);return 1;}
    return 0;
#undef IS
}
static int member(PJ*v,Class*c,unsigned index,int tag,int*owner,char*name,char*desc){CP*p=cp(v,c,index,tag),*nt;if(!p)return 0;unsigned cached=CP_TEXT(p);if(!cached){if(!cname(v,c,CP_A(p),name))return 0;cached=(unsigned)loadclass(v,name);if(cached)CP_SET_TEXT(p,cached);}*owner=(int)cached;if(!*owner)return 0;nt=cp(v,c,CP_B(p),12);return nt&&utf(v,c,CP_A(nt),name,128)&&utf(v,c,CP_B(nt),desc,128);}
static void returnframe(PJ*v,int has,int32_t x){int init=v->frames[v->depth-1].init;v->depth--;if(init==-1)v->rendered=1;else if(init>0)v->classes[init]->state=2;if(has&&v->depth)push(v,v->frames+v->depth-1,x);}
static unsigned byte(PJ*v,Frame*f,Method*m){if(f->pc>=m->len){err(v,"Bytecode read out of bounds");return 0;}return v->classes[f->cls]->bytes[m->code+f->pc++];}
static unsigned word(PJ*v,Frame*f,Method*m){unsigned a=byte(v,f,m);return a<<8|byte(v,f,m);}
static uint32_t dword(PJ*v,Frame*f,Method*m){uint32_t a=word(v,f,m);return a<<16|word(v,f,m);}
static void branch(PJ*v,Frame*f,Method*m,uint32_t pc,int32_t off){int64_t dst=(int64_t)pc+off;if(dst<0||dst>=m->len)err(v,"Branch target out of bounds");else f->pc=(uint32_t)dst;}
static int monitor_enter(PJ*v,int32_t ref){
    if(!ref)return throw_exception(v,"java/lang/NullPointerException",NULL);if(!obj(v,ref))return -1;
    int empty=-1;for(int i=0;i<32;i++){if(!v->monitors[i].ref){if(empty<0)empty=i;continue;}if(v->monitors[i].ref!=ref)continue;
        if(v->monitors[i].owner!=v->current+1){v->contexts[v->current]->monitor_wait=ref;return 0;}
        if(v->monitors[i].depth==65535)return err(v,"Monitor recursion limit");v->monitors[i].depth++;return 1;}
    if(empty<0)return err(v,"Monitor count limit");v->monitors[empty].ref=ref;v->monitors[empty].owner=(uint8_t)(v->current+1);v->monitors[empty].depth=1;return 1;
}
static int monitor_exit(PJ*v,int32_t ref){
    if(!ref)return throw_exception(v,"java/lang/NullPointerException",NULL);
    for(int i=0;i<32;i++)if(v->monitors[i].ref==ref&&v->monitors[i].owner==v->current+1){if(!--v->monitors[i].depth){v->monitors[i].ref=0;for(int t=0;t<4;t++)if(v->contexts[t]&&v->contexts[t]->monitor_wait==ref)v->contexts[t]->monitor_wait=0;}return 1;}
    return throw_exception(v,"java/lang/IllegalMonitorStateException",NULL);
}
static void execute(PJ*v){
    Frame*f=v->frames+v->depth-1;Class*c=v->classes[f->cls];Method*m=c->methods+f->method;uint32_t pc0=f->pc;unsigned op=byte(v,f,m),i;int32_t a,b,x;int owner,mi,argc,ret;char name[128],desc[128];
#define POP() pop(v,f)
#define PUSH(x) push(v,f,(int32_t)(x))
#define BYTE() byte(v,f,m)
#define WORD() word(v,f,m)
    f->lastpc=pc0;v->instructions++;
    switch(op){
    case 0:break;case 1:PUSH(0);break;
    case 2:case 3:case 4:case 5:case 6:case 7:case 8:PUSH((int)op-3);break;
    case 16:PUSH((int8_t)BYTE());break;case 17:PUSH((int16_t)WORD());break;
    case 9:case 10:pushwide(v,f,op-9);break;
    case 14:case 15:pushdouble(v,f,op==15?1.0:0.0);break;
    case 11:case 12:case 13:PUSH(bits_float((float)(op-11)));break;
    case 20:{CP*p;i=WORD();p=cp(v,c,i,0);if(p&&(CP_TAG(p)==5||CP_TAG(p)==6)&&i+1<c->nc){PUSH(CP_VALUE(p));PUSH(CP_VALUE(c->cp+i+1));}else err(v,"Invalid wide constant");break;}
    case 22:case 24:i=BYTE();PUSH(local(v,f,i));PUSH(local(v,f,i+1));break;
    case 30:case 31:case 32:case 33:i=op-30;PUSH(local(v,f,i));PUSH(local(v,f,i+1));break;
    case 38:case 39:case 40:case 41:i=op-38;PUSH(local(v,f,i));PUSH(local(v,f,i+1));break;
    case 55:case 57:i=BYTE();b=POP();a=POP();setlocal(v,f,i,a);setlocal(v,f,i+1,b);break;
    case 63:case 64:case 65:case 66:i=op-63;b=POP();a=POP();setlocal(v,f,i,a);setlocal(v,f,i+1,b);break;
    case 71:case 72:case 73:case 74:i=op-71;b=POP();a=POP();setlocal(v,f,i,a);setlocal(v,f,i+1,b);break;
    case 18:case 19:{CP*p;i=op==18?BYTE():WORD();p=cp(v,c,i,0);if(!p)break;if(CP_TAG(p)==3||CP_TAG(p)==4)PUSH(CP_VALUE(p));else if(CP_TAG(p)==8){CP*t=cp(v,c,CP_A(p),1);if(t){uint32_t slot=CP_TEXT(p);int32_t ref=slot?(int32_t)(REF+slot):0;if(!ref){ref=string(v,c->bytes+CP_TEXT(t),CP_A(t));if(ref)CP_SET_TEXT(p,(uint32_t)ref-REF);}PUSH(ref);}}else err(v,"ldc type unsupported");break;}
    case 21:case 23:case 25:PUSH(local(v,f,BYTE()));break;
    case 34:case 35:case 36:case 37:PUSH(local(v,f,op-34));break;
    case 56:i=BYTE();a=POP();setlocal(v,f,i,a);break;
    case 67:case 68:case 69:case 70:a=POP();setlocal(v,f,op-67,a);break;
    case 48:{Object*o;b=POP();a=POP();o=obj(v,a);if(!o)break;if(o->kind!=16||b<0||(uint32_t)b>=o->len){err(v,"Float array index/type invalid");break;}PUSH(array_get(o,(uint32_t)b));break;}
    case 81:{Object*o;x=POP();b=POP();a=POP();o=obj(v,a);if(!o)break;if(o->kind!=16||b<0||(uint32_t)b>=o->len){err(v,"Float array index/type invalid");break;}array_put(o,(uint32_t)b,x);break;}
    case 26:case 27:case 28:case 29:PUSH(local(v,f,op-26));break;
    case 42:case 43:case 44:case 45:PUSH(local(v,f,op-42));break;
    case 54:case 58:i=BYTE();a=POP();setlocal(v,f,i,a);break;
    case 59:case 60:case 61:case 62:a=POP();setlocal(v,f,op-59,a);break;
    case 75:case 76:case 77:case 78:a=POP();setlocal(v,f,op-75,a);break;
    case 46:case 50:case 51:case 52:case 53:{Object*o;b=POP();a=POP();o=obj(v,a);if(!o)break;if(b<0||(uint32_t)b>=o->len||o->kind<1||o->kind==2){err(v,"Array index/type invalid");break;}x=array_get(o,(uint32_t)b);PUSH(x);break;}
    case 47:{Object*o;b=POP();a=POP();o=obj(v,a);if(!o)break;if(b<0||(uint32_t)b>=o->len||o->kind!=21){err(v,"Long array index/type invalid");break;}pushwide(v,f,((uint64_t*)o->data)[b]);break;}
    case 79:case 83:case 84:case 85:case 86:{Object*o;x=POP();b=POP();a=POP();o=obj(v,a);if(!o)break;if(b<0||(uint32_t)b>=o->len||o->kind<1||o->kind==2){err(v,"Array index/type invalid");break;}array_put(o,(uint32_t)b,x);break;}
    case 80:{Object*o;uint64_t z=popwide(v,f);b=POP();a=POP();o=obj(v,a);if(!o)break;if(b<0||(uint32_t)b>=o->len||o->kind!=21){err(v,"Long array index/type invalid");break;}((uint64_t*)o->data)[b]=z;break;}
    case 87:POP();break;case 88:POP();POP();break;
    case 89:a=POP();PUSH(a);PUSH(a);break;
    case 90:a=POP();b=POP();PUSH(a);PUSH(b);PUSH(a);break;
    case 91:{int32_t z;a=POP();b=POP();z=POP();PUSH(a);PUSH(z);PUSH(b);PUSH(a);break;}
    case 92:a=POP();b=POP();PUSH(b);PUSH(a);PUSH(b);PUSH(a);break;
    case 93:{int32_t z;a=POP();b=POP();z=POP();PUSH(b);PUSH(a);PUSH(z);PUSH(b);PUSH(a);break;}
    case 95:a=POP();b=POP();PUSH(a);PUSH(b);break;
    case 96:b=POP();a=POP();PUSH((uint32_t)a+(uint32_t)b);break;
    case 98:case 102:case 106:case 110:{float y=float_bits(POP()),z=float_bits(POP()),r=op==98?z+y:op==102?z-y:op==106?z*y:z/y;PUSH(bits_float(r));break;}
    case 118:PUSH(bits_float(-float_bits(POP())));break;
    case 134:PUSH(bits_float((float)POP()));break;
    case 137:PUSH(bits_float((float)(int64_t)popwide(v,f)));break;
    case 139:{float z=float_bits(POP());PUSH(z!=z?0:z>=2147483648.0f?INT32_MAX:z<=-2147483648.0f?INT32_MIN:(int32_t)z);break;}
    case 140:{float z=float_bits(POP());pushwide(v,f,(uint64_t)(z!=z?0:z>=9223372036854775808.0f?INT64_MAX:z<=-9223372036854775808.0f?INT64_MIN:(int64_t)z));break;}
    case 141:pushdouble(v,f,(double)float_bits(POP()));break;
    case 144:PUSH(bits_float((float)popdouble(v,f)));break;
    case 149:case 150:{float y=float_bits(POP()),z=float_bits(POP());PUSH(z!=z||y!=y?(op==149?-1:1):z<y?-1:z>y?1:0);break;}
    case 151:case 152:{double y=popdouble(v,f),z=popdouble(v,f);PUSH(z!=z||y!=y?(op==151?-1:1):z<y?-1:z>y?1:0);break;}
    case 99:case 103:case 107:case 111:{double y=popdouble(v,f),z=popdouble(v,f),r=0.0;if(op==99)r=z+y;else if(op==103)r=z-y;else if(op==107)r=z*y;else r=z/y;pushdouble(v,f,r);break;}
    case 97:case 101:case 105:case 109:case 113:case 127:case 129:case 131:{uint64_t y=popwide(v,f),z=popwide(v,f),r=0;if(op==97)r=z+y;else if(op==101)r=z-y;else if(op==105)r=z*y;else if(op==127)r=z&y;else if(op==129)r=z|y;else if(op==131)r=z^y;else if(!y){err(v,"Long divide by zero");break;}else if(z==0x8000000000000000ull&&y==~0ull)r=op==109?z:0;else r=op==109?(uint64_t)((int64_t)z/(int64_t)y):(uint64_t)((int64_t)z%(int64_t)y);pushwide(v,f,r);break;}
    case 117:pushwide(v,f,0ull-popwide(v,f));break;
    case 119:pushdouble(v,f,-popdouble(v,f));break;
    case 121:case 123:case 125:{unsigned shift=(unsigned)POP()&63;uint64_t z=popwide(v,f),r;if(op==121)r=z<<shift;else if(op==125)r=z>>shift;else r=shift?(z>>shift)|((int64_t)z<0?(~0ull<<(64-shift)):0):z;pushwide(v,f,r);break;}
    case 133:a=POP();pushwide(v,f,(uint64_t)(int64_t)a);break;
    case 135:a=POP();pushdouble(v,f,(double)a);break;
    case 136:{uint64_t z=popwide(v,f);PUSH((uint32_t)z);break;}
    case 142:{double d=popdouble(v,f);if(d!=d)PUSH(0);else if(d>=2147483647.0)PUSH(INT32_MAX);else if(d<=-2147483648.0)PUSH(INT32_MIN);else PUSH((int32_t)d);break;}
    case 148:{int64_t y=(int64_t)popwide(v,f),z=(int64_t)popwide(v,f);PUSH(z<y?-1:z>y?1:0);break;}
    case 100:b=POP();a=POP();PUSH((uint32_t)a-(uint32_t)b);break;
    case 104:b=POP();a=POP();PUSH((uint32_t)a*(uint32_t)b);break;
    case 108:case 112:b=POP();a=POP();if(!b){err(v,"ArithmeticException: divide by zero");break;}if(a==INT32_MIN&&b==-1)PUSH(op==108?INT32_MIN:0);else PUSH(op==108?a/b:a%b);break;
    case 116:a=POP();PUSH(0u-(uint32_t)a);break;
    case 120:b=POP();a=POP();PUSH((uint32_t)a<<(b&31));break;
    case 122:b=POP();a=POP();i=(unsigned)b&31;PUSH(i?((uint32_t)a>>i)|(a<0?(~0u<<(32-i)):0):(uint32_t)a);break;
    case 124:b=POP();a=POP();PUSH((uint32_t)a>>(b&31));break;
    case 126:b=POP();a=POP();PUSH(a&b);break;case 128:b=POP();a=POP();PUSH(a|b);break;case 130:b=POP();a=POP();PUSH(a^b);break;
    case 132:i=BYTE();b=(int8_t)BYTE();a=local(v,f,i);setlocal(v,f,i,(int32_t)((uint32_t)a+(uint32_t)b));break;
    case 145:a=POP();PUSH((int8_t)a);break;case 146:a=POP();PUSH((uint16_t)a);break;case 147:a=POP();PUSH((int16_t)a);break;
    case 153:case 154:case 155:case 156:case 157:case 158:b=(int16_t)WORD();a=POP();if((op==153&&a==0)||(op==154&&a!=0)||(op==155&&a<0)||(op==156&&a>=0)||(op==157&&a>0)||(op==158&&a<=0))branch(v,f,m,pc0,b);break;
    case 159:case 160:case 161:case 162:case 163:case 164:case 165:case 166:{int32_t off=(int16_t)WORD();b=POP();a=POP();if(((op==159||op==165)&&a==b)||((op==160||op==166)&&a!=b)||(op==161&&a<b)||(op==162&&a>=b)||(op==163&&a>b)||(op==164&&a<=b))branch(v,f,m,pc0,off);break;}
    case 167:b=(int16_t)WORD();branch(v,f,m,pc0,b);break;
    case 170:case 171:{int32_t def,selected;uint32_t count;a=POP();while(f->pc&3)BYTE();def=(int32_t)dword(v,f,m);selected=def;
        if(op==170){int32_t low=(int32_t)dword(v,f,m),high=(int32_t)dword(v,f,m);int64_t n=(int64_t)high-low+1;if(n<0||n>16384){err(v,"Invalid tableswitch");break;}count=(uint32_t)n;for(i=0;i<count&&!v->error[0];i++){int32_t off=(int32_t)dword(v,f,m);if((int64_t)a-low==i)selected=off;}}
        else{count=dword(v,f,m);if(count>8192){err(v,"Invalid lookupswitch");break;}for(i=0;i<count&&!v->error[0];i++){int32_t key=(int32_t)dword(v,f,m),off=(int32_t)dword(v,f,m);if(key==a)selected=off;}}
        branch(v,f,m,pc0,selected);break;}
    case 196:{unsigned sub=BYTE();i=WORD();if(sub==21||sub==23||sub==25)PUSH(local(v,f,i));else if(sub==22||sub==24){a=local(v,f,i);b=local(v,f,i+1);PUSH(a);PUSH(b);}else if(sub==55||sub==57){b=POP();a=POP();setlocal(v,f,i,a);setlocal(v,f,i+1,b);}else if(sub==54||sub==56||sub==58){a=POP();setlocal(v,f,i,a);}else if(sub==132){b=(int16_t)WORD();setlocal(v,f,i,(int32_t)((uint32_t)local(v,f,i)+(uint32_t)b));}else err(v,"Unsupported wide instruction");break;}
    case 200:branch(v,f,m,pc0,(int32_t)dword(v,f,m));break;
    case 173:case 175:{uint64_t z=popwide(v,f);returnframe(v,0,0);if(v->depth)pushwide(v,v->frames+v->depth-1,z);break;}
    case 172:case 174:case 176:a=POP();returnframe(v,1,a);break;case 177:returnframe(v,0,0);break;
    case 178:case 179:case 180:case 181:{Field*field=NULL;int cur,guard=0;i=WORD();if(!member(v,c,i,9,&owner,name,desc))break;cur=owner;while(cur&&guard++<CLASSES){Class*k=v->classes[cur];unsigned z;for(z=0;z<k->nf;z++)if(eq(v,k,k->fields[z].name,name)&&eq(v,k,k->fields[z].desc,desc)){field=k->fields+z;break;}if(field)break;cur=k->parent;}
        if(!field&&op==178&&v->classes[owner]->builtin==B_SYSTEM&&!strcmp(name,"out")&&!strcmp(desc,"Ljava/io/PrintStream;")){if(!v->system_out){int ps=loadclass(v,"java/io/PrintStream");if(!ps)break;v->system_out=allocobj(v,ps,0,v->classes[ps]->instance_fields);if(!v->system_out)break;}PUSH(v->system_out);break;}
        if(!field&&op==178&&v->classes[owner]->builtin==B_CALENDAR){
            int cv=0,cf=0;
            if(!strcmp(name,"YEAR")){cv=1;cf=1;}else if(!strcmp(name,"MONTH")){cv=2;cf=1;}else if(!strcmp(name,"DATE")||!strcmp(name,"DAY_OF_MONTH")){cv=5;cf=1;}else if(!strcmp(name,"DAY_OF_WEEK")){cv=7;cf=1;}else if(!strcmp(name,"HOUR")){cv=10;cf=1;}else if(!strcmp(name,"HOUR_OF_DAY")){cv=11;cf=1;}else if(!strcmp(name,"MINUTE")){cv=12;cf=1;}else if(!strcmp(name,"SECOND")){cv=13;cf=1;}else if(!strcmp(name,"MILLISECOND")){cv=14;cf=1;}else if(!strcmp(name,"AM_PM")){cv=9;cf=1;}else if(!strcmp(name,"AM")){cv=0;cf=1;}else if(!strcmp(name,"PM")){cv=1;cf=1;}else if(!strcmp(name,"SUNDAY")){cv=1;cf=1;}else if(!strcmp(name,"MONDAY")){cv=2;cf=1;}else if(!strcmp(name,"TUESDAY")){cv=3;cf=1;}else if(!strcmp(name,"WEDNESDAY")){cv=4;cf=1;}else if(!strcmp(name,"THURSDAY")){cv=5;cf=1;}else if(!strcmp(name,"FRIDAY")){cv=6;cf=1;}else if(!strcmp(name,"SATURDAY")){cv=7;cf=1;}else if(!strcmp(name,"JANUARY")){cv=0;cf=1;}else if(!strcmp(name,"FEBRUARY")){cv=1;cf=1;}else if(!strcmp(name,"MARCH")){cv=2;cf=1;}else if(!strcmp(name,"APRIL")){cv=3;cf=1;}else if(!strcmp(name,"MAY")){cv=4;cf=1;}else if(!strcmp(name,"JUNE")){cv=5;cf=1;}else if(!strcmp(name,"JULY")){cv=6;cf=1;}else if(!strcmp(name,"AUGUST")){cv=7;cf=1;}else if(!strcmp(name,"SEPTEMBER")){cv=8;cf=1;}else if(!strcmp(name,"OCTOBER")){cv=9;cf=1;}else if(!strcmp(name,"NOVEMBER")){cv=10;cf=1;}else if(!strcmp(name,"DECEMBER")){cv=11;cf=1;}else if(!strcmp(name,"ERA")){cv=0;cf=1;}else if(!strcmp(name,"FIELD_COUNT")){cv=17;cf=1;}
            if(cf){PUSH(cv);break;}
        }
        if(!field&&op==178&&v->classes[owner]->builtin==B_COMMAND){
            int cv=0,cf=0;
            if(!strcmp(name,"SCREEN")){cv=1;cf=1;}else if(!strcmp(name,"BACK")){cv=2;cf=1;}else if(!strcmp(name,"CANCEL")){cv=3;cf=1;}else if(!strcmp(name,"OK")){cv=4;cf=1;}else if(!strcmp(name,"HELP")){cv=5;cf=1;}else if(!strcmp(name,"STOP")){cv=6;cf=1;}else if(!strcmp(name,"EXIT")){cv=7;cf=1;}else if(!strcmp(name,"ITEM")){cv=8;cf=1;}
            if(cf){PUSH(cv);break;}
        }
        if(!field){snprintf(v->error,sizeof(v->error),"Unsupported field: %.36s.%.32s",v->classes[owner]->name,name);break;}
        if((!!(field->flags&STATIC))!=(op<=179)){err(v,"Field static/instance mismatch");break;}
        if(op<=179){if(!initialize(v,cur)){f->pc=pc0;break;}if(op==178){if(field->wide)PUSH(field->high);PUSH(field->value);}else{field->value=POP();if(field->wide)field->high=POP();}}
        else {Object*o;int32_t high=0;x=op==181?POP():0;if(op==181&&field->wide)high=POP();a=POP();o=obj(v,a);if(o){if(o->kind||(uint32_t)field->offset+(field->wide?1u:0u)>=o->len){err(v,"Invalid instance field");break;}if(op==180){PUSH(o->data[field->offset]);if(field->wide)PUSH(o->data[field->offset+1]);}else{o->data[field->offset]=field->wide?high:x;if(field->wide)o->data[field->offset+1]=x;}}}break;}
    case 182:case 183:case 184:case 185:{int target;int32_t args[33]={0},out;const char*own;unsigned cpindex=WORD();if(op==185){BYTE();BYTE();}if(!member(v,c,cpindex,op==185?11:10,&owner,name,desc)||!signature(v,desc,&argc,&ret))break;
        if(op==184&&!initialize(v,owner)){f->pc=pc0;break;}if(op!=184)argc++;
        if(f->sp-f->base-m->locals<argc){err(v,"Invoke operand underflow");break;}
        for(i=0;i<(unsigned)argc;i++)args[i]=v->slots[f->sp-argc+i];
        target=owner;
        if(op!=184){if(!args[0]){throw_exception(v,"java/lang/NullPointerException",NULL);break;}Object*o=obj(v,args[0]);if(!o)break;if(op!=183)target=o->cls;}
        mi=method(v,&target,name,desc);
        if(mi>=0){Method*called=v->classes[target]->methods+mi;if((!!(called->flags&STATIC))!=(op==184)){err(v,"Invoke static/instance mismatch");break;}f->sp-=argc;pushframe(v,target,mi,args,argc,0);}
        else {int result;if(op==182||op==185){Object*receiver=obj(v,args[0]);if(receiver&&receiver->cls)owner=receiver->cls;}own=v->classes[owner]->name;result=native(v,v->classes[owner]->builtin,name,desc,args,&out);if(!result){int cur=v->classes[owner]->parent,guard=0;while(cur&&guard++<CLASSES&&!result){result=native(v,v->classes[cur]->builtin,name,desc,args,&out);cur=v->classes[cur]->parent;}}
            if(!result)snprintf(v->error,sizeof(v->error),"Unsupported API: %.32s.%.20s%.20s",own,name,desc);
            if(result>0){f->sp-=argc;if(ret==2)PUSH(v->native_high);if(ret)PUSH(out);}}
        break;}
    case 187:i=WORD();if(!cname(v,c,i,name))break;owner=loadclass(v,name);if(!owner)break;if(!initialize(v,owner)){f->pc=pc0;break;}PUSH(allocobj(v,owner,0,v->classes[owner]->instance_fields));break;
    case 188:case 189:{char descriptor[128];i=op==188?BYTE():WORD();if(op==188){const char*types="ZCFDBSIJ";if(i<4||i>11){err(v,"Array primitive unsupported");break;}descriptor[0]='[';descriptor[1]=types[i-4];descriptor[2]=0;}else{if(!cname(v,c,i,name))break;if(strlen(name)>124){err(v,"Array name too long");break;}if(name[0]=='[')snprintf(descriptor,sizeof(descriptor),"[%s",name);else snprintf(descriptor,sizeof(descriptor),"[L%s;",name);}owner=loadclass(v,descriptor);if(!owner)break;a=POP();if(a<0){throw_exception(v,"java/lang/NegativeArraySizeException",NULL);break;}PUSH(allocobj(v,owner,op==189?1:(int)i+10,(uint32_t)a));break;}
    case 197:{int32_t sizes[8];i=WORD();unsigned dims=BYTE();if(!dims||dims>8||!cname(v,c,i,name)){err(v,"Invalid multianewarray dimensions");break;}unsigned count=0;while(name[count]=='[')count++;if(dims>count){err(v,"Invalid multianewarray descriptor");break;}for(int d=(int)dims-1;d>=0;d--)sizes[d]=POP();int valid=1;for(unsigned d=0;d<dims;d++)if(sizes[d]<0)valid=0;if(!valid){throw_exception(v,"java/lang/NegativeArraySizeException",NULL);break;}PUSH(multi_array(v,name,sizes,0,(int)dims));break;}
    case 190:{Object*o=obj(v,POP());if(o){if(o->kind<1||o->kind==2)err(v,"arraylength on non-array");else PUSH(o->len);}break;}
    case 191:a=POP();if(a)v->thrown=a;else throw_exception(v,"java/lang/NullPointerException",NULL);break;
    case 194:a=POP();i=(unsigned)monitor_enter(v,a);if(i==0){PUSH(a);f->pc=pc0;v->yield=1;}break;
    case 195:a=POP();monitor_exit(v,a);break;
    case 192:case 193:{int cur,ok=0,guard=0;Object*o;i=WORD();if(!cname(v,c,i,name))break;a=POP();if(!a){PUSH(0);break;}o=obj(v,a);if(!o)break;owner=0;for(int ci=1;ci<=v->nc;ci++)if(!strcmp(v->classes[ci]->name,name)){owner=ci;break;}cur=o->cls;if(!cur&&name[0]=='['&&o->kind==array_kind(name))ok=1;if(owner)while(cur&&guard++<CLASSES){if(cur==owner){ok=1;break;}cur=v->classes[cur]->parent;}if(op==193)PUSH(ok);else if(ok)PUSH(a);else throw_exception(v,"java/lang/ClassCastException",NULL);break;}
    case 198:case 199:b=(int16_t)WORD();a=POP();if((op==198&&!a)||(op==199&&a))branch(v,f,m,pc0,b);break;
    default:snprintf(v->error,sizeof(v->error),"Unsupported opcode 0x%02X at %.50s:%lu",op,c->name,(unsigned long)pc0);break;
    }
#undef POP
#undef PUSH
#undef BYTE
#undef WORD
}
static int call_event(PJ*v,int32_t ref,const char*name,const char*desc,int32_t value){Object*o=obj(v,ref);int cls,mi;int32_t args[2]={ref,value};if(!o)return 0;cls=o->cls;mi=method(v,&cls,name,desc);if(mi<0){if(!strcmp(name,"paint")){int cur=o->cls,guard=0;while(cur&&guard++<CLASSES){if(v->classes[cur]->builtin==B_GAMECANVAS){int32_t out=0;native(v,B_GAMECANVAS,"paint","(Ljavax/microedition/lcdui/Graphics;)V",args,&out);v->yield=1;break;}cur=v->classes[cur]->parent;}}return 0;}return pushframe(v,cls,mi,args,strcmp(desc,"()V")?2:1,!strcmp(name,"paint")?-1:0);}
static int launch(PJ*v){int cls,mi;int32_t arg;cls=loadclass(v,v->main);if(!cls)return 0;if(!initialize(v,cls))return !v->error[0];if(!v->midlet){int ancestor=cls,guard=0,valid=0;while(ancestor&&guard++<CLASSES){if(v->classes[ancestor]->builtin==B_MIDLET){valid=1;break;}ancestor=v->classes[ancestor]->parent;}if(!valid)return err(v,"Main class does not extend MIDlet");v->midlet=allocobj(v,cls,0,v->classes[cls]->instance_fields);arg=v->midlet;mi=method(v,&cls,"<init>","()V");if(mi<0)return err(v,"MIDlet constructor missing");v->start_pending=1;return pushframe(v,cls,mi,&arg,1,0);}if(v->start_pending){v->start_pending=0;return call_event(v,v->midlet,"startApp","()V",0);}return 1;}
static int schedule(PJ*v){
    if(!v->midlet||v->start_pending){launch(v);return v->depth>0;}
    if(v->kr!=v->kw&&v->canvas){unsigned idx=v->kr++&15;return call_event(v,v->canvas,v->keys[idx].release?"keyReleased":"keyPressed","(I)V",v->keys[idx].key);}
    if(v->serially_n){for(int i=0;i<v->serially_n;i++){int32_t r=v->serially[i];Object*ro=r?obj(v,r):NULL;if(ro){int cls=ro->cls;int mi=method(v,&cls,"run","()V");if(mi>=0)pushframe(v,cls,mi,NULL,0,0);}}v->serially_n=0;if(v->depth)return 1;}
    /* drain due timers */{
        uint32_t now=v->p.clock_ms();int fired=-1;
        for(int i=0;i<v->timer_n;i++){if(v->timers[i].cancelled)continue;if((int32_t)(now-v->timers[i].due)>=0){fired=i;break;}}
        if(fired>=0){int32_t task=v->timers[fired].task; if(v->timers[fired].periodic){v->timers[fired].due+=v->timers[fired].period;}else{v->timers[fired].cancelled=1;} if(task){Object*to=obj(v,task);if(to){int cls=to->cls;int mi=method(v,&cls,"run","()V");if(mi>=0)pushframe(v,cls,mi,NULL,0,0);}} if(v->depth)return 1;}
    }
    if(v->dirty&&v->canvas){v->dirty=0;v->tx=v->ty=0;v->cx=v->cy=0;v->cw=240;v->ch=320;v->color=0;return call_event(v,v->canvas,"paint","(Ljavax/microedition/lcdui/Graphics;)V",v->graphics);}
    return 0;
}
static int choose_context(PJ*v){
    switch_context(v,0);if(v->depth&&!v->contexts[0]->monitor_wait)return 1;
    uint32_t now=v->p.clock_ms();int worker_busy=0;for(int t=1;t<4;t++){Context*ctx=v->contexts[t];if(ctx&&ctx->runnable&&ctx->depth>1&&(int32_t)(now-ctx->wake)>=0){worker_busy=1;break;}}int defer_paint=worker_busy&&v->dirty&&v->canvas&&pj_memory()+4096u>PJ_MEMORY_LIMIT;
    if(!v->contexts[0]->monitor_wait&&(!v->midlet||v->start_pending||v->kr!=v->kw||(!v->prefer_worker&&v->dirty&&v->canvas&&!defer_paint))){if(schedule(v)){v->prefer_worker=1;return 1;}if(v->error[0])return 0;}
    int start=v->worker_cursor;if(start<1||start>3)start=1;for(int pass=0;pass<3;pass++){int t=1+((start-1+pass)%3);Context*ctx=v->contexts[t];if(!ctx||!ctx->runnable||ctx->monitor_wait||(int32_t)(v->p.clock_ms()-ctx->wake)<0)continue;switch_context(v,t);v->prefer_worker=0;v->worker_cursor=t==3?1:t+1;if(!ctx->started){ctx->started=1;if(!call_event(v,ctx->runnable,"run","()V",0)){err(v,"Runnable.run method missing");return 0;}}if(v->depth)return 1;ctx->runnable=0;ctx->thread=0;switch_context(v,0);pj_free(ctx->frames);pj_free(ctx->slots);pj_free(ctx);v->contexts[t]=NULL;}
    switch_context(v,0);if(v->contexts[0]->monitor_wait)return 0;if(v->dirty&&v->canvas){v->prefer_worker=1;return schedule(v);}return 0;
}
PJ *pj_create(PJPlatform p){PJ*v=pj_alloc(sizeof(*v));int c;if(!v)return NULL;memset(v,0,sizeof(*v));v->worker_cursor=1;v->contexts[0]=context_new(64);if(!v->contexts[0]){pj_free(v);return NULL;}switch_context(v,0);v->jar.file=-1;v->p=p;v->random=1;v->cw=240;v->ch=320;c=loadclass(v,"javax/microedition/lcdui/Graphics");v->graphics=allocobj(v,c,0,8);c=loadclass(v,"javax/microedition/lcdui/Display");v->display=allocobj(v,c,0,8);return v;}
void pj_destroy(PJ*v){int i;if(!v)return;platform_tone_stop();for(unsigned p=0;p<OBJECT_PAGES;p++)if(v->object_pages[p]){for(unsigned s=p?0:1;s<object_page_cap(v,p);s++){Object*o=v->object_pages[p]+s;pj_free(o->data);if(o->native&&o->cls&&v->classes[o->cls]->builtin==B_INPUT)jar_stream_close((PJZipStream*)o->native);else pj_free(o->native);}pj_free(v->object_pages[p]);}for(i=0;i<4;i++)if(v->contexts[i]){pj_free(v->contexts[i]->frames);pj_free(v->contexts[i]->slots);pj_free(v->contexts[i]);}for(i=1;i<=v->nc;i++)freeclass(v->classes[i]);jar_close(&v->jar);pj_free(v->jad);pj_free(v);}
static int manifest(PJ*v){unsigned char*d;size_t n;char midlet[256],*p,*q;d=jar_extract(&v->jar,"META-INF/MANIFEST.MF",&n,v->error);if(!d)return err(v,"Missing/invalid JAR manifest");
    if((!v->jad||jar_property(v->jad,v->jad_size,"MIDlet-1",midlet,sizeof(midlet))!=1)&&jar_property(d,n,"MIDlet-1",midlet,sizeof(midlet))!=1){pj_free(d);return err(v,"Missing / too long MIDlet-1");}
    if(!v->jad||jar_property(v->jad,v->jad_size,"MIDlet-Name",v->title,sizeof(v->title))!=1)jar_property(d,n,"MIDlet-Name",v->title,sizeof(v->title));p=strchr(midlet,',');q=p?strchr(p+1,','):NULL;if(!q){pj_free(d);return err(v,"Invalid MIDlet-1 property");}q++;while(*q==' ')q++;if(strlen(q)>=sizeof(v->main)||!*q){pj_free(d);return err(v,"Invalid MIDlet main class");}strcpy(v->main,q);p=v->main+strlen(v->main);while(p>v->main&&(p[-1]==' '||p[-1]=='\t'))*--p=0;for(p=v->main;*p;p++)if(*p=='.')*p='/';pj_free(d);return launch(v);
}
int pj_load_memory(PJ*v,const unsigned char*d,size_t n){unsigned char*copy;if(n>PJ_JAR_LIMIT)return err(v,"JAR exceeds configured archive limit");copy=pj_alloc(n);if(!copy)return err(v,"Memory limit: JAR");memcpy(copy,d,n);if(!jar_open(&v->jar,copy,n,v->error)){pj_free(copy);return 0;}return manifest(v);}
static int jad_local_url(char*url){
    size_t read=0,write=0;
    while(url[read]){unsigned c=(unsigned char)url[read++];if(c=='%'){unsigned value=0;for(int i=0;i<2;i++){unsigned h=(unsigned char)url[read];if(!h)return 0;read++;if(h>='0'&&h<='9')h-='0';else if(h>='a'&&h<='f')h=h-'a'+10;else if(h>='A'&&h<='F')h=h-'A'+10;else return 0;value=value*16+h;}c=value;}if(!c||c<32||c==':'||c=='?'||c=='#')return 0;url[write++]=(char)(c=='\\'?'/':c);}
    url[write]=0;if(!write||url[0]=='/')return 0;
    char*start=url;for(char*p=url;;p++)if(*p=='/'||!*p){size_t n=(size_t)(p-start);if(!n||(n==2&&start[0]=='.'&&start[1]=='.'))return 0;if(!*p)break;start=p+1;}
#ifdef MRE
    for(char*p=url;*p;p++)if(*p=='/')*p='\\';
#endif
    return 1;
}
int pj_load(PJ*v,const char*path){unsigned char*d;size_t n;char jarpath[260];size_t len=strlen(path);if(len>=sizeof(jarpath))return err(v,"Path too long");strcpy(jarpath,path);
    if(len>=4&&path[len-4]=='.'&&(path[len-3]=='j'||path[len-3]=='J')&&(path[len-2]=='a'||path[len-2]=='A')&&(path[len-1]=='d'||path[len-1]=='D')){char url[200],*slash,*back;d=platform_read(path,8192,&n);if(!d)return err(v,"Cannot read JAD");if(jar_property(d,n,"MIDlet-Jar-URL",url,sizeof(url))!=1){pj_free(d);return err(v,"JAD missing MIDlet-Jar-URL");}v->jad=d;v->jad_size=n;if(!jad_local_url(url))return err(v,"JAD URL must be local JAR filename or relative path");slash=strrchr(jarpath,'/');back=strrchr(jarpath,'\\');if(back&&(!slash||back>slash))slash=back;if(slash)slash[1]=0;else jarpath[0]=0;if(strlen(jarpath)+strlen(url)>=sizeof(jarpath))return err(v,"JAD path too long");strcat(jarpath,url);}
    if(!jar_open_file(&v->jar,jarpath,v->error))return 0;
    if(v->jad){char size[32];int present=jar_property(v->jad,v->jad_size,"MIDlet-Jar-Size",size,sizeof(size));if(present){uint32_t expected=0;if(present<0||!*size)return err(v,"Invalid MIDlet-Jar-Size");for(const char*p=size;*p;p++){if(*p<'0'||*p>'9'||expected>(PJ_JAR_LIMIT-(unsigned)(*p-'0'))/10u)return err(v,"Invalid MIDlet-Jar-Size");expected=expected*10u+(unsigned)(*p-'0');}if(expected!=v->jar.size)return err(v,"MIDlet-Jar-Size mismatch");}}
    return manifest(v);
}
int pj_step(PJ*v,unsigned budget,unsigned max_ms){uint32_t start=v->p.clock_ms();unsigned n;v->rendered=0;if(v->error[0]||v->paused||v->stopped)return 0;
    v->yield=0;if(!choose_context(v))return !v->error[0];
    for(n=0;n<budget&&!v->error[0]&&!v->stopped;n++){if(!v->depth&&!choose_context(v))break;if(v->depth)execute(v);if(v->thrown)propagate(v);if(v->yield||(!v->depth&&v->rendered))break;if((n&63)==63&&v->p.clock_ms()-start>=max_ms)break;}return !v->error[0];}
void pj_key(PJ*v,int key,int released){int action=game_action(key);if(action){uint32_t bit=1u<<action;if(released)v->key_held&=~bit;else{v->key_held|=bit;v->key_latched|=bit;}}if(action&&v->canvas){Object*canvas=obj(v,v->canvas);int cur=canvas?canvas->cls:0,guard=0;while(cur&&guard++<CLASSES){if(v->classes[cur]->builtin==B_GAMECANVAS&&canvas->data[1])return;cur=v->classes[cur]->parent;}}if(v->kw-v->kr<16){unsigned i=v->kw++&15;v->keys[i].key=key;v->keys[i].release=released;}}
void pj_repaint(PJ*v){v->dirty=1;}
void pj_pause(PJ*v,int paused){v->paused=paused;if(paused)platform_tone_stop();}
const char *pj_error(PJ*v){return v?v->error:"Cannot allocate VM";}
const char *pj_title(PJ*v){return v?v->title:"PocketJava";}
uint64_t pj_instructions(PJ*v){return v?v->instructions:0;}
int pj_busy(PJ*v){return v&&(v->current==0?v->depth:v->contexts[0]->depth)>0;}
int pj_rendered(PJ*v){return v&&v->rendered;}
int pj_finished(PJ*v){return v&&v->stopped;}
#ifndef MRE
void pj_debug_dump(PJ*v){if(!v)return;for(int t=0;t<4;t++)if(v->contexts[t]){Context*ctx=v->contexts[t];int depth=t==v->current?v->depth:ctx->depth;for(int i=0;i<depth;i++){Frame*f=ctx->frames+i;Class*c=v->classes[f->cls];Method*m=c->methods+f->method;char name[128];utf(v,c,m->name,name,sizeof(name));printf("FRAME task=%d %s.%s pc=%u opcodepc=%u\n",t,c->name,name,f->pc,f->lastpc);}}for(unsigned p=0;p<OBJECT_PAGES;p++)if(v->object_pages[p])for(unsigned s=p?0:1;s<object_page_cap(v,p);s++){Object*o=v->object_pages[p]+s;if(o->data){uint32_t logical_len=o->kind==2?(o->len&STRING_STORAGE_LEN_MASK):o->len;uint32_t native_bytes=0;if(o->native&&o->cls){int b=v->classes[o->cls]->builtin;if(b==B_VECTOR||b==B_HASHTABLE||b==B_BYTEOUTPUT)native_bytes=(uint32_t)native_buffer_size(o);else if(b==B_STRING)native_bytes=sizeof(StringCursor);}if((size_t)logical_len*element_size(o->kind)+native_bytes>16000)printf("OBJECT id=%u class=%s kind=%d len=%u native=%u\n",(p<<OBJECT_PAGE_SHIFT)+s,o->cls?v->classes[o->cls]->name:"array",o->kind,logical_len,(unsigned)native_bytes);}}}
#endif
