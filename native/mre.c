#include "pocket.h"
#include "vmsys.h"
#include "vmio.h"
#include "vmgraph.h"
#include "vmtimer.h"
#include "vmchset.h"
#include "vmmm.h"
#include "demo_jar.h"
#include <string.h>
#include <stdio.h>
static PJ *vm;
static VMINT layer=-1,timer=-1;
static VMUINT8 *buffer;
static int mode,selected,count,foreground;
static char paths[64][260],root[24],status[160];
static uint32_t lastpaint;
/* One bounded MIDI buffer; stop the previous voice before reusing its bytes. */
static unsigned char tone_midi[48];
static VMINT tone_handle=-1;
static void tone_event(VMINT handle,VMINT event){if(handle==tone_handle&&event==VM_STATE_MIDI_STOP)tone_handle=-1;}
void platform_tone_stop(void){if(tone_handle>=0){vm_midi_stop(tone_handle);tone_handle=-1;}}
int platform_tone(int note,int duration,int volume){
    static const unsigned char header[]={ 'M','T','h','d',0,0,0,6,0,0,0,1,1,244,'M','T','r','k',0,0,0,0,0,255,81,3,7,161,32,0,144 };
    unsigned char delta[4];unsigned n=0;uint32_t ticks=(uint32_t)duration;size_t pos=sizeof(header);
    platform_tone_stop();memcpy(tone_midi,header,sizeof(header));tone_midi[pos++]=(unsigned char)note;tone_midi[pos++]=(unsigned char)(volume*127/100);
    do{delta[n++]=(unsigned char)(ticks&127);ticks>>=7;}while(ticks&&n<4);
    while(n){n--;tone_midi[pos++]=(unsigned char)(delta[n]|(n?128:0));}
    tone_midi[pos++]=128;tone_midi[pos++]=(unsigned char)note;tone_midi[pos++]=0;tone_midi[pos++]=0;tone_midi[pos++]=255;tone_midi[pos++]=47;tone_midi[pos++]=0;
    tone_midi[21]=(unsigned char)(pos-22);vm_midi_set_type(VM_FORMAT_MIDI);
    tone_handle=vm_midi_play_by_bytes(tone_midi,(VMINT)pos,1,tone_event);return tone_handle>=0;
}
static uint16_t rgb565(uint32_t c){return (uint16_t)(((c>>19)&31)<<11|((c>>10)&63)<<5|((c>>3)&31));}
static void wide(VMWCHAR*out,const char*s,size_t cap){size_t n=0;while(*s&&n+1<cap){unsigned c=(unsigned char)*s++;if(c>=192&&c<224&&((unsigned char)s[0]&192)==128)c=(c&31)<<6|((unsigned char)*s++&63);else if(c>=224&&c<240&&s[0]&&s[1]&&((unsigned char)s[0]&192)==128&&((unsigned char)s[1]&192)==128){c=(c&15)<<12|((unsigned char)s[0]&63)<<6|((unsigned char)s[1]&63);s+=2;}out[n++]=(VMWCHAR)c;}out[n]=0;}
static void drawrect(int x,int y,int w,int h,uint32_t color){if(!buffer||w<=0||h<=0)return;int right=x+w,bottom=y+h;if(x<0)x=0;if(y<0)y=0;if(right>240)right=240;if(bottom>320)bottom=320;uint16_t c=rgb565(color);for(int yy=y;yy<bottom;yy++){uint16_t*row=(uint16_t*)buffer+yy*240;for(int xx=x;xx<right;xx++)row[xx]=c;}}
static void drawtext(int x,int y,const char*s,uint32_t color){VMWCHAR text[128];size_t n=0;while(*s&&n<127){unsigned char c=(unsigned char)*s++;unsigned code=c;if(c>=192&&c<224&&((unsigned char)s[0]&192)==128){code=(c&31)<<6|((unsigned char)*s++&63);}else if(c>=224&&c<240&&s[0]&&s[1]&&((unsigned char)s[0]&192)==128&&((unsigned char)s[1]&192)==128){code=(c&15)<<12|((unsigned char)s[0]&63)<<6|((unsigned char)s[1]&63);s+=2;}text[n++]=(VMWCHAR)code;}text[n]=0;if(buffer)vm_graphic_textout(buffer,x,y,text,(VMINT)n,rgb565(color));}
static uint32_t clockms(void){return (uint32_t)vm_get_tick_count();}
/* MRE may return fewer bytes than requested, so read in bounded chunks. */
static int read_exact(VMFILE f,void*out,size_t n){unsigned char*p=(unsigned char*)out;size_t total=0;while(total<n){VMUINT want=(VMUINT)(n-total>1024u?1024u:n-total),got=0;vm_file_read(f,p+total,want,&got);if(!got||got>want)return 0;total+=got;}return 1;}
intptr_t platform_open(const char*path,size_t*size){VMWCHAR wp[260];VMUINT n;VMFILE f;wide(wp,path,260);f=vm_file_open(wp,MODE_READ,1);if(f<0)return -1;if(vm_file_getfilesize(f,&n)!=0){vm_file_close(f);return -1;}*size=n;return f;}
/* vm_file_seek returns 0 on success and only accepts BASE_BEGIN/CURR/END. */
int platform_read_at(intptr_t file,size_t offset,void*out,size_t n){if(offset>0x7fffffff||vm_file_seek((VMFILE)file,(VMINT)offset,BASE_BEGIN)!=0)return 0;return read_exact((VMFILE)file,out,n);}
void platform_close(intptr_t file){if(file!=-1)vm_file_close((VMFILE)file);}
unsigned char *platform_read(const char*path,size_t limit,size_t*n){VMWCHAR wp[260];VMFILE f;VMUINT size=0;unsigned char*d;*n=0;wide(wp,path,260);f=vm_file_open(wp,MODE_READ,1);if(f<0)return NULL;if(vm_file_getfilesize(f,&size)!=0||size>limit){vm_file_close(f);return NULL;}d=pj_alloc(size);if(!d){vm_file_close(f);return NULL;}if(!read_exact(f,d,size)){pj_free(d);vm_file_close(f);return NULL;}vm_file_close(f);*n=size;return d;}
static int extension(const char*s,const char*ext){size_t n=strlen(s);if(n<4)return 0;s+=n-4;return s[0]=='.'&&(s[1]==ext[1]||s[1]==ext[1]-32)&&(s[2]==ext[2]||s[2]==ext[2]-32)&&(s[3]==ext[3]||s[3]==ext[3]-32);}
#include "scanner.inc"
static const char *basename_of(const char*path){const char*p=strrchr(path,'\\');return p?p+1:path;}
static const char*preview(char*out,const char*text,unsigned chars){size_t n=0;while(*text&&chars--){unsigned c=(unsigned char)*text;size_t bytes=c<128?1:c<224?2:3;for(size_t i=0;i<bytes&&*text;i++)out[n++]=*text++;}out[n]=0;return text;}
#include "ui.inc"
static void open_selected(void){int ok;ui_options=0;if(scan_active)scan_limited=1;scan_stop();pj_destroy(vm);vm=pj_create((PJPlatform){drawrect,drawtext,clockms});status[0]=0;if(!vm){strcpy(status,"Insufficient RAM");menu();return;}ui_header("POCKETJAVA");drawtext(16,64,"Loading Java bytecode...",UI_INK);vm_graphic_flush_layer(&layer,1);if(!selected)ok=pj_load_memory(vm,demo_jar,sizeof(demo_jar));else{ok=pj_load(vm,paths[selected-1]);}if(!ok){strncpy(status,pj_error(vm),159);status[159]=0;pj_destroy(vm);vm=NULL;mode=0;menu();}else{mode=1;lastpaint=clockms();}}
static void tick(VMINT id){(void)id;if(!foreground)return;if(mode==0&&scan_active){scan_step(16);menu();return;}if(mode!=1||!vm)return;if(!pj_busy(vm)&&clockms()-lastpaint>=PJ_FRAME_MS){pj_repaint(vm);lastpaint=clockms();}pj_step(vm,PJ_SLICE_OPCODES,PJ_SLICE_MS);if(pj_error(vm)[0]||pj_finished(vm)){strncpy(status,pj_error(vm),159);status[159]=0;pj_destroy(vm);vm=NULL;mode=0;menu();}else if(!pj_busy(vm)&&pj_rendered(vm)&&buffer)vm_graphic_flush_layer(&layer,1);}
static void key(VMINT event,VMINT code){if(!foreground)return;if(mode==1){if(event==VM_KEY_EVENT_DOWN&&code==VM_KEY_STAR){mode=2;pj_pause(vm,1);pause_menu();return;}pj_key(vm,code,event==VM_KEY_EVENT_UP);return;}if(event!=VM_KEY_EVENT_DOWN&&event!=VM_KEY_EVENT_REPEAT)return;
    if(mode==2){if(code==VM_KEY_OK||code==VM_KEY_NUM5||code==VM_KEY_STAR){mode=1;pj_pause(vm,0);pj_repaint(vm);}else if(code==VM_KEY_NUM0){pj_destroy(vm);vm=NULL;mode=0;menu();}return;}
    if(ui_options){
        if(code==VM_KEY_DOWN||code==VM_KEY_NUM8)ui_choice=(ui_choice+1)%4;
        else if(code==VM_KEY_UP||code==VM_KEY_NUM2)ui_choice=(ui_choice+3)%4;
        else if(code==VM_KEY_RIGHT_SOFTKEY||code==VM_KEY_BACK)ui_options=0;
        else if(code==VM_KEY_OK||code==VM_KEY_NUM5){ui_options=0;if(ui_choice==0&&ui_tab!=3&&ui_accept(selected)){open_selected();return;}if(ui_choice==1){scan_start();selected=0;status[0]=0;}if(ui_choice==2)ui_tab=3;}
        menu();return;
    }
    if(code==VM_KEY_LEFT||code==VM_KEY_NUM4||code==VM_KEY_RIGHT||code==VM_KEY_NUM6){ui_tab=(ui_tab+((code==VM_KEY_LEFT||code==VM_KEY_NUM4)?3:1))%4;if(ui_tab!=3&&!ui_accept(selected)){selected=0;ui_move(1);}status[0]=0;menu();}
    else if(code==VM_KEY_LEFT_SOFTKEY){ui_options=1;ui_choice=0;menu();}
    else if(code==VM_KEY_DOWN||code==VM_KEY_NUM8){if(ui_tab!=3)ui_move(1);status[0]=0;menu();}
    else if(code==VM_KEY_UP||code==VM_KEY_NUM2){if(ui_tab!=3)ui_move(-1);status[0]=0;menu();}
    else if(code==VM_KEY_OK||code==VM_KEY_NUM5){if(ui_tab!=3&&ui_accept(selected))open_selected();}
    else if(code==VM_KEY_POUND){scan_start();selected=0;status[0]=0;menu();}
    else if(code==VM_KEY_RIGHT_SOFTKEY||code==VM_KEY_BACK){vm_exit_app();}}

