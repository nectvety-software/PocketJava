/* Exercise the public VM API; a ZIP error must not overwrite the adjacent title. */
#define main unused_host_main
#include "../native/host.c"
#undef main
int main(void){
    size_t baseline=pj_memory();PJ*v=pj_create((PJPlatform){rect,text,ms});
    if(!v)return 1;
    strcpy((char*)pj_title(v),"TITLE SENTINEL");
    unsigned char invalid[22]={0};
    int rejected=!pj_load_memory(v,invalid,sizeof(invalid));
    int intact=!strcmp(pj_title(v),"TITLE SENTINEL");
    pj_destroy(v);
    if(!rejected||!intact||pj_memory()!=baseline)return 1;
    puts("ERROR BOUNDARY PASS");return 0;
}
