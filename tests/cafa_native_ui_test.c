/* SPDX-License-Identifier: GPL-3.0-only */
/* A genuine native-page contract, not a screenshot of a lookalike renderer. */
#include <assert.h>
#include "../firmware/src/cafa_protocol.h"
#define UI_TEST_NO_MAIN 1
#define FELUCCA_CAFA 1
#ifndef FELUCCA_CONTROLLER
#define FELUCCA_CONTROLLER 0
#endif
#define FELUCCA_OTA 1
static cafa_link_t cafa;
static volatile uint8_t cafa_audio_on;
static int cafa_frame(const uint8_t *,uint32_t);
static int cafa_input(void),cafa_draw(void),cafa_leds(void);
static unsigned native_full_fills,native_pixels;
static void native_fill(uint32_t x,uint32_t y,uint32_t w,uint32_t h) {
 native_pixels+=w*h; if(!x && !y && w==240 && h==240)native_full_fills++;
}
static void native_blit(uint32_t x,uint32_t y,uint32_t w,uint32_t h) { (void)x;(void)y;native_pixels+=w*h; }
#define HOST_LCD_FILL_HOOK native_fill
#define HOST_LCD_BLIT_HOOK native_blit
#include "ui_test.c"
static uint32_t ota_now_ms(void) { return fm1_ms; }
static void ota_idle(void) { fm1_ms++; }
#include "../firmware/src/cafa.c"
static uint16_t image[240*240];
static void state(uint8_t *f,unsigned rev,unsigned page) {
 memset(f,0,101); f[0]=0x7d;f[1]=0x43;f[2]=0x41;f[3]=2;f[4]=CAFA_STATE;
 cafa_put14(f+5,123);cafa_put14(f+7,rev);
 f[9]=4;f[10]=page;f[11]=24; /* zero octave */
 cafa_put14(f+12,1024);cafa_put14(f+14,1008);cafa_put14(f+16,1023);cafa_put14(f+18,120);
 memcpy(f+29,"PHONE TRACK FIVE",16);memcpy(f+45,"Harmonic cloud  ",16);
 memcpy(f+61,"ATTACK  DECAY   SUSTAIN RELEASE ",32);
 cafa_put14(f+93,10);cafa_put14(f+95,48);cafa_put14(f+97,75);cafa_put14(f+99,60);
}
static void dump(const char *name) {
 FILE *f=fopen(name,"wb");unsigned i;assert(f);fprintf(f,"P6\n240 240\n255\n");
 for(i=0;i<240*240;i++){unsigned r=host_screen[i],p=((r>>8)|(r<<8))&65535u;uint8_t c[3]={(p>>11)*255/31,((p>>5)&63)*255/63,(p&31)*255/31};fwrite(c,1,3,f);}fclose(f);
}
static void palette_parity(void) {
 unsigned pal,large,variant,i,j,count=0;uint8_t f[101];
 static const unsigned env[3][4]={{0,0,0,0},{10,48,75,60},{127,127,127,127}};
 for(pal=0;pal<NPALETTES;pal++)for(large=0;large<2;large++)for(variant=0;variant<3;variant++) {
  ui_power_on();palette_set(pal);if(large)ui_prefs|=PREF_LARGE;
  state(f,1,3);for(i=0;i<4;i++)cafa_put14(f+93+2*i,env[variant][i]);
  memcpy(cafa.view,f+9,CAFA_STATE_BYTES);cafa.version=2;cafa.phase=2;cafa_audio_on=1;cafa_dirty=2;cafa_native_shown=0;
  cafa_draw();memcpy(image,host_screen,sizeof image);
  ui_power_on();palette_set(pal);if(large)ui_prefs|=PREF_LARGE;
  sel_set(4);memset(TSEL,0,sizeof *TSEL);TSEL->p[P_SLEN]=16;
  TSEL->p[P_ATK]=env[variant][0];TSEL->p[P_DEC]=env[variant][1];TSEL->p[P_SUS]=env[variant][2];TSEL->p[P_REL]=env[variant][3];
  memset(&ui,0,sizeof ui);ui.page=0;ui.force=1;fr.on=0;memset(&motion,0,sizeof motion);ui_draw_page(0);
  for(j=Y_LABEL;j<foot_y();j++)for(i=0;i<240;i++)assert(image[j*240+i]==host_screen[j*240+i]);
  count++;
 }
 printf("Native ENV pixel parity: %u palette/size/envelope scenes match actual Skiff renderer\n",count);
}
static void loom_v4(void) {
 uint8_t f[101],saved_tracks[sizeof trk],saved_ui[sizeof ui];song_t saved_song;
 unsigned pal,large,page,rev=1,i;char label[9],value[12];const char *unit;
 static const char *labels[]={
 "VOLUME% PAN+100 MUTE    BPM     ","NOTE    VEL%    GATE x10PROB%   ",
 "CUTOFF  RESO    TONE    DRIVE   ","ATTACK  DECAY   SUSTAIN RELEASE ",
 "BUS     SEND%   UNITY   ENABLED ","VOLUME% PAN+100 MUTE    TRACK   ",
 "RATE%   DEPTH%  SHAPE   SYNC    ","MODE    OCT+3   LATCH   RATE x2 ",
 "LENGTH  BPM     SWING+23HUMAN%  "};
 for(pal=0;pal<NPALETTES;pal++)for(large=0;large<2;large++) {
  ui_power_on();palette_set(pal);if(large)ui_prefs|=PREF_LARGE;
  cafa.phase=0;state(f,0,0);f[3]=4;f[4]=CAFA_HELLO;cafa_frame(f,9);so_r=so_w;
  for(page=0;page<9;page++) {
   state(f,rev++,page);f[3]=4;f[9]=7;f[11]=55;memcpy(f+61,labels[page],32);
   cafa_put14(f+18,12);f[20]=127;f[21]=127;f[22]=3;
   memcpy(saved_tracks,trk,sizeof trk);memcpy(saved_ui,&ui,sizeof ui);saved_song=song;
   cafa_frame(f,101);so_r=so_w;cafa_draw();assert(cafa_native_shown);
   /* Entry alone silences standalone; draw never changes its saved state. */
   memcpy(saved_tracks,trk,sizeof trk);memcpy(saved_ui,&ui,sizeof ui);saved_song=song;
   cafa_dirty=2;cafa_draw();
   assert(!memcmp(saved_tracks,trk,sizeof trk));assert(!memcmp(saved_ui,&ui,sizeof ui));assert(!memcmp(&saved_song,&song,sizeof song));
   native_pixels=0;cafa_draw();assert(!native_pixels);
   cafa_put14(f+7,rev++);cafa_put14(f+93,100);cafa_frame(f,101);so_r=so_w;cafa_draw();
   for(i=0;i<42;i++)cafa_draw();memcpy(image,host_screen,sizeof image);cafa_dirty=2;cafa_draw();assert(!memcmp(image,host_screen,sizeof image));
   if(pal==0 && !large){char path[64];sprintf(path,"build/loom-v4-page-%u.ppm",page);dump(path);}
  }
 }
 state(f,rev++,7);f[3]=4;memcpy(f+61,"OCT+3   INV+2   GATE x10RATE x2 ",32);
 cafa_put14(f+93,0);cafa_loom_value(f+9,0,label,value,&unit);assert(!strcmp(label,"OCT") && !strcmp(value,"-3"));
 cafa_put14(f+95,4);cafa_loom_value(f+9,1,label,value,&unit);assert(!strcmp(value,"2"));
 cafa_put14(f+97,640);cafa_loom_value(f+9,2,label,value,&unit);assert(!strcmp(value,"64.0"));
 cafa_put14(f+99,1);cafa_loom_value(f+9,3,label,value,&unit);assert(!strcmp(value,"0.5"));
 puts("Loom v4: all nine native pages, all palettes/tall cards, T8, state isolation, idle no-write, incremental/full parity and offset units passed");
}
static void loom_fixtures(void) {
 const char *dir=getenv("LOOM_V5_FIXTURES");unsigned version=5;char path[1024];uint8_t f[180];unsigned t,p;FILE *fp;size_t n;
 if(!dir){dir=getenv("LOOM_V4_FIXTURES");version=4;}
 if(!dir)return;
 ui_power_on();cafa.phase=0;usb.config=1;
 snprintf(path,sizeof path,"%s/hello.syx",dir);fp=fopen(path,"rb");assert(fp);n=fread(f,1,sizeof f,fp);fclose(fp);
 cafa_frame(f+1,n-2);so_r=so_w;assert(cafa.phase==1 && cafa.version==version);
 for(t=1;t<=8;t++)for(p=0;p<9;p++) {
  snprintf(path,sizeof path,"%s/t%u-p%u.syx",dir,t,p);fp=fopen(path,"rb");assert(fp);n=fread(f,1,sizeof f,fp);fclose(fp);
  assert(n==11+cafa_state_bytes(version) && f[0]==240 && f[n-1]==247);cafa_frame(f+1,n-2);so_r=so_w;
  assert(cafa.phase==2 && cafa.view[0]==t-1 && cafa.view[1]==(version==5 && p==2 ? 9 : p) && cafa.revision==cafa_u14(f+8));cafa_draw();
  if(t==8){snprintf(path,sizeof path,"build/loom-v4-model-%u.ppm",p);dump(path);}
 }
 for(t=0;t<2;t++) {
  snprintf(path,sizeof path,"%s/tempo%u.syx",dir,t?300:12);fp=fopen(path,"rb");assert(fp);n=fread(f,1,sizeof f,fp);fclose(fp);
  cafa_frame(f+1,n-2);so_r=so_w;assert(cafa_u14(cafa.view+9)==(t?300:12));cafa_draw();
 }
 snprintf(path,sizeof path,"%s/bye.syx",dir);fp=fopen(path,"rb");assert(fp);n=fread(f,1,sizeof f,fp);fclose(fp);
 cafa_frame(f+1,n-2);assert(!cafa.phase);
 puts("Real Loom app fixtures: 72 model snapshots, 12/300 BPM, HELLO/BYE accepted and natively drawn");
}
static unsigned dynamic_drain(uint8_t *f) {
 unsigned n=0;while(so_r!=so_w){uint32_t w=sx_out_q[so_r++%SXQ];unsigned cin=w&15,k=cin==4?3:cin-4,j;
 assert(k<=3);for(j=0;j<k;j++)f[n++]=(w>>(8+8*j))&255;}return n;
}
static void dynamic_v5(void) {
 uint8_t f[165],out[128],saved_tracks[sizeof trk],saved_ui[sizeof ui];song_t saved_song;
 unsigned pal,large,i,rev=1;char label[9],value[13];
 for(pal=0;pal<NPALETTES;pal++)for(large=0;large<2;large++) {
  ui_power_on();palette_set(pal);if(large)ui_prefs|=PREF_LARGE;
  cafa.phase=0;cafa_release_guard=0;host_pressed=host_notes=0;memset(f,0,sizeof f);state(f,0,9);f[3]=5;f[4]=CAFA_HELLO;
  cafa_frame(f,9);so_r=so_w;f[4]=CAFA_STATE;cafa_put14(f+7,rev++);
  memcpy(f+29,"RUNTIME PLUGIN  ",16);memcpy(f+45,"CUSTOM BANK     ",16);
  memcpy(f+61,"GAIN    WAVE    ENABLE  TRIGGER ",32);
  cafa_put14(f+9+92,1);cafa_put14(f+9+94,3);cafa_put14(f+9+96,99);cafa_put14(f+9+98,100);
  memcpy(f+9+100,"-12.34 dB   SAW         OFF         RUN         ",48);
  for(i=0;i<4;i++)f[9+148+i]=i+1;cafa_put14(f+9+152,42);
  cafa_frame(f,sizeof f);so_r=so_w;assert(cafa.phase==2 && cafa.version==5);cafa_draw();
  memcpy(saved_tracks,trk,sizeof trk);memcpy(saved_ui,&ui,sizeof ui);saved_song=song;
  native_pixels=0;cafa_draw();assert(!native_pixels);
  cafa_dynamic_value(cafa.view,0,label,value);assert(!strcmp(label,"GAIN") && !strcmp(value,"-12.34 dB"));
  /* Text-only changes must redraw despite an unchanged normalized knob value. */
  memcpy(f+9+100,"+6.00 dB    ",12);cafa_put14(f+7,rev++);cafa_frame(f,sizeof f);so_r=so_w;
  native_pixels=0;cafa_draw();assert(native_pixels);for(i=0;i<42;i++)cafa_draw();
  memcpy(image,host_screen,sizeof image);cafa_dirty=2;cafa_draw();assert(!memcmp(image,host_screen,sizeof image));
  assert(!memcmp(saved_tracks,trk,sizeof trk) && !memcmp(saved_ui,&ui,sizeof ui) && !memcmp(&saved_song,&song,sizeof song));
  if(pal==0 && !large)dump("build/cafa-dynamic-v5.ppm");
  /* Inputs refer to the page actually shown, not a newer page waiting for draw. */
  cafa_put14(f+7,rev++);cafa_put14(f+9+152,43);cafa_frame(f,sizeof f);so_r=so_w;
  fm1_in.buttons=fm1_in.notes=0;cafa_first=1;cafa_input();assert(dynamic_drain(out)==33 && cafa_u14(out+30)==42);
  cafa_draw();host_enc[panel.enc[0]]=panel.dir[0];cafa_input();assert(dynamic_drain(out)==33 && cafa_u14(out+30)==43);
 }
 f[4]=CAFA_BYE;cafa_frame(f,9);assert(!cafa.phase && cafa_audio_on == FELUCCA_CONTROLLER);
 puts("CAFA v5: runtime labels/formatted values, 100-engine catalog, native cards in all palettes/sizes, text-only redraw, project isolation and displayed-context INPUT passed");
}
#include "cafa_native_overlay_test.h"
#include "cafa_native_graph_test.h"
/* Drive real VIEW_PART publications and measure the LCD writes between them.
 * A final framebuffer alone cannot catch an erase followed by the same pixels. */
