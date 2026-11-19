/* SPDX-License-Identifier: GPL-3.0-only
 * Open companion wire-6 codec. Frames here exclude F0 and F7. */
#ifndef CAFA_WIRE6_H
#define CAFA_WIRE6_H
#include <stdint.h>
/* Memory primitives are supplied by firmware libc.c (or the host harness). */
#include "cafa_native_view.h"
#define WIRE6_LEASE_MS 1500u
#define WIRE6_MAX_BODY 156u
#define WIRE6_MAX_HELLO 101u
#ifndef WIRE6_AUDIO_MODES
#define WIRE6_AUDIO_MODES 3u
#endif
enum { WIRE6_HELLO=1, WIRE6_ACK, WIRE6_STATE, WIRE6_HEARTBEAT, WIRE6_BYE,
       WIRE6_INPUT, WIRE6_PROBE, WIRE6_OFFER, WIRE6_VIEW_PART, WIRE6_VIEW_DELTA, WIRE6_DIAG,
       WIRE6_PART_READY };
enum { WIRE6_OK=0, WIRE6_UNSUPPORTED, WIRE6_INVALID, WIRE6_BUSY };
enum { WIRE6_NATIVE_UI=9, WIRE6_NATIVE_KEEPALIVE=11, WIRE6_NATIVE_DELTA=12, WIRE6_NATIVE_PANEL=13, WIRE6_PANEL_MIDI=14, WIRE6_NATIVE_CHROME=15 };
static const uint8_t wire6_capabilities[] = {1,2,3,WIRE6_NATIVE_UI,WIRE6_NATIVE_KEEPALIVE,WIRE6_NATIVE_DELTA,WIRE6_NATIVE_PANEL
#if FELUCCA_CONTROLLER && FELUCCA_UART && !BOARD_SMK37
    ,WIRE6_PANEL_MIDI
#endif
};
enum { WIRE6_NONE=0, WIRE6_SEND_OFFER, WIRE6_SEND_ACK, WIRE6_FIRST_STATE,
       WIRE6_NEW_STATE, WIRE6_CLOSE, WIRE6_SEND_PART_READY };
