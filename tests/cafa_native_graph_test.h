/* SPDX-License-Identifier: GPL-3.0-only
 * Published app data must reach the native painter for every page family. */
static uint32_t native_graph_section(uint8_t *v,uint32_t screen,uint32_t *change)
{
    uint32_t at=256u,tag,count=1u,len,k;
    uint8_t *p;
    if(screen==5u) {
        p=v+112;memcpy(p,"SEND        ",12);memcpy(p+12,"50            ",14);
        p[30]=1;p[31]=60;p[32]=1;p[33]=1;*change=at;return at;
    }
    if(screen==15u || screen==16u)v[7]=CAFA_NATIVE_VARIANT_ANALOG;
    if(screen==12u || screen==13u || screen==14u)tag=6u;
    else if(screen==10u || screen==35u || screen==38u || screen==40u || screen==41u || screen==43u ||
            screen==44u || screen==46u || screen==67u)tag=5u;
    else if(screen==42u)tag=2u;
    else if(screen==0u || screen==1u || screen==2u || screen==3u || screen==6u || screen==7u ||
            screen==8u || screen==9u || screen==11u || screen==15u || screen==16u || screen==22u ||
            screen==29u ||
            screen==23u || (screen>=24u && screen<=27u) ||
            (screen>=32u && screen<=34u) || screen==48u)tag=3u;
    else tag=1u;
    v[at]=(uint8_t)tag;
    if(tag==6u) {
        cafa_put14(v+at+1,20u);p=v+at+3u;memset(p,0,20);p[60u/7u]=1u<<(60u%7u);p[19]=60;
        *change=at+3u+60u/7u;return at+23u;
    }
    if(tag==3u)count=2u;
    len=(tag==1u?1u:2u)+count*(tag==1u?48u:tag==2u?7u:tag==3u?4u:7u);
    cafa_put14(v+at+1,len);p=v+at+3u;
    if(tag==1u) {
        p[0]=1;memset(p+1,' ',40);memcpy(p+1,"ALPHA",5);memcpy(p+17,"VALUE",5);
        memcpy(p+33,"TAG",3);p[41]=3u;cafa_put14(p+42,12000u);
        *change=at+4u;
    } else if(tag==2u) {
        cafa_put14(p,1);p[2]=60;cafa_put14(p+3,0);cafa_put14(p+5,128);p[7]=1;p[8]=100;
        *change=at+5u;
    } else if(tag==3u) {
        cafa_put14(p,2);cafa_put14(p+2,0);cafa_put14(p+4,1000);
        cafa_put14(p+6,16383);cafa_put14(p+8,5000);
        *change=at+11u;
    } else {
        cafa_put14(p,1);cafa_put14(p+2,12000);p[4]=1u;
        for(k=0;k<4u;k++)p[5u+k]=' ';
        memcpy(p+5,"A001",4);*change=at+5u;
    }
    return at+3u+len;
}
static void native_graph_tests(void)
{
    uint8_t v[3072];uint8_t before[sizeof host_screen];uint32_t s,n,change,failures=0;
    song_t local_song;uint8_t local_tracks[sizeof trk],local_ui[sizeof ui];
    ui_power_on();
    for(s=0;s<=48u;s++) {
        n=native_test_view(v,s);n=native_graph_section(v,s,&change);
        if(s==17u){
            cafa_put14(v+20,0u);cafa_put14(v+24,0u);cafa_put14(v+28,1u);
            memcpy(v+260u,"A               ",16u);
            memcpy(v+276u,"1:1             ",16u);
        }
        assert(cafa_native_view_valid(v,n));
        lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
        memcpy(before,host_screen,sizeof before);
        if(s==5u)v[112+31]=110u;
        else if(v[256]==6u)v[change]^=1u<<(60u%7u);
        else if(v[256]==3u)cafa_put14(v+change,14000u);
        else if(v[256]==2u)v[change]=72u;
        else if(v[256]==5u){cafa_put14(v+change,1000u);v[change+2u]^=17u;}
        else if(s==30u)v[259u+41u]^=2u;
        else v[change]='B';
        assert(cafa_native_view_valid(v,n));
        lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
        if(!memcmp(before,host_screen,sizeof before)) {
            fprintf(stderr,"Native screen %u ignored its app section\n",s);failures++;
        }
        memcpy(before,host_screen,sizeof before);
        sel_set(3);song.playing=1;song.rec=8;song.g[G_BPM]=213;
        trk[3].p[P_SCALE]=5;trk[3].step[0].note[0]=72;
        ui.page=13;ui.home=1;ui.lane=5;ui.msg_t=2;strcpy(ui.msg,"LOCAL SECRET");
        local_song=song;memcpy(local_tracks,trk,sizeof trk);memcpy(local_ui,&ui,sizeof ui);
        lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
        assert(!memcmp(before,host_screen,sizeof before));
        assert(!memcmp(&local_song,&song,sizeof song));
        assert(!memcmp(local_tracks,trk,sizeof trk));assert(!memcmp(local_ui,&ui,sizeof ui));
    }
    /* EDIT pages have engine-specific native bodies. Exercise each published
     * variant with the section its painter actually consumes. */
    for(s=15u;s<=16u;s++) {
        uint32_t variant;
        for(variant=0u;variant<=6u;variant++) {
            if(variant==CAFA_NATIVE_VARIANT_DRUM)continue;
            n=native_test_view(v,s);v[7]=(uint8_t)variant;
            n=native_graph_section(v,(variant==CAFA_NATIVE_VARIANT_FM_OPERATOR ||
                                      variant==CAFA_NATIVE_VARIANT_DRUM_SOUND ||
                                      variant==CAFA_NATIVE_VARIANT_DRUM_LANES)?28u:0u,&change);
            v[7]=(uint8_t)variant;
            assert(cafa_native_view_valid(v,n));
            lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
            memcpy(before,host_screen,sizeof before);
            if(v[256]==3u)cafa_put14(v+change,14000u);
            else v[change]='B';
            assert(cafa_native_view_valid(v,n));
            lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
            if(!memcmp(before,host_screen,sizeof before))
                fprintf(stderr,"Native EDIT screen %u variant %u ignored app data\n",s,variant);
            assert(memcmp(before,host_screen,sizeof before));
        }
    }
    n=native_test_view(v,42u);v[7]=CAFA_NATIVE_VARIANT_DRUM;
    v[n]=CAFA_NATIVE_CELLS;cafa_put14(v+n+1,2u+128u*7u);
    cafa_put14(v+n+3,128u);n+=5u+128u*7u;
    for(s=0;s<128u;s++)memset(v+261u+s*7u+3u,' ',4u);
    assert(cafa_native_view_valid(v,n));
    lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
    memcpy(before,host_screen,sizeof before);
    v[256u+5u+2u]|=1u;
    lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
    assert(memcmp(before,host_screen,sizeof before));
    for(s=31u;s<=47u;s+=16u) {
        uint32_t i,at;
        n=native_test_view(v,s);cafa_put14(v+26,2u);cafa_put14(v+28,8u);cafa_put14(v+24,7u);
        at=n;v[at]=CAFA_NATIVE_ROWS;cafa_put14(v+at+1,1u+6u*48u);v[at+3]=6u;
        for(i=0;i<6u;i++) {
            uint8_t *r=v+at+4u+i*48u;
            memset(r,' ',40);r[0]='T';r[1]=(uint8_t)('3'+i);
            memcpy(r+32,"ENGINE  ",8);r[40]=(i==5u)?1u:0u;
            cafa_put14(r+41,s==47u?0u:12000u);cafa_put14(r+43,s==47u?16383u:8192u);
            cafa_put14(r+45,2u+i);r[47]=0u;
        }
        n+=4u+6u*48u;
        if(s==47u) {
            at=n;v[at]=CAFA_NATIVE_CELLS;cafa_put14(v+at+1,2u+96u*7u);
            cafa_put14(v+at+3,96u);n+=5u+96u*7u;
            for(i=0;i<96u;i++)memset(v+at+5u+i*7u+3u,' ',4u);
        }
        assert(cafa_native_view_valid(v,n));
        lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
        memcpy(before,host_screen,sizeof before);
        if(s==47u)v[at+5u+5u*16u*7u+2u]=1u;
        else v[256u+4u+5u*48u]='X';
        lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
        assert(memcmp(before,host_screen,sizeof before));
    }
    ui_power_on();
    ui_prefs|=PREF_LARGE;
    n=native_test_view(v,CAFA_SCREEN_PATTERN);
    cafa_put14(v+17,192u);v[19]=16u;
    v[n]=CAFA_NATIVE_CELLS;cafa_put14(v+n+1,2u+64u*7u);
    cafa_put14(v+n+3,64u);
    for(s=0;s<64u;s++) {
        cafa_put14(v+n+5u+s*7u,192u+s);
        memset(v+n+5u+s*7u+3u,' ',4u);
    }
    change=n+5u+63u*7u+2u;
    n+=5u+64u*7u;
    assert(cafa_native_view_valid(v,n));
    cafa_native_profile_draw(v,n,1);memcpy(before,host_screen,sizeof before);
    v[change]=1u;
    cafa_native_profile_draw(v,n,1);
    assert(memcmp(before,host_screen,sizeof before)); /* Step 256 is visible in the fourth row. */
    memcpy(before,host_screen,sizeof before);
    cafa_put14(v+20,255u);
    cafa_native_profile_draw(v,n,1);
    assert(memcmp(before,host_screen,sizeof before)); /* The absolute cursor reaches the last visible cell. */

    /* A short last window contains only track 20. Its band and footer use
     * the absolute identity even though its visible position is zero. */
    palette_set(UI_WINDJAMMER_INDEX);
    n=native_test_view(v,CAFA_SCREEN_HOME);
    cafa_put14(v+13,19u);cafa_put14(v+15,20u);
    cafa_put14(v+24,19u);cafa_put14(v+26,19u);cafa_put14(v+28,20u);
    n=native_graph_section(v,CAFA_SCREEN_HOME,&change);
    v[n]=CAFA_NATIVE_CHROME;cafa_put14(v+n+1,56u);
    cafa_put14(v+n+3,16383u);
    memset(v+n+6,' ',9u);memset(v+n+16,' ',40u);
    memcpy(v+n+32,"ENGINE",6u);
    n+=59u;
    assert(cafa_native_view_valid(v,n));
    cafa_native_profile_draw(v,n,1);
    assert(host_screen[(LG_Y_GRAPH+3u)*240u+4u]==swap16(T_TRK(19u)));
    assert(host_screen[(LG_Y_GRAPH+3u)*240u+122u]==swap16(T_BG));
    memcpy(before,host_screen,sizeof before);
    cafa_put14(v+13,1u);cafa_put14(v+24,1u);cafa_put14(v+26,1u);
    cafa_native_profile_draw(v,n,1);
    change=foot_y()*240u*sizeof host_screen[0];
    assert(memcmp(before+change,(const uint8_t *)host_screen+change,sizeof before-change));

    /* SONG's row window accompanies the cells while the header still carries
     * the selected entry window. A selected absolute track gets the band. */
    n=native_test_view(v,CAFA_SCREEN_SONG);
    cafa_put14(v+13,19u);cafa_put14(v+15,20u);
    n=native_graph_section(v,CAFA_SCREEN_HOME,&change);
    cafa_put14(v+change+41u,19u);
    uint8_t *song_row = v + change;
    v[n]=CAFA_NATIVE_CELLS;cafa_put14(v+n+1,2u+16u*7u);
    cafa_put14(v+n+3,16u);
    for(s=0;s<16u;s++)memset(v+n+5u+s*7u+3u,' ',4u);
    uint8_t *preview_cell = v + n + 5u + 3u*7u;
    preview_cell[2] = 1u;
    memcpy(preview_cell + 3u, "02  ", 4u);
    n+=5u+16u*7u;
    assert(cafa_native_view_valid(v,n));
    cafa_native_profile_draw(v,n,1);
    assert(host_screen[(Y_GRAPH+5u)*240u+4u]==swap16(T_TRK(19u)));
    assert(host_screen[(Y_GRAPH+27u)*240u+4u]==swap16(T_BG));
    change = (Y_GRAPH + SG_Y(0) + 1u)*240u + SG_X(3);
    assert(host_screen[change] == swap16(T_TRK(19u)));
    cafa_put14(song_row + 43u, 3u); cafa_put14(song_row + 45u, 1u);
    cafa_native_profile_draw(v,n,1);
    assert(host_screen[change] == swap16(T_TEXT));
    assert(!failures);
    puts("Native graph pages: 47 descriptors plus HOME/SCOPE respond to typed app data and ignore poisoned local music");
}
