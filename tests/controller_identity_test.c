/* Controller USB mailbox: shipping OTA identity survives wire-6 ownership. */
#include <assert.h>
#include "../firmware/src/cafa_protocol.h"
#define FELUCCA_CONTROLLER 1
#define FELUCCA_CAFA 1
#define FELUCCA_ID "FM-1_700"
#define EDITOR_TEST_NO_MAIN 1
static cafa_link_t cafa;
static volatile uint8_t cafa_audio_on;
static int cafa_frame(const uint8_t *p, uint32_t n);
#include "editor_test.c"
#include "../firmware/src/ota.c"
#define FM1_NCOL 11u
#define LED_KEY0 14u
#define LED_PLAY_GREEN ((8u << 3) | 1u)
static void fm1_led_dim_level(uint32_t lo) { (void)lo; }
#define CAFA_PAINT_FULL 128u
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_breath[FM1_NCOL], fm1_led_mid[FM1_NCOL];
static struct { uint8_t active, requested; uint32_t buttons; } cafa_device_menu;
static uint8_t cafa_native_confirm_backdrop;
static void cafa_native_device_reset(void) {}
static void cafa_native_device_open_local(void) {}
static int cafa_native_device_input(uint32_t *b, uint32_t *k, int32_t *t)
{ (void)b; (void)k; (void)t; return 0; }
static void cafa_native_device_draw_local(void) {}
static void cafa_native_profile_reset(void) {}
static uint8_t cafa_native_profile_regions(const uint8_t *a, uint32_t al, const uint8_t *b, uint32_t bl)
{ (void)a; (void)al; (void)b; (void)bl; return 0; }
static void cafa_native_profile_draw_regions(const uint8_t *v, uint32_t n, int full, uint8_t regions)
{ (void)v; (void)n; (void)full; (void)regions; }
static void cafa_native_draw(const uint8_t *v, const uint8_t *old, int full, int changed)
{ (void)v; (void)old; (void)full; (void)changed; }
#include "../firmware/src/cafa.c"

static void feed(const uint8_t *p, uint32_t n)
{
    uint32_t i;
    sysex_byte(0xF0);
    for (i = 0; i < n; i++) sysex_byte(p[i]);
    sysex_byte(0xF7);
}

static void service(void)
{
    ed_service();
    ota_service();
    host_drain();
    assert(!sx_ready);
}

static void identity(const uint8_t *query, uint32_t query_n,
                     const uint8_t *reply, uint32_t reply_n)
{
    host_wire_n = 0;
    feed(query, query_n);
    assert(sx_ready);
    ed_service();
    assert(sx_ready); /* editor leaves the flasher's frame for OTA */
    ota_service();
    host_drain();
    assert(!sx_ready && host_wire_n == reply_n && !memcmp(host_wire, reply, reply_n));
}

int main(void)
{
    static const uint8_t query[] = {0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7f};
    static const uint8_t reply[] = {
        0xf0, 0x00, 0x32, 0x45, 0x58, 0x01, 0x00, 0x00, 0x23, 0x4d,
        0x5a, 0x44, 0x79, 0x75, 0x06, 0x0c, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0xf7
    };
    static const uint8_t hello[] = {
        0x7d, 0x43, 0x41, 6, WIRE6_HELLO, 7, 0, 0, 0,
        1, 0, 0, 3, 'A', 'P', 'P', 1, 1, 1, 0, 1
    };
    static const uint8_t hello_ack[] = {
        0xf0, 0x7d, 0x43, 0x41, 6, WIRE6_ACK, 7, 0, 0, 0,
        WIRE6_HELLO, WIRE6_OK, 1, 1, 1, 0, 0, 0xf7
    };
    static const uint8_t state_ack[] = {
        0xf0, 0x7d, 0x43, 0x41, 6, WIRE6_ACK, 7, 0, 1, 0,
        WIRE6_STATE, WIRE6_OK, 0xf7
    };
    uint8_t state[147];
    uint32_t k;
    reset();
    identity(query, sizeof query, reply, sizeof reply);
    host_wire_n = 0;
    feed(hello, sizeof hello);
    service();
    assert(wire6.phase == 1 && !cafa.phase && host_wire_n == sizeof hello_ack &&
           !memcmp(host_wire, hello_ack, sizeof hello_ack));
    memset(state, 0, sizeof state);
    memcpy(state, hello, 9);
    state[4] = WIRE6_STATE;
    state[7] = 1;
    memset(state + 9, ' ', 138);
    state[9] = 1; state[10] = 0;
    wire6_put28(state + 11, 42);
    memcpy(state + 15, "OPEN INSTRUMENT ", 16);
    memcpy(state + 31, "SOUND BANK 1", 12);
    cafa_put14(state + 43, 0); cafa_put14(state + 45, 1);
    cafa_put14(state + 47, 0); cafa_put14(state + 49, 1);
    for (k = 0; k < 4; k++) {
        uint8_t *control = state + 51 + 24 * k;
        memset(control, ' ', 20);
        memset(control + 20, 0, 4);
    }
    memcpy(state + 51, "CUTOFF  ", 8);
    memcpy(state + 59, "1200 Hz     ", 12);
    cafa_put14(state + 71, 8192); state[73] = 1; state[74] = 1;
    host_wire_n = 0;
    feed(state, sizeof state);
    service();
    assert(wire6.phase == 2 && wire6.revision == 1 && cafa_audio_on &&
           wire6.view_len == 138 && !memcmp(wire6.view, state + 9, 138));
    assert(host_wire_n == sizeof state_ack && !memcmp(host_wire, state_ack, sizeof state_ack));
    identity(query, sizeof query, reply, sizeof reply);
    assert(wire6.phase == 2 && wire6.revision == 1 && cafa_audio_on);
    puts("controller identity: FM-1_700 in idle and wire-6 ACTIVE; complete STATE ACK");
    return 0;
}