typedef struct {
    uint32_t last_ms, drawn_context;
    uint16_t session, revision;
    uint32_t stage_ms;
    uint16_t view_len, stage_len, stage_total, stage_revision;
    uint16_t stage_last_offset, stage_last_size;
    uint8_t phase, audio_mode, accepted_count, hello_len, core_minor;
    uint8_t diagnostic;
    uint8_t accepted[8], hello[WIRE6_MAX_HELLO];
    uint8_t *view, *stage;
} wire6_link_t;
typedef struct {
    uint8_t action, command, result, renew;
    uint16_t session, serial;
} wire6_reply_t;
static uint32_t wire6_u14(const uint8_t *p) { return p[0]|(uint32_t)p[1]<<7; }
static uint32_t wire6_u28(const uint8_t *p) {
    return p[0]|(uint32_t)p[1]<<7|(uint32_t)p[2]<<14|(uint32_t)p[3]<<21;
}
static void wire6_put28(uint8_t *p,uint32_t v) {
    unsigned k;for(k=0;k<4;k++)p[k]=(uint8_t)((v>>(7*k))&127u);
}
static int wire6_newer(uint32_t a,uint32_t b) {
    uint32_t d=(a-b)&16383u;return d && d<8192u;
}
static int wire6_expired(const wire6_link_t *c,uint32_t now) {
    return c->phase && now-c->last_ms>=WIRE6_LEASE_MS;
}
static int wire6_text(const uint8_t *p,uint32_t n) {
    uint32_t i;for(i=0;i<n;i++)if(p[i]<32u || p[i]>126u)return 0;return 1;
}
static int wire6_has(const wire6_link_t *c,uint8_t id) {
    uint32_t i;for(i=0;i<c->accepted_count;i++)if(c->accepted[i]==id)return 1;return 0;
}
static int wire6_view_valid(const uint8_t *p,uint32_t n,uint8_t caps) {
    uint32_t i,k;
    if(n<34u || p[0]<1u || p[0]>4u || p[1]>3u ||
       !wire6_text(p+6,28u))return 0;
    if(p[0]==1u) {
        if(n!=138u || !wire6_u14(p+36) || !wire6_u14(p+40) ||
           wire6_u14(p+34)>=wire6_u14(p+36) || wire6_u14(p+38)>=wire6_u14(p+40))return 0;
        for(k=0;k<4;k++) {
            const uint8_t *q=p+42+24*k;
            if(!wire6_text(q,20u) || wire6_u14(q+20)>16383u || q[22]>4u || q[23]>1u)return 0;
            if(!q[22]) {
                if(q[20] || q[21] || q[23])return 0;
                for(i=0;i<20;i++)if(q[i]!=' ')return 0;
            }
        }
    } else if(p[0]==2u) {
        if(n!=42u || p[34]>3u || p[41]>2u ||
           (!p[41] && wire6_u28(p+37)) ||
           (!(caps&2u) && (p[34] || p[35] || p[36] || p[37] || p[38] || p[39] || p[40] || p[41])))return 0;
    } else if(p[0]==3u) {
        if(n!=35u || p[34]>7u)return 0;
    } else {
        uint32_t selected,total,first,visible,controls,flags;
        const uint8_t *q;
        if(n!=138u || !(caps&4u))return 0;
        selected=wire6_u14(p+34);total=wire6_u14(p+36);
        first=wire6_u14(p+38);visible=p[40];controls=p[132];
        if(!total || selected>=total || first>=total || (first&7u) ||
           visible!=(total-first<8u?total-first:8u) ||
           selected<first || selected>=first+visible || p[41]>1u || controls>15u)return 0;
        for(i=0;i<8u;i++) {
            q=p+42u+8u*i;flags=q[7];
            if(!wire6_text(q,5u) || flags>63u ||
               ((flags&1u) && !(flags&16u)) || ((flags&2u) && !(flags&32u)) ||
               (!(flags&8u) && wire6_u14(q+5)))return 0;
            if(i>=visible) {
                for(k=0;k<5u;k++)if(q[k]!=' ')return 0;
                if(wire6_u14(q+5) || flags)return 0;
            }
        }
        flags=p[42u+8u*(selected-first)+7u];
        if(((controls&1u)!=0u)!=((flags&8u)!=0u) ||
           ((controls&4u)!=0u)!=((flags&16u)!=0u) ||
           ((controls&8u)!=0u)!=((flags&32u)!=0u) ||
           !wire6_text(p+106,24u) ||
           (!(controls&1u) && memcmp(p+106,"            ",12u)) ||
           (!(controls&2u) && (memcmp(p+118,"            ",12u) || wire6_u14(p+130))))return 0;
        for(i=133u;i<138u;i++)if(p[i])return 0;
    }
    return 1;
}
/* An identified, malformed message returns an INVALID ACK without mutating the
 * owner. Unidentified/truncated traffic is ignored. PROBE never owns a lease. */