static void publish_native_test_view(const uint8_t *view,unsigned length,unsigned revision)
{
 uint8_t frame[165]={0x7d,0x43,0x41,6,WIRE6_VIEW_PART,123,0};
 unsigned offset=0;
 cafa_put14(frame+7,revision);cafa_put14(frame+9,length);
 while(offset<length){
  unsigned count=length-offset<152u?length-offset:152u;
  cafa_put14(frame+11,offset);memcpy(frame+13,view+offset,count);
  assert(cafa_frame(frame,13u+count));offset+=count;fm1_ms+=34;
 }
 assert(wire6.phase==2 && wire6.revision==revision);so_r=so_w;
}
static void transient_overlay_keeps_graph(void)
{
 uint8_t view[3072];
 ui_power_on();
 unsigned length=native_test_view(view,0);
 cafa_native_profile_draw(view,length,1);
 memcpy(image,host_screen,sizeof image);
 uint8_t *tag=view+length;
 tag[0]=CAFA_NATIVE_TRANSIENT_BOX;tag[1]=67;tag[2]=0;
 tag[3]=tag[4]=0;memset(tag+5,' ',64);memcpy(tag+5,"RECORDED",8);tag[69]=1;
 assert(cafa_native_view_valid(view,length+70));
 cafa_native_profile_draw(view,length+70,1);
 int changed=0;
 for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++)if(image[240*y+x]!=host_screen[240*y+x]) {
   assert(x>=TB_X-1 && x<=TB_X+TB_W && y>=graph_y()+TB_Y-1 && y<=graph_y()+TB_Y+TB_H);
   changed=1;
 }
 assert(changed);
 cafa_native_profile_draw(view,length,1);
 assert(!memcmp(image,host_screen,sizeof image));
}
static void native_refresh_tests(void)
{
 uint8_t hello[]={0x7d,0x43,0x41,6,WIRE6_HELLO,123,0,0,0,1,0,0,1,'A',2,1,1,0,1,9,1,0,1};
 uint8_t view[3072];unsigned length;
 ui_power_on();wire6_leave();usb.config=1;
 assert(cafa_frame(hello,sizeof hello));assert(wire6.phase==1);so_r=so_w;
 length=native_test_view(view,0);
 publish_native_test_view(view,length,1);assert(cafa_draw());
 native_pixels=0;publish_native_test_view(view,length,2);assert(cafa_draw());
 assert(native_pixels==0); /* Periodic publication of the same view. */
 memcpy(image,host_screen,sizeof image);
 memcpy(view+81,"UPDATED FOOTER",14);wire6_put28(view+2,28);
 publish_native_test_view(view,length,3);
 publish_native_test_view(view,length,4); /* A repeat before the display gets CPU time. */
 assert(wire6.drawn_context==27);
 native_full_fills=0;assert(cafa_draw());
 assert(wire6.drawn_context==28 && !native_full_fills);
 assert(memcmp(image,host_screen,sizeof image));
 memcpy(image,host_screen,sizeof image);
 cafa_native_profile_draw(view,length,1);
 assert(!memcmp(image,host_screen,sizeof image));
 length=native_test_view(view,57);
 publish_native_test_view(view,length,5);assert(cafa_draw());
 assert(cafa_input());so_r=so_w;
 memcpy(image,host_screen,sizeof image);
 host_enc[panel.enc[EN_PRESET]]=panel.dir[EN_PRESET];
 assert(cafa_input());assert(cafa_draw());so_r=so_w;
 assert(memcmp(image,host_screen,sizeof image)); /* Local menu navigation with an idle host. */
 native_pixels=0;assert(cafa_draw());assert(native_pixels==0);
 wire6_leave();
}
static void native_region_parity(const uint8_t *view,unsigned length)
{
 memcpy(image,host_screen,sizeof image);
 cafa_native_profile_draw(view,length,1);
 assert(!memcmp(image,host_screen,sizeof image));
}
static void native_region_tests(void)
{
 uint8_t hello[]={0x7d,0x43,0x41,6,WIRE6_HELLO,123,0,0,0,1,0,0,1,'A',2,1,1,0,1,9,1,0,1};
 uint8_t view[3072];unsigned length,change,rev=1;
 ui_power_on();wire6_leave();usb.config=1;
 assert(cafa_frame(hello,sizeof hello));so_r=so_w;
 length=native_test_view(view,0);length=native_graph_section(view,0,&change);
 memcpy(view+112,"ATTACK      ",12);memcpy(view+124,"50            ",14);
 cafa_put14(view+142,8192);view[144]=1;view[145]=1;
 memcpy(view+112+72,"DEPTH       ",12);memcpy(view+124+72,"19            ",14);
 cafa_put14(view+142+72,3000);view[144+72]=1;view[145+72]=1;
 assert(cafa_native_view_valid(view,length));
 publish_native_test_view(view,length,rev++);assert(cafa_draw());
 native_region_parity(view,length);
 /* One card's text and value updates only its own 57-pixel strip. */
 memcpy(view+112,"CUTOFF      ",12);memcpy(view+124,"51            ",14);
 cafa_put14(view+142,8500);
 publish_native_test_view(view,length,rev++);native_pixels=native_full_fills=0;assert(cafa_draw());
 assert(native_pixels>0 && native_pixels<5000 && !native_full_fills);
 native_region_parity(view,length);
 memcpy(view+33,"UPDATED TITLE",13);
 publish_native_test_view(view,length,rev++);native_pixels=native_full_fills=0;assert(cafa_draw());
 assert(native_pixels>0 && native_pixels<8000 && !native_full_fills);
 native_region_parity(view,length);
 cafa_put14(view+change,14000u);
 publish_native_test_view(view,length,rev++);native_pixels=native_full_fills=0;assert(cafa_draw());
 assert(native_pixels>8000 && native_pixels<57600 && !native_full_fills);
 native_region_parity(view,length);
 memcpy(view+81,"UPDATED FOOTER",14);
 publish_native_test_view(view,length,rev++);native_pixels=native_full_fills=0;assert(cafa_draw());
 assert(native_pixels>0 && native_pixels<10000 && !native_full_fills);
 native_region_parity(view,length);
 /* Two publications before the frame must retain both card regions. */
 memcpy(view+124,"52            ",14);publish_native_test_view(view,length,rev++);
 memcpy(view+124+72,"20            ",14);publish_native_test_view(view,length,rev++);
 native_pixels=native_full_fills=0;assert(cafa_draw());
 assert(native_pixels>2500 && native_pixels<8000 && !native_full_fills);
 native_region_parity(view,length);
 /* ENV playback position and transport affect the header, not its curve. */
 view[32]=1;cafa_put14(view+22,4);
 publish_native_test_view(view,length,rev++);native_pixels=native_full_fills=0;assert(cafa_draw());
 assert(native_pixels>0 && native_pixels<8000 && !native_full_fills);
 native_region_parity(view,length);
 /* A card value also feeds the FX fader graph; subtitle titles the scale graph. */
 length=native_test_view(view,5);length=native_graph_section(view,5,&change);
 publish_native_test_view(view,length,rev++);assert(cafa_draw());native_region_parity(view,length);
 cafa_put14(view+112+30,9500);
 publish_native_test_view(view,length,rev++);native_pixels=native_full_fills=0;assert(cafa_draw());
 assert(native_pixels>8000 && !native_full_fills);native_region_parity(view,length);
 length=native_test_view(view,12);length=native_graph_section(view,12,&change);
 publish_native_test_view(view,length,rev++);assert(cafa_draw());native_region_parity(view,length);
 memcpy(view+57,"C SHARP SCALE",13);
 publish_native_test_view(view,length,rev++);native_pixels=native_full_fills=0;assert(cafa_draw());
 assert(native_pixels>8000 && !native_full_fills);native_region_parity(view,length);
 /* A layer redraw remains bounded even when its fields overlap several painters. */
 length=native_test_view(view,49);publish_native_test_view(view,length,rev++);assert(cafa_draw());
 memcpy(view+81,"LAYER UPDATE",12);
 publish_native_test_view(view,length,rev++);native_pixels=native_full_fills=0;assert(cafa_draw());
 assert(native_pixels>0 && !native_full_fills);native_region_parity(view,length);
 view[32]=1u;cafa_put14(view+22,12u);
 publish_native_test_view(view,length,rev++);assert(cafa_draw());
 cafa_put14(view+22,16u);
 publish_native_test_view(view,length,rev++);native_pixels=native_full_fills=0;assert(cafa_draw());
 assert(native_pixels==240u*H_HEAD && !native_full_fills);native_region_parity(view,length);
 wire6_leave();
 puts("Native kind-7 regions: card, head, graph, footer, accumulated updates and ENV playback are selective and full-frame exact");
}
static void native_chrome_tests(void)
{
 uint8_t old[3072],view[3072];unsigned n,k,mask,changed=0;
 uint8_t *row,*chrome;
 ui_power_on();ui_prefs|=PREF_LARGE;
 n=native_test_view(view,CAFA_SCREEN_HOME);
 cafa_put14(view+15,NTRK);cafa_put14(view+24,2);cafa_put14(view+28,NTRK);
 view[n]=CAFA_NATIVE_ROWS;cafa_put14(view+n+1,1u+48u*NTRK);view[n+3]=NTRK;
 for(k=0;k<NTRK;k++) {
  row=view+n+4u+48u*k;memset(row,' ',40u);memcpy(row,"SOUND A",7);
  row[40]=(uint8_t)(k==2u ? 1u : 0u);cafa_put14(row+43,16383u);
 }
 n+=4u+48u*NTRK;
 view[n]=CAFA_NATIVE_CHROME;cafa_put14(view+n+1,56u);chrome=view+n+3u;
 memset(chrome,0,56u);cafa_put14(chrome,16383u);
 memset(chrome+3u,' ',9u);memset(chrome+13u,' ',40u);
 memcpy(chrome+29u,"ANALOG  ",8);memcpy(chrome+37u,"SOUND A         ",16);
 n+=59u;assert(cafa_native_view_valid(view,n));
 cafa_native_profile_draw(view,n,1);memcpy(old,view,n);
 memcpy(chrome+37u,"SOUND B",7);
 mask=cafa_native_profile_regions(old,n,view,n);
 assert(mask==CAFA_PAINT_FOOT);cafa_native_profile_draw_regions(view,n,0,mask);native_region_parity(view,n);
 memcpy(old,view,n);cafa_put14(chrome,19u);
 mask=cafa_native_profile_regions(old,n,view,n);
 assert(mask==CAFA_PAINT_HEAD);cafa_native_profile_draw_regions(view,n,0,mask);native_region_parity(view,n);
 memcpy(old,view,n);chrome[53u]=1u;
 mask=cafa_native_profile_regions(old,n,view,n);
 assert(mask==0x1eu);cafa_native_profile_draw_regions(view,n,0,mask);native_region_parity(view,n);
 memcpy(old,view,n);row=view+CAFA_NATIVE_HEADER+4u+48u*3u;
 memcpy(row,"BRAVO  ",7);
 mask=cafa_native_profile_regions(old,n,view,n);
 assert(mask==CAFA_PAINT_GRAPH);
 memcpy(image,host_screen,sizeof image);
 cafa_native_profile_draw_regions(view,n,0,mask);
 for(unsigned y=graph_y();y<graph_y()+graph_h();y++)for(unsigned x=120u;x<238u;x++)
  changed|=image[y*240u+x]!=host_screen[y*240u+x];
 assert(changed);native_region_parity(view,n);
 /* A third-party producer may put its rows after CHROME. */
 memcpy(old,view,n);
 memcpy(view+CAFA_NATIVE_HEADER,old+n-59u,59u);
 memcpy(view+CAFA_NATIVE_HEADER+59u,old+CAFA_NATIVE_HEADER,n-CAFA_NATIVE_HEADER-59u);
 assert(cafa_native_view_valid(view,n));
 memcpy(old,view,n);
 memcpy(view+CAFA_NATIVE_HEADER+59u+4u+3u*48u,"CHARLIE",7u);
 mask=cafa_native_profile_regions(old,n,view,n);
 assert(mask==CAFA_PAINT_GRAPH);cafa_native_profile_draw_regions(view,n,0,mask);native_region_parity(view,n);
 puts("Native CHROME header, cards, footer and six-sound large HOME redraw match full frames");
}
static unsigned native_chord_view(uint8_t *v,unsigned screen,unsigned mode)
{
 unsigned n=native_test_view(v,screen),count=mode==3u?1u:2u;
 uint8_t *keys,*row;
 memcpy(v+57,mode<3u?(screen==13u?"CHORD KEYS OFF":"FOLLOW OFF"):"C MAJOR",mode<3u?(screen==13u?14u:10u):7u);
 v[n]=CAFA_NATIVE_KEYBOARD;cafa_put14(v+n+1,20u);keys=v+n+3u;
 keys[48u/7u]|=1u<<(48u%7u);keys[60u/7u]|=1u<<(60u%7u);
 keys[84u/7u]|=1u<<(84u%7u);keys[19]=0;n+=23u;
 v[n]=CAFA_NATIVE_ROWS;cafa_put14(v+n+1,1u+48u*count);v[n+3]=(uint8_t)count;
 row=v+n+4u;memset(row,' ',40u);
 memcpy(row,screen==13u?"CHORD VIEW V1":"FOLLOW VIEW V1",screen==13u?13u:14u);
 memcpy(row+16u,"ON ROOT",7u);row[40]=screen==14u?2u:0u;
 cafa_put14(row+41u,mode);cafa_put14(row+43u,48u);cafa_put14(row+45u,84u);
 if(count==2u) {
  row+=48u;memset(row,' ',40u);
  if(mode==4u){memcpy(row,"STRUM",5u);cafa_put14(row+41u,40u);cafa_put14(row+43u,2u);}
  else memcpy(row+16u,"[K1] A TRACK TO FOLLOW",22u);
 }
 n+=4u+48u*count;assert(cafa_native_view_valid(v,n));return n;
}
static void native_chord_rows_tests(void)
{
 uint8_t v[3072],legacy[3072];unsigned n,plain;
 ui_power_on();
 n=native_chord_view(v,13u,0u);cafa_native_profile_draw(v,n,1);memcpy(image,host_screen,sizeof image);
 n=native_chord_view(v,13u,3u);cafa_native_profile_draw(v,n,1);
 assert(memcmp(image,host_screen,sizeof image));memcpy(image,host_screen,sizeof image);
 n=native_chord_view(v,13u,4u);cafa_native_profile_draw(v,n,1);
 assert(memcmp(image,host_screen,sizeof image));
 n=native_chord_view(v,14u,2u);cafa_native_profile_draw(v,n,1);memcpy(image,host_screen,sizeof image);
 n=native_chord_view(v,14u,3u);cafa_native_profile_draw(v,n,1);
 assert(memcmp(image,host_screen,sizeof image));memcpy(image,host_screen,sizeof image);
 cafa_put14(v+n-48u+45u,128u);cafa_native_profile_draw(v,n,1);
 assert(memcmp(image,host_screen,sizeof image));
 plain=native_test_view(legacy,13u);memcpy(legacy+57,"LEGACY CHORD",12u);
 legacy[plain]=CAFA_NATIVE_KEYBOARD;cafa_put14(legacy+plain+1,20u);
 legacy[plain+3u+60u/7u]|=1u<<(60u%7u);plain+=23u;
 cafa_native_profile_draw(legacy,plain,1);memcpy(image,host_screen,sizeof image);
 legacy[plain]=CAFA_NATIVE_ROWS;cafa_put14(legacy+plain+1,49u);legacy[plain+3]=1u;
 memset(legacy+plain+4u,' ',40u);memcpy(legacy+plain+4u,"OTHER ROW",9u);
 plain+=52u;assert(cafa_native_view_valid(legacy,plain));
 cafa_native_profile_draw(legacy,plain,1);assert(!memcmp(image,host_screen,sizeof image));
 puts("Native CHORD/FOLLOW typed off, wide, strum, source-note and legacy rows render independently");
}
#if FELUCCA_CONTROLLER
static void controller_idle_device_test(void)
{
 uint16_t menu_pixel;
 ui_power_on();wire6_leave();cafa.phase=0;cafa_release_guard=0;
 cafa_idle_menu=cafa_idle_dirty=0;cafa_idle_home_t=0;
 fm1_ms=0;fm1_in.buttons=1u<<panel.btn[B_HOME];
 cafa_input();fm1_ms=701;cafa_input();
 assert(cafa_idle_menu && cafa_device_menu.active && cafa_device_menu.sel==MI_COLOR);
 assert(!mtab_rows(MTAB_CONTROL) && !mtab_rows(MTAB_AUDIO));
 assert(cafa_draw());menu_pixel=host_screen[100u*240u+8u];
 fm1_in.buttons=0;cafa_input();
 host_enc[panel.enc[EN_PRESET]]=panel.dir[EN_PRESET];cafa_input();assert(cafa_draw());
 assert(cafa_device_menu.sel==MI_LARGE); /* hidden PLAYHEAD row is skipped */
 host_enc[panel.enc[EN_ALGO]]=panel.dir[EN_ALGO];cafa_input();assert(cafa_draw());
 assert(MI_TAB[cafa_device_menu.sel]==MTAB_SYSTEM);
 fm1_in.buttons=1u<<panel.btn[B_HOME];host_pressed=fm1_in.buttons;
 cafa_input();assert(!cafa_idle_menu && !cafa_device_menu.active);
 assert(cafa_draw() && host_screen[100u*240u+8u]!=menu_pixel);
 ui.uboot=3;assert(cafa_draw());memcpy(image,host_screen,sizeof image);
 ui.uboot=2;assert(cafa_draw() && memcmp(image,host_screen,sizeof image));
 ui.uboot=0;assert(cafa_draw() && memcmp(image,host_screen,sizeof image));
 puts("Controller idle: long HOME menu, available rows, exit and recovery countdown passed");
}
#endif
int main(void) {
 transient_overlay_keeps_graph();
 uint8_t f[101],saved_tracks[sizeof trk],saved_ui[sizeof ui],saved_motion[sizeof motion],saved_fav[sizeof favorites];
 song_t saved_song;unsigned i,j;
 ui_power_on();usb.config=1;
 state(f,0,3);f[4]=CAFA_HELLO;cafa_frame(f,9);assert(cafa.phase==1 && cafa.version==2);
 assert(((sx_out_q[so_r+1]>>16)&255u)==2); /* ACK echoes the negotiated wire version */
 so_r=so_w;f[4]=CAFA_STATE;cafa_frame(f,101);assert(cafa.phase==2);so_r=so_w;
 for(i=0;i<NTRK;i++){trk[i].p[P_ATK]=i+60;trk[i].step[7].note[0]=i+48;}
 ui.page=7;ui.home=1;ui.menu=1;ui.confirm=1;ui.layer=2;ui.take_t=20;ui.hot_t=12;
 fr.on=1;motion.on=63;motion.count=1;motion.event[0].id=P_ATK;motion.event[0].place=4u<<6;
 memcpy(saved_tracks,trk,sizeof trk);memcpy(saved_ui,&ui,sizeof ui);memcpy(saved_motion,&motion,sizeof motion);
 memcpy(saved_fav,&favorites,sizeof favorites);saved_song=song;
 native_pixels=native_full_fills=0;cafa_draw();
 assert(!native_full_fills && native_pixels && cafa_native_shown);
 assert(!memcmp(saved_tracks,trk,sizeof trk) && !memcmp(saved_ui,&ui,sizeof ui));
 assert(!memcmp(saved_motion,&motion,sizeof motion) && !memcmp(saved_fav,&favorites,sizeof favorites));
 assert(!memcmp(&saved_song,&song,sizeof song) && fr.on && !cafa_native_view && tsel==&trk[song.sel]);
 memcpy(image,host_screen,sizeof image);dump("build/cafa-native-env.ppm");
 /* With no state change or animation, native rendering performs no LCD writes. */
 native_pixels=0;cafa_draw();assert(!native_pixels);
 /* Native ENV's actual ordinary renderer, supplied the equivalent model. Its
  * cards and graph must match byte-for-byte, not just resemble one another. */
 ui_power_on();sel_set(4);memset(TSEL,0,sizeof *TSEL);TSEL->p[P_SLEN]=16;
 TSEL->p[P_ATK]=10;TSEL->p[P_DEC]=48;TSEL->p[P_SUS]=75;TSEL->p[P_REL]=60;
 memset(&ui,0,sizeof ui);ui.page=0;ui.force=1;fr.on=0;memset(&motion,0,sizeof motion);
 ui_draw_page(0);
 for(j=Y_LABEL;j<foot_y();j++)for(i=0;i<240;i++)assert(image[j*240+i]==host_screen[j*240+i]);
 /* Return to linked mode, update a real ENV value and verify isolation and
  * incremental equivalence when the native hot-knob timer has settled. */
 state(f,1,3);cafa_frame(f,101);so_r=so_w;cafa_dirty=2;cafa_draw();
 memcpy(saved_tracks,trk,sizeof trk);saved_song=song;memcpy(saved_ui,&ui,sizeof ui);
 state(f,2,3);cafa_put14(f+95,127);cafa_frame(f,101);so_r=so_w;
 native_full_fills=0;cafa_draw();assert(!native_full_fills && cafa_native_cache.hot_col==1 && cafa_native_cache.hot_t);
 for(i=0;i<42;i++)cafa_draw();assert(!cafa_native_cache.hot_t);
 memcpy(image,host_screen,sizeof image);cafa_dirty=2;cafa_draw();assert(!memcmp(image,host_screen,sizeof image));
 assert(!memcmp(saved_tracks,trk,sizeof trk) && !memcmp(&saved_song,&song,sizeof song) && !memcmp(saved_ui,&ui,sizeof ui));
 /* A playing 1024-step timeline is header-only on ENV; never index a native
  * 64-step pattern with its absolute phone step. */
 state(f,3,3);f[11]=55;cafa_frame(f,101);so_r=so_w;cafa_draw();dump("build/cafa-native-env-playing.ppm");
 assert(!memcmp(saved_tracks,trk,sizeof trk));
 /* Switching back to a legacy proxy page must replace all native pixels. */
 state(f,4,0);cafa_frame(f,101);so_r=so_w;cafa_draw();assert(!cafa_native_shown);
 memcpy(image,host_screen,sizeof image);cafa_dirty=2;cafa_draw();assert(!memcmp(image,host_screen,sizeof image));
 state(f,5,3);cafa_frame(f,101);so_r=so_w;native_full_fills=0;cafa_draw();assert(cafa_native_shown && !native_full_fills);
 /* Session exit preserves untouched project data and selects the build's idle route. */
 state(f,5,3);f[4]=CAFA_BYE;cafa_frame(f,9);assert(!cafa.phase && cafa_audio_on == FELUCCA_CONTROLLER);
 assert(!memcmp(saved_tracks,trk,sizeof trk));
 printf("Native CAFA ENV: real Skiff cards/graph pixel parity, project/UI isolation, hot-knob updates, 1024-step header, page return and v2 exit passed; track projection=%zu bytes, UI cache=%zu bytes\n",sizeof(track_t),sizeof cafa_native_cache);
 palette_parity();
 loom_v4();
 loom_fixtures();
 dynamic_v5();
 native_overlay_tests();
 native_graph_tests();
 native_refresh_tests();
 native_region_tests();
 native_chrome_tests();
 native_chord_rows_tests();
#if FELUCCA_CONTROLLER
 controller_idle_device_test();
#endif
 return 0;
}
