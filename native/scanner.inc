/* Bounded whole-drive discovery, following RetroMRE's directory queue approach.
 * Only a small batch runs per timer callback. Queue memory is released before Java. */
#define SCAN_DIRS 96
#define SCAN_DEPTH 6
typedef struct {char path[260];int depth;} ScanDir;
static ScanDir *scan_queue;
static ScanDir scan_current;
static int scan_head,scan_tail,scan_pending,scan_active,scan_ready,scan_limited;
static VMINT scan_handle=-1;
static struct vm_fileinfo_t scan_info;
static void scan_stop(void){if(scan_handle>=0)vm_find_close(scan_handle);scan_handle=-1;pj_free(scan_queue);scan_queue=NULL;scan_active=0;}
static void scan_push(const char*path,int depth){if(scan_pending==SCAN_DIRS){scan_limited=1;return;}strcpy(scan_queue[scan_tail].path,path);scan_queue[scan_tail].depth=depth;scan_tail=(scan_tail+1)%SCAN_DIRS;scan_pending++;}
static int scan_drive(int drive){if(drive>='a'&&drive<='z')drive-=32;return drive>='A'&&drive<='Z'?drive:0;}
static void scan_start(void){char path[4];int card=scan_drive(vm_get_removable_driver()),phone=scan_drive(vm_get_system_driver());scan_stop();count=selected=0;scan_head=scan_tail=scan_pending=scan_limited=0;scan_ready=1;scan_queue=pj_alloc(sizeof(ScanDir)*SCAN_DIRS);if(!scan_queue){strcpy(status,"Insufficient scan RAM");return;}if(!card&&!phone)card='E';snprintf(root,sizeof(root),"%c:\\j2me\\",card?card:phone);path[1]=':';path[2]='\\';path[3]=0;if(card){path[0]=(char)card;scan_push(path,0);}if(phone&&phone!=card){path[0]=(char)phone;scan_push(path,0);}scan_active=1;}
static int scan_name(char*out,const VMWCHAR*in,size_t words){size_t n=0;for(size_t i=0;i<words;i++){unsigned c=in[i];if(!c){out[n]=0;return 1;}size_t bytes=c<128?1:c<2048?2:3;if(n+bytes>=260)return 0;if(bytes==1)out[n++]=(char)c;else if(bytes==2){out[n++]=(char)(192|(c>>6));out[n++]=(char)(128|(c&63));}else{out[n++]=(char)(224|(c>>12));out[n++]=(char)(128|((c>>6)&63));out[n++]=(char)(128|(c&63));}}return 0;}
static int scan_prefix(const char*a,const char*b){while(*b){unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;if(x>='a'&&x<='z')x-=32;if(y>='a'&&y<='z')y-=32;if(x!=y)return 0;}return 1;}
static void scan_step(unsigned budget){while(scan_active&&budget--){VMWCHAR widepath[260];char path[260],name[260];if(scan_handle<0){if(!scan_pending){scan_stop();break;}scan_current=scan_queue[scan_head];scan_head=(scan_head+1)%SCAN_DIRS;scan_pending--;size_t n=strlen(scan_current.path);if(n+2>=sizeof(path)){scan_limited=1;continue;}snprintf(path,sizeof(path),"%s%s*",scan_current.path,n&&scan_current.path[n-1]=='\\'?"":"\\");wide(widepath,path,260);memset(&scan_info,0,sizeof(scan_info));scan_handle=vm_find_first(widepath,&scan_info);if(scan_handle<0)continue;}
        if(scan_name(name,scan_info.filename,sizeof(scan_info.filename)/sizeof(scan_info.filename[0]))){char*base=strrchr(name,'\\');base=base?base+1:name;if(strcmp(base,".")&&strcmp(base,"..")&&*base){if(name[1]==':'){if(!scan_prefix(name,scan_current.path)){scan_limited=1;goto next_entry;}strcpy(path,name);}else{size_t a=strlen(scan_current.path),b=strlen(base);if(a+b+2>sizeof(path)){scan_limited=1;goto next_entry;}memcpy(path,scan_current.path,a);if(a&&path[a-1]!='\\')path[a++]='\\';memcpy(path+a,base,b+1);}wide(widepath,path,260);int attr=vm_file_get_attributes(widepath);if(attr>=0&&(attr&VM_FS_ATTR_DIR)){if(scan_current.depth<SCAN_DEPTH)scan_push(path,scan_current.depth+1);else scan_limited=1;}else if(attr>=0&&(extension(base,".jar")||extension(base,".jad"))){int duplicate=0;for(int i=0;i<count;i++)if(!strcmp(paths[i],path))duplicate=1;if(!duplicate){strcpy(paths[count++],path);if(count==64){scan_limited=1;scan_stop();break;}}}}}else scan_limited=1;
next_entry: if(vm_find_next(scan_handle,&scan_info)<0){vm_find_close(scan_handle);scan_handle=-1;}
    }}
