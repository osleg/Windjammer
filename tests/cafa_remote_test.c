/* SPDX-License-Identifier: GPL-3.0-only
 * REMOTE_UI at the production CAFA mailbox and LCD/LED boundary. */
#define FELUCCA_CONTROLLER 1
#define CAFA_REMOTE_TEST 1
#include "cafa_wire6_test.c"

static void remote_send(uint8_t *f,unsigned sid,unsigned serial,unsigned n,unsigned rc)
{
    uint8_t out[256];unsigned got;
    f[0]=0x7d;f[1]=0x43;f[2]=0x41;f[3]=6;f[4]=WIRE6_REMOTE;
    cafa_put14(f+5,sid);cafa_put14(f+7,serial);
    assert(cafa_frame(f,n+9u));
    got=drain(out);
    if(rc==256u)assert(!got);
    else assert(got==13u && out[5]==WIRE6_ACK && out[10]==WIRE6_REMOTE && out[11]==rc);
}
static void remote_put_pixel(uint8_t *p,unsigned rgb)
{
    p[0]=rgb&127u;p[1]=(rgb>>7)&127u;p[2]=rgb>>14;
}
static void compact_remote(void)
{
    uint8_t f[200],out[256],*p=f+9;
    unsigned n,s=1u,i;
    header(f,WIRE6_PROBE,96,1);
    assert(cafa_frame(f,9));
    assert(drain(out)==29u && out[5]==WIRE6_OFFER && out[15]==3u &&
           out[20]==WIRE6_REMOTE_UI && out[24]==WIRE6_REMOTE_COMPACT && out[25]==1u && !out[26] && !out[27]);

    n=hello(f,96,0);f[16]=2;
    f[n++]=WIRE6_REMOTE_COMPACT;f[n++]=1;f[n++]=0;f[n++]=1;
    assert(cafa_frame(f,n));
    assert(drain(out)==13u && out[11]==WIRE6_UNSUPPORTED && !wire6.phase);
    f[n-1u]=0;
    assert(cafa_frame(f,n));
    assert(drain(out)==18u && wire6.phase==1u && !wire6_has(&wire6,WIRE6_REMOTE_COMPACT));
    header(f,WIRE6_BYE,96,0);
    assert(cafa_frame(f,9) && !wire6.phase);

    n=hello(f,97,0);f[16]=3;
    f[n++]=WIRE6_REMOTE_UI;f[n++]=1;f[n++]=0;f[n++]=1;
    f[n++]=WIRE6_REMOTE_COMPACT;f[n++]=1;f[n++]=0;f[n++]=0;
    assert(cafa_frame(f,n));
    assert(drain(out)==26u && wire6_has(&wire6,WIRE6_REMOTE_COMPACT));
    p[0]=REMOTE_TILE;p[1]=0;cafa_put14(p+2,0);
    remote_put_pixel(p+4,0xf800u);
    remote_send(f,97,s++,7u,WIRE6_OK);
    assert(remote.stage_len==1u && !remote.ready[0]);

    /* A valid literal followed by a malformed run must not change the stage. */
    p[0]=REMOTE_PACKED_TILE;cafa_put14(p+2,1);
    p[4]=1;remote_put_pixel(p+5,0x07e0u);remote_put_pixel(p+8,0x001fu);
    p[11]=127;remote_put_pixel(p+12,0xffffu);
    remote_pixels->staging[1]=0x1234u;
    remote_send(f,97,s,14u,WIRE6_INVALID);
    p[14]=4;remote_send(f,97,s,15u,WIRE6_INVALID);p[14]=3;
    assert(remote.stage_len==1u && remote_pixels->staging[1]==0x1234u);
    remote_send(f,97,s++,15u,WIRE6_OK);
    assert(remote.stage_len==67u && !remote.ready[0]);

    /* 192 pixels would overflow. Two 64-pixel runs and one 61-pixel run fit. */
    cafa_put14(p+2,67);
    for(i=4u;i<16u;i+=4u) {p[i]=127;remote_put_pixel(p+i+1u,0x1234u);}
    remote_send(f,97,s,16u,WIRE6_INVALID);
    assert(remote.stage_len==67u && !remote.ready[0]);
    p[12]=124;
    remote_send(f,97,s++,16u,WIRE6_OK);
    assert(remote.ready[0] && remote_pixels->cache[0]==0x00f8u &&
           remote_pixels->cache[1]==0xe007u && remote_pixels->cache[2]==0x1f00u &&
           remote_pixels->cache[3]==0xffffu && remote_pixels->cache[66]==0xffffu &&
           remote_pixels->cache[67]==0x3412u && remote_pixels->cache[255]==0x3412u);

    /* Invalid restaging at offset zero leaves the published tile usable. */
    cafa_put14(p+2,0);
    remote_send(f,97,s,5u,WIRE6_INVALID);
    assert(remote.ready[0]);

    p[0]=REMOTE_BLITS;p[1]=p[2]=p[3]=0;
    p[4]=1;p[5]=1;p[6]=0;
    remote_send(f,97,s,7u,WIRE6_INVALID);
    p[4]=0;p[5]=REMOTE_GRID;
    remote_send(f,97,s,7u,WIRE6_INVALID);
    assert(!remote.covered_count && !remote.dirty[0]);
    for(i=0;i<REMOTE_MAX_BLITS;i++) {
        p[1u+3u*i]=0;p[2u+3u*i]=i%REMOTE_GRID;p[3u+3u*i]=i/REMOTE_GRID;
    }
    remote_send(f,97,s,1u+3u*REMOTE_MAX_BLITS,WIRE6_OK);
    remote_send(f,97,s,1u+3u*REMOTE_MAX_BLITS,WIRE6_OK);
    assert(remote.covered_count==REMOTE_MAX_BLITS && remote.dirty[0] && remote.dirty[50] &&
           remote_pixels->framebuffer[0]==0x00f8u && remote_pixels->framebuffer[3u*16u*240u+5u*16u]==0x00f8u);
}
int main(void)
{
    uint8_t f[200],out[256];unsigned n,s=1u,off,count,i,x,y;
    ui_power_on();usb.config=1;
    song.playing=1;
    n=hello(f,94,0);f[16]=2;
    f[n++]=WIRE6_REMOTE_UI;f[n++]=1;f[n++]=0;f[n++]=1;
    /* An existing workspace user must survive a rejected Remote UI HELLO. */
    void *other=mode_pool_claim(&n,sizeof(remote_pixels_t));
    assert(other);
    assert(cafa_frame(f,n));
    assert(drain(out)==13u && out[11]==WIRE6_BUSY && !wire6.phase);
    mode_pool_release(&n);
    assert(cafa_frame(f,n));
    assert(drain(out)==22u && wire6.phase==1u && !mode_pool_claim(&n,1u));
    f[9]=REMOTE_RESET;remote_send(f,94,s++,1u,WIRE6_OK);
    f[9]=REMOTE_PACKED_TILE;remote_send(f,94,s,1u,WIRE6_UNSUPPORTED);
    f[9]=REMOTE_BLITS;remote_send(f,94,s,1u,WIRE6_UNSUPPORTED);
    memset(f+9,0,34);f[9]=REMOTE_PRESENT;remote_send(f,94,s++,34u,WIRE6_INVALID);
    assert(wire6.phase==1u && song.playing);
    for(off=0;off<256u;off+=count) {
        count=256u-off<50u?256u-off:50u;
        f[9]=REMOTE_TILE;f[10]=0;cafa_put14(f+11,off);
        for(i=0;i<count;i++)remote_put_pixel(f+13u+3u*i,0xf800u);
        remote_send(f,94,s++,4u+3u*count,WIRE6_OK);
    }
    assert(remote.ready[0]);
    for(y=0;y<15u;y++)for(x=0;x<15u;x++) {
        f[9]=REMOTE_BLIT;f[10]=0;f[11]=(uint8_t)x;f[12]=(uint8_t)y;
        remote_send(f,94,s++,4u,WIRE6_OK);
    }
    assert(remote.covered_count==225u && host_screen[0]!=0x00f8u);
    memset(f+9,0,34);f[9]=REMOTE_PRESENT;wire6_put28(f+10,0x12345u);
    remote_send(f,94,s,34u,256u);
    assert(wire6.phase==1u && song.playing);
    f[10]^=1u;remote_send(f,94,s,34u,WIRE6_INVALID);f[10]^=1u;
    for(i=0;i<=225u;i++)cafa_part_ready();
    assert(drain(out)==13u && out[10]==WIRE6_REMOTE && out[11]==WIRE6_OK);
    assert(wire6.phase==2u && cafa_audio_on && host_screen[0]==0x00f8u &&
           host_screen[239u*240u+239u]==0x00f8u && wire6.drawn_context==0x12345u);
    remote_send(f,94,s,34u,WIRE6_OK);
    /* Unplug during a new presentation and immediately reuse the same memory. */
    f[9]=REMOTE_BLIT;f[10]=f[11]=f[12]=0;
    remote_send(f,94,++s,4u,WIRE6_OK);
    memset(f+9,0,34);f[9]=REMOTE_PRESENT;
    remote_send(f,94,++s,34u,256u);
    cafa_part_ready();
    usb.config=0;
    cafa_input();
    assert(!wire6.phase && mode_pool_claim(&n,sizeof(remote_pixels_t))==other);
    cafa_part_ready();
    assert(!drain(out));
    mode_pool_release(&n);

    /* Native connections leave the workspace available. */
    usb.config=1;n=hello(f,95,0);
    assert(cafa_frame(f,n));
    assert(wire6.phase==1u && mode_pool_claim(&n,sizeof(remote_pixels_t))==other);
    mode_pool_release(&n);
    header(f,WIRE6_BYE,95,0);
    assert(cafa_frame(f,9) && !wire6.phase);
    drain(out);
    compact_remote();
    puts("REMOTE_UI: recovery, tile staging, atomic compact batches, bounded LCD and first takeover passed");
    return 0;
}