static wire6_reply_t wire6_receive(wire6_link_t *c,const uint8_t *f,uint32_t n,uint32_t now)
{
    wire6_reply_t r={0};uint32_t i,sid,serial,count,off,accepted=0;
    const uint8_t *p;
    if(n<9u || f[0]!=0x7d || f[1]!=0x43 || f[2]!=0x41 || f[3]!=6u)return r;
    for(i=0;i<9u;i++)if(f[i]&128u)return r;
    sid=wire6_u14(f+5);serial=wire6_u14(f+7);p=f+9;
    if(!sid)return r;
    r.session=(uint16_t)sid;r.serial=(uint16_t)serial;r.command=f[4];r.result=WIRE6_INVALID;
    if(n>9u+WIRE6_MAX_BODY) {
        if(f[4]==WIRE6_VIEW_DELTA && c->phase && c->session==sid)
            r.action=WIRE6_SEND_ACK;
        return r;
    }
    for(i=9u;i<n;i++)if(f[i]&128u) {
        if(f[4]==WIRE6_VIEW_DELTA && c->phase && c->session==sid)
            r.action=WIRE6_SEND_ACK;
        return r;
    }
    if(f[4]==WIRE6_PROBE && n==9u && serial<=1u) {r.action=WIRE6_SEND_OFFER;return r;}
    if(f[4]==WIRE6_HELLO) {
        r.action=WIRE6_SEND_ACK;
        if(serial || n<14u || n>9u+WIRE6_MAX_HELLO)return r;
        if(c->phase) {
            if(c->session!=sid)r.result=WIRE6_BUSY;
            else if(c->hello_len==n-9u && !memcmp(c->hello,p,c->hello_len))r.result=WIRE6_OK;
            return r;
        }
        if(p[3]<1u || p[3]>64u || n<14u+p[3])return r;
        if(!wire6_text(p+4,p[3]))return r;
        for(i=0;i<p[3];i++)if(p[4+i]==' ')return r;
        off=4u+p[3];count=p[off++];
        if(count>8u || n!=9u+off+4u*count || p[2]>1u)return r;
        if(p[0]!=1u || !(WIRE6_AUDIO_MODES&(1u<<p[2]))) {r.result=WIRE6_UNSUPPORTED;return r;}
        { uint8_t seen[128]={0};uint32_t required_instrument=0,keepalive_required=0,delta_required=0,panel_required=0,chrome_required=0;
          for(i=0;i<count;i++) {
              const uint8_t *q=p+off+4*i;
              if(!q[0] || q[3]>1u || seen[q[0]])return r;
              seen[q[0]]=1;
              if(q[0]==1u && (q[3]&1u))required_instrument=1;
              if(q[0]==WIRE6_NATIVE_KEEPALIVE)keepalive_required=q[3];
              if(q[0]==WIRE6_NATIVE_DELTA)delta_required=q[3];
              if(q[0]==WIRE6_NATIVE_PANEL)panel_required=q[3];
              if(q[0]==WIRE6_NATIVE_CHROME)chrome_required=q[3];
              if((q[0]<=3u || q[0]==WIRE6_NATIVE_UI || q[0]==WIRE6_NATIVE_KEEPALIVE ||
                  q[0]==WIRE6_NATIVE_DELTA || q[0]==WIRE6_NATIVE_PANEL || q[0]==WIRE6_NATIVE_CHROME
#if FELUCCA_CONTROLLER && FELUCCA_UART && !BOARD_SMK37
                  || q[0]==WIRE6_PANEL_MIDI
#endif
                 ) &&
                 q[1]==1u && q[2]==0u)accepted|=1u<<(q[0]-1u);
              else if(q[3]&1u){r.result=WIRE6_UNSUPPORTED;return r;}
          }
          if(!required_instrument || !(accepted&1u)) {r.result=WIRE6_UNSUPPORTED;return r;}
          if(!(accepted&(1u<<(WIRE6_NATIVE_UI-1)))) {
              if(keepalive_required || panel_required){r.result=WIRE6_UNSUPPORTED;return r;}
              accepted&=~((1u<<(WIRE6_NATIVE_KEEPALIVE-1))|(1u<<(WIRE6_NATIVE_PANEL-1)));
          }
          if((accepted&((1u<<(WIRE6_NATIVE_UI-1))|(1u<<(WIRE6_NATIVE_KEEPALIVE-1)))) !=
             ((1u<<(WIRE6_NATIVE_UI-1))|(1u<<(WIRE6_NATIVE_KEEPALIVE-1)))) {
              if(delta_required){r.result=WIRE6_UNSUPPORTED;return r;}
              accepted&=~(1u<<(WIRE6_NATIVE_DELTA-1));
          }
          if(!(accepted&(1u<<(WIRE6_NATIVE_PANEL-1)))) {
              if(chrome_required){r.result=WIRE6_UNSUPPORTED;return r;}
              accepted&=~(1u<<(WIRE6_NATIVE_CHROME-1));
          }
        }
        c->session=(uint16_t)sid;c->phase=1;c->audio_mode=p[2];
        c->diagnostic=(uint8_t)(p[3]==21u && !memcmp(p+4,"org.fm1.skiff.keytime",21u) ? 1u :
            p[3]==22u && !memcmp(p+4,"org.fm1.skiff.keytime2",22u) ? 2u : 0u);
        c->last_ms=now;c->hello_len=(uint8_t)(n-9u);memcpy(c->hello,p,c->hello_len);
        c->core_minor=p[1];
        c->accepted_count=0;
        for(i=0;i<sizeof wire6_capabilities;i++) {
            uint8_t id=wire6_capabilities[i];
            if(accepted&(1u<<(id-1u)))c->accepted[c->accepted_count++]=id;
        }
        if(accepted&(1u<<(WIRE6_NATIVE_CHROME-1)))c->accepted[c->accepted_count++]=WIRE6_NATIVE_CHROME;
        r.result=WIRE6_OK;return r;
    }
    if(!c->phase || c->session!=sid)return r;
    if(f[4]==WIRE6_DIAG) {
        r.action=WIRE6_SEND_ACK;
#if FELUCCA_CONTROLLER
        if(!c->diagnostic || c->phase!=2 || c->audio_mode!=1u ||
           !wire6_has(c,WIRE6_NATIVE_PANEL)) {r.result=WIRE6_UNSUPPORTED;return r;}
        if(n==10u && p[0]>=1u && p[0]<=3u)r.result=WIRE6_OK;
#else
        r.result=WIRE6_UNSUPPORTED;
#endif
        return r;
    }
    if(f[4]==WIRE6_BYE && n==9u) {r.action=WIRE6_CLOSE;return r;}
    if(f[4]==WIRE6_HEARTBEAT && n==9u) {
        c->last_ms=now;r.action=WIRE6_SEND_ACK;r.result=WIRE6_OK;r.renew=1;return r;
    }
    if(f[4]==WIRE6_VIEW_PART) {
        uint32_t total,offset,size;
        r.action=0;
        if(!wire6_has(c,9u) || !c->view || !c->stage || n<14u)return r;
        total=wire6_u14(p);offset=wire6_u14(p+2);size=n-13u;
        if(total<CAFA_NATIVE_HEADER || total>CAFA_NATIVE_MAX ||
           offset>=total || offset+size>total || size>152u)return r;
        if(c->phase==2 && serial==c->revision) {
            if(offset+size<=c->view_len && c->view_len==total &&
               !memcmp(c->view+offset,p+4,size)) {r.result=WIRE6_OK;r.action=WIRE6_SEND_ACK;}
            return r;
        }
        if(c->phase==2 && !wire6_newer(serial,c->revision))return r;
        if(c->stage_len && serial!=c->stage_revision) {
            if(!offset && wire6_newer(serial,c->stage_revision))c->stage_len=0;
            else return r;
        }
        if(!c->stage_len) {
            if(offset)return r;
            c->stage_revision=(uint16_t)serial;c->stage_total=(uint16_t)total;
        }
        if(total!=c->stage_total || serial!=c->stage_revision)return r;
        if(offset<c->stage_len) {
            /* Reissue credit for the last accepted part if its response was
             * lost. Older parts cannot advance the sender past an unread one. */
            if(c->core_minor && wire6_has(c,WIRE6_NATIVE_UI) &&
               offset==c->stage_last_offset && size==c->stage_last_size &&
               offset+size==c->stage_len && !memcmp(c->stage+offset,p+4,size))
                r.action=WIRE6_SEND_PART_READY;
            return r;
        }
        if(offset!=c->stage_len)return r;
        memcpy(c->stage+offset,p+4,size);c->stage_len=(uint16_t)(offset+size);
        c->stage_last_offset=(uint16_t)offset;c->stage_last_size=(uint16_t)size;
        c->last_ms=now;c->stage_ms=now;r.renew=1;
        if(c->stage_len!=total) {
            if(c->core_minor && wire6_has(c,WIRE6_NATIVE_UI))r.action=WIRE6_SEND_PART_READY;
            return r;
        }
        if(!cafa_native_view_for_caps(c->stage,total,wire6_has(c,WIRE6_NATIVE_PANEL),wire6_has(c,WIRE6_NATIVE_CHROME))) {c->stage_len=0;return r;}
        {uint8_t *swap=c->view;c->view=c->stage;c->stage=swap;}
        c->view_len=(uint16_t)total;c->stage_len=0;c->revision=(uint16_t)serial;
        r.result=WIRE6_OK;r.action=c->phase==1?WIRE6_FIRST_STATE:WIRE6_NEW_STATE;
        c->phase=2;return r;
    }
    if(f[4]==WIRE6_VIEW_DELTA) {
        uint32_t at=2u, end=0, offset,size;
        uint8_t *swap;
        r.action=WIRE6_SEND_ACK;
        if(!wire6_has(c,WIRE6_NATIVE_DELTA)){r.result=WIRE6_UNSUPPORTED;return r;}
        if(c->phase!=2)return r;
        if(c->stage_len){r.result=WIRE6_BUSY;return r;}
        if(!c->view || !c->stage || c->view_len<CAFA_NATIVE_HEADER ||
           c->view[0]!=CAFA_NATIVE_KIND || n<15u || wire6_u14(p)!=c->revision ||
           !wire6_newer(serial,c->revision))return r;
        /* Inspect every record before touching scratch or the lease. A record is
         * offset:u14, count:u7, then exactly count bytes. */
        while(at<n-9u) {
            if(n-9u-at<3u)return r;
            offset=wire6_u14(p+at);size=p[at+2u];at+=3u;
            if(!size || offset<end || offset+size>c->view_len || size>n-9u-at)return r;
            end=offset+size;at+=size;
        }
        if(at!=n-9u)return r;
        memcpy(c->stage,c->view,c->view_len);
        at=2u;
        while(at<n-9u) {
            offset=wire6_u14(p+at);size=p[at+2u];at+=3u;
            memcpy(c->stage+offset,p+at,size);at+=size;
        }
        if(c->stage[0]!=CAFA_NATIVE_KIND || c->stage[6]!=c->view[6] ||
           c->stage[7]!=c->view[7] || !cafa_native_view_for_caps(c->stage,c->view_len,wire6_has(c,WIRE6_NATIVE_PANEL),wire6_has(c,WIRE6_NATIVE_CHROME)))return r;
        swap=c->view;c->view=c->stage;c->stage=swap;
        c->revision=(uint16_t)serial;c->last_ms=now;
        r.result=WIRE6_OK;r.action=WIRE6_NEW_STATE;r.renew=1;
        return r;
    }
    if(f[4]!=WIRE6_STATE)return r;
    r.action=WIRE6_SEND_ACK;
    if(c->stage_len || !c->view)return r;
    if(!wire6_view_valid(p,n-9u,(uint8_t)(1u |
       (wire6_has(c,2u)?2u:0u) | (wire6_has(c,3u)?4u:0u))))return r;
    if(c->phase==2 && !wire6_newer(serial,c->revision)) {
        if(serial==c->revision)r.result=WIRE6_OK;
        return r;
    }
    memcpy(c->view,p,n-9u);c->view_len=(uint8_t)(n-9u);
    c->revision=(uint16_t)serial;c->last_ms=now;r.result=WIRE6_OK;r.renew=1;
    r.action=c->phase==1 ? WIRE6_FIRST_STATE:WIRE6_NEW_STATE;c->phase=2;
    return r;
}
#endif
