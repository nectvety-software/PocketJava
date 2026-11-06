/* RetroMRE-inspired paper/orange launcher, drawn into the existing framebuffer. */
#define UI_PAPER 0xf7f2e7u
#define UI_ORANGE 0xc85a1eu
#define UI_INK 0x292820u
#define UI_LINE 0xd8c7adu
#define UI_SELECT 0xf7cba9u
static int ui_tab,ui_options,ui_choice;
static int ui_accept(int index){return index==0?ui_tab!=2:ui_tab==0||(ui_tab==1&&extension(paths[index-1],".jar"))||(ui_tab==2&&extension(paths[index-1],".jad"));}
static void ui_move(int direction){for(int i=0;i<=count;i++){selected=(selected+count+1+direction)%(count+1);if(ui_accept(selected))break;}}
static void ui_header(const char*title){drawrect(0,0,240,320,UI_PAPER);drawrect(0,16,240,23,UI_ORANGE);drawrect(6,23,4,4,0xffffff);drawrect(11,26,4,4,0xffffff);drawtext(22,20,title,0xffffff);}
static void ui_footer(const char*left,const char*right){drawrect(0,300,240,1,UI_LINE);drawtext(4,304,left,UI_INK);drawtext(198,304,right,UI_INK);}
static void ui_dialog(void){
    static const char*items[]={"1 OPEN","2 RESCAN","3 INFO","4 BACK"};
    drawrect(14,84,212,122,UI_PAPER);drawrect(14,84,212,19,UI_INK);drawtext(22,86,"OPTIONS",0xffffff);
    for(int i=0;i<4;i++){int y=104+i*25;if(i==ui_choice)drawrect(15,y,210,24,UI_SELECT);drawtext(22,y+4,items[i],UI_INK);drawrect(15,y+24,210,1,UI_LINE);}
    ui_footer("OK","BACK");
}
static void ui_status(const char*text){
    /* Error text can exceed the 30-glyph row: wrap it over up to three lines. */
    char part[120];const char*rest=preview(part,text,31);
    if(!*rest){drawtext(4,282,part,UI_ORANGE);return;}
    drawrect(0,252,240,40,UI_PAPER);drawtext(4,254,part,UI_ORANGE);
    for(int row=1;row<3&&*rest;row++){rest=preview(part,rest,31);drawtext(4,254+row*13,part,UI_ORANGE);}
}
static void menu(void){
    char label[100],line[120];int total=0,position=0;
    if(ui_options){drawrect(0,0,240,300,0x8e8c7d);drawrect(0,300,240,20,UI_PAPER);ui_dialog();vm_graphic_flush_layer(&layer,1);return;}
    ui_header("POCKETJAVA");
    for(int i=0;i<=count;i++)if(ui_accept(i)){if(i==selected)position=total;total++;}
    snprintf(line,sizeof(line),"%d/%d",total?position+1:0,total);drawrect(195,20,41,16,UI_PAPER);drawtext(198,20,line,UI_ORANGE);
    static const char*tabs[]={"GAMES","JAR","JAD","INFO"};
    for(int t=0;t<4;t++){drawrect(t*60,40,60,21,t==ui_tab?UI_ORANGE:0xf2ead8);drawtext(t*60+7,43,tabs[t],t==ui_tab?0xffffff:UI_INK);if(t)drawrect(t*60,44,1,13,UI_LINE);}
    if(ui_tab==3){
        drawtext(8,72,"POCKETJAVA / MRE",UI_INK);drawtext(8,98,"Java JAR + JAD",UI_INK);
        drawtext(8,126,"2/8: SELECT   5: OPEN",UI_INK);drawtext(8,150,"4/6: CHANGE TAB",UI_INK);drawtext(8,174,"#: RESCAN",UI_INK);
        drawtext(8,202,"IN GAME: * PAUSE",UI_INK);drawtext(8,226,"12000 OPS / 8 MS",UI_ORANGE);drawtext(8,250,"33 MS FRAME TIMER",UI_ORANGE);
    }else{
        drawtext(8,64,scan_active?"SCANNING...":scan_limited?"SCAN LIMIT":ui_tab==1?"JAR GAMES":ui_tab==2?"JAD DESCRIPTORS":"ALL GAMES",0x686050);
        int first=position>4?position-4:0,row=0,ordinal=0;
        for(int i=0;i<=count&&row<5;i++)if(ui_accept(i)){
            if(ordinal++<first)continue;int y=80+row*40;row++;
            if(i==selected){drawrect(0,y,236,38,UI_SELECT);drawrect(234,y,3,38,UI_ORANGE);}
            drawrect(5,y+6,24,24,UI_LINE);drawrect(6,y+7,22,22,UI_PAPER);
            drawrect(10,y+14,14,9,i?UI_ORANGE:0x407aaa);drawrect(13,y+11,8,3,i?UI_ORANGE:0x407aaa);drawrect(13,y+17,2,2,UI_PAPER);drawrect(20,y+17,2,2,UI_PAPER);
            preview(label,i?basename_of(paths[i-1]):"BUILT-IN DEMO",22);drawtext(37,y+4,label,UI_INK);
            drawtext(37,y+22,i?(extension(paths[i-1],".jad")?"JAD | DESCRIPTOR":"JAR | JAVA GAME"):"JAVA | OFFLINE DEMO",0x686050);drawrect(0,y+38,237,1,UI_LINE);
        }
        if(!total)drawtext(14,108,"NO MATCHING FILES",0x686050);
        drawrect(238,80,2,199,UI_LINE);if(total){int h=total<=5?199:199*5/total;if(h<8)h=8;int offset=total<=5?0:(199-h)*position/(total-1);drawrect(238,80+offset,2,h,UI_ORANGE);}
        if(status[0])ui_status(status);else if(selected){preview(line,paths[selected-1],31);drawtext(4,282,line,0x686050);}
    }
    ui_footer("MENU","EXIT");vm_graphic_flush_layer(&layer,1);
}
static void pause_menu(void){
    char s[80];ui_header("PAUSED");preview(s,pj_title(vm),30);drawtext(8,52,s,UI_INK);
    drawrect(14,96,212,103,UI_LINE);drawrect(15,97,210,101,UI_PAPER);drawrect(15,97,210,26,UI_SELECT);
    drawtext(22,103,"5 / *  RESUME",UI_INK);drawtext(22,140,"0  STOP GAME",UI_INK);
    snprintf(s,sizeof(s),"RAM %luK / PEAK %luK",(unsigned long)(pj_memory()/1024),(unsigned long)(pj_peak()/1024));drawtext(8,222,s,0x686050);
    snprintf(s,sizeof(s),"OPS %lu",(unsigned long)pj_instructions(vm));drawtext(8,248,s,0x686050);ui_footer("RESUME","0 STOP");vm_graphic_flush_layer(&layer,1);
}