static void release_display(void){foreground=0;if(timer>=0){vm_delete_timer(timer);timer=-1;}if(vm)pj_pause(vm,1);buffer=NULL;if(layer>=0){vm_graphic_delete_layer(layer);layer=-1;}}
static void system_event(VMINT msg,VMINT param){(void)param;if(msg==VM_MSG_PAINT){if(layer<0){layer=vm_graphic_create_layer(0,0,240,320,-1);if(layer<0){vm_exit_app();return;}buffer=vm_graphic_get_layer_buffer(layer);vm_graphic_set_clip(0,0,240,320);}foreground=1;if(timer<0)timer=vm_create_timer(PJ_FRAME_MS,tick);if(mode==0){if(!scan_ready)scan_start();if(selected>count)selected=0;menu();}else if(mode==2)pause_menu();else{pj_pause(vm,0);pj_repaint(vm);}}else if(msg==VM_MSG_HIDE||msg==VM_MSG_INACTIVE)release_display();else if(msg==VM_MSG_QUIT){release_display();scan_stop();pj_destroy(vm);vm=NULL;}else if(msg==VM_MSG_CARD_PLUG_OUT){scan_stop();scan_ready=0;count=selected=0;pj_destroy(vm);vm=NULL;mode=0;strcpy(status,"SD card removed");if(foreground)menu();}}
void vm_main(void){vm_reg_sysevt_callback(system_event);vm_reg_keyboard_callback(key);}

unsigned char *platform_record_read(uint32_t store,int id,size_t*n){char path[100];snprintf(path,sizeof(path),"%srms-%08lx-%d.dat",root,(unsigned long)store,id);return platform_read(path,65536,n);}
int platform_record_write(uint32_t store,int id,const void*data,size_t n){char path[100];VMWCHAR wp[100];VMUINT written=0;snprintf(path,sizeof(path),"%srms-%08lx-%d.dat",root,(unsigned long)store,id);wide(wp,path,100);VMFILE f=vm_file_open(wp,MODE_CREATE_ALWAYS_WRITE,1);if(f<0)return 0;int ok=vm_file_write(f,(void*)data,(VMUINT)n,&written)>=0&&written==n;vm_file_close(f);return ok;}
int platform_record_remove(uint32_t store,int id){char path[100];VMWCHAR wp[100];snprintf(path,sizeof(path),"%srms-%08lx-%d.dat",root,(unsigned long)store,id);wide(wp,path,100);return vm_file_delete(wp)==0;}
