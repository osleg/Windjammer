/* SPDX-License-Identifier: GPL-3.0-only
 * Owning boundary: real native LCD output and captured device-menu input.
 * A second generic renderer, leaked standalone data, or simultaneous local and
 * remote input ownership must fail here even if a byte catalogue still passes. */
static uint32_t native_test_view(uint8_t *v, uint32_t screen)
{
    uint32_t k; int family=cafa_native_family(screen);
    memset(v,0,3072);v[0]=7;v[6]=(uint8_t)screen;v[8]=(uint8_t)(family<0?6:family);
    cafa_put14(v+2,27);cafa_put14(v+11,1);cafa_put14(v+15,8);
    cafa_put14(v+20,16383);cafa_put14(v+22,16383);cafa_put14(v+24,16383);
    memset(v+33,' ',79);
    for(k=0;k<4;k++)memset(v+112+36*k,' ',30);
    if((screen>=57&&screen<=62)||screen==70||screen==71)v[1]=8;
    return 256;
}
static uint32_t native_test_lines(uint8_t *v,uint32_t at,const char *const *lines,uint32_t count)
{
    uint32_t k;v[at]=7;cafa_put14(v+at+1,1+24*count);v[at+3]=(uint8_t)count;
    for(k=0;k<count;k++) {
        uint32_t n=(uint32_t)strlen(lines[k]);if(n>24)n=24;
        memset(v+at+4+24*k,' ',24);memcpy(v+at+4+24*k,lines[k],n);
    }
    return at+4+24*count;
}
static void native_overlay_tests(void)
{
    uint8_t v[3072];uint32_t n,pal,x,y,s;
    static const char *const lines[]={"HELLO","UNTITLED"};
    static const uint8_t screens[]={49,50,51,52,53,54,55,56,63,64,65,66,68,69};
    {
        uint32_t changed=0;
        ui_power_on();n=native_test_view(v,0);
        cafa_native_profile_draw(v,n,1);memcpy(image,host_screen,sizeof image);
        memcpy(v+81,"APP FOOTER",10);
        cafa_native_profile_draw(v,n,1);
        for(y=foot_y();y<240;y++)for(x=0;x<240;x++)
            changed|=image[y*240+x]!=host_screen[y*240+x];
        assert(changed); /* App status must be visible inside the native footer. */
    }
    for(pal=0;pal<NPALETTES;pal++) {
        ui_power_on();palette_set(pal);cafa_native_profile_reset();
        n=native_test_view(v,64);memcpy(v+33,"SAVE U01",8);cafa_put14(v+20,3);
        n=native_test_lines(v,n,lines,2);assert(cafa_native_view_valid(v,n));
        lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
        memcpy(image,host_screen,sizeof image);
        /* Independent equivalent standalone NAME state: compare the actual
         * field, keyboard and footer painters, not a screenshot approximation. */
        memset(&nm,0,sizeof nm);nm.kind=NK_USER_SAVE;nm.len=5;nm.cur=3;
        strcpy(nm.s,"HELLO");strcpy(nm.ph,"UNTITLED");
        lcd_fill(0,0,240,240,T_BG);nm_draw_field();nm_draw_panel();nm_draw_foot();
        for(y=Y_LABEL;y<Y_LABEL+CARD_H;y++)for(x=0;x<240;x++)assert(image[y*240+x]==host_screen[y*240+x]);
        for(y=Y_GRAPH;y<Y_GRAPH+H_GRAPH;y++)for(x=0;x<240;x++)assert(image[y*240+x]==host_screen[y*240+x]);
        for(y=Y_FOOT;y<240;y++)for(x=0;x<240;x++)assert(image[y*240+x]==host_screen[y*240+x]);
    }
    for(s=0;s<sizeof screens;s++) {
        song_t saved_song;uint8_t tracks[sizeof trk],local_ui[sizeof ui];
        ui_power_on();n=native_test_view(v,screens[s]);assert(cafa_native_view_valid(v,n));
        lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);memcpy(image,host_screen,sizeof image);
        /* Deliberately unrelated local music. A remote empty window stays empty. */
        sel_set(3);song.playing=1;song.rec=8;song.g[G_BPM]=213;song.octave=2;
        trk[3].p[P_SCALE]=5;trk[3].p[P_MUTE]=1;trk[3].step[0].note[0]=72;
        ui.lane=5;ui.page=12;ui.home=1;ui.msg_t=3;strcpy(ui.msg,"LOCAL SECRET");
        saved_song=song;memcpy(tracks,trk,sizeof trk);memcpy(local_ui,&ui,sizeof ui);
        lcd_fill(0,0,240,240,T_BG);cafa_native_profile_draw(v,n,1);
        assert(!memcmp(image,host_screen,sizeof image));
        assert(!memcmp(&saved_song,&song,sizeof song));assert(!memcmp(tracks,trk,sizeof trk));assert(!memcmp(local_ui,&ui,sizeof ui));
    }
    {
        song_t saved_song;uint8_t tracks[sizeof trk],local_ui[sizeof ui];
        uint32_t buttons=0,keys=7;int32_t turns[7]={0};
        ui_power_on();cafa_native_profile_reset();n=native_test_view(v,57);
        cafa_native_profile_draw(v,n,1);assert(cafa_device_menu.active);
        cafa_device_menu.sel=MI_LARGE;
        saved_song=song;memcpy(tracks,trk,sizeof trk);memcpy(local_ui,&ui,sizeof ui);
        ui_prefs&=(uint8_t)~PREF_LARGE;turns[EN_K1]=1;
        assert(cafa_native_device_input(&buttons,&keys,turns)==1);
        assert(ui_prefs&PREF_LARGE);assert(!buttons&&!keys&&!turns[EN_K1]);
        assert(!memcmp(tracks,trk,sizeof trk)&&!memcmp(&saved_song,&song,sizeof song)&&!memcmp(local_ui,&ui,sizeof ui));
        buttons=1u<<B_PLAY;assert(cafa_native_device_input(&buttons,&keys,turns)==1);
        assert(buttons==(1u<<B_PLAY)); /* transport remains application-owned */
        buttons=1u<<B_HOME;assert(cafa_native_device_input(&buttons,&keys,turns)==2);
        assert(!cafa_device_menu.active);
        cafa_native_profile_draw(v,n,1);assert(!cafa_device_menu.active); /* stale publication must not reopen */
        cafa_native_profile_reset();ui_prefs&=(uint8_t)~PREF_LARGE;
    }
    puts("Native overlays: NAME palette pixel parity, app/local data isolation, device menu input ownership and stale-open guard passed");
}
