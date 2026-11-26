#include "watch_service.h"
#include "common_blesvc.h"
#include <string.h>

/* 6f91xxxx-c2a6-4b3e-8f1d-0a5e3b7c9d10, little-endian; xxxx goes into bytes 12-13 */
static void uuid128(Char_UUID_t *u, uint16_t id)
{
    static const uint8_t base[16] = {0x10,0x9d,0x7c,0x3b,0x5e,0x0a,0x1d,0x8f,0x3e,0x4b,0xa6,0xc2,0x00,0x00,0x91,0x6f};
    memcpy(u->Char_UUID_128, base, 16);
    u->Char_UUID_128[12] = (uint8_t)id;
    u->Char_UUID_128[13] = (uint8_t)(id >> 8);
}

enum { C_TIME, C_STEPS, C_HIST, C_HR, C_CTRL, C_PPG, C_SET, C_COUNT };

static struct {
    uint16_t svc, bas_svc, hrs_svc;
    uint16_t ch[C_COUNT];
    uint16_t bas_level, hrs_meas, hrs_loc;
    uint16_t conn;
    bool connected, encrypted;
} ctx;

#define SECURE_R  (ATTR_PERMISSION_ENCRY_READ | ATTR_PERMISSION_AUTHEN_READ)
#define SECURE_W  (ATTR_PERMISSION_ENCRY_WRITE | ATTR_PERMISSION_AUTHEN_WRITE)

static void add_char(uint16_t svc, uint16_t id, uint8_t len, uint8_t props, uint8_t perm, uint8_t evmask, uint16_t *h)
{
    Char_UUID_t u;
    uuid128(&u, id);
    aci_gatt_add_char(svc, UUID_TYPE_128, &u, len, props, perm, evmask, 16, 1, h);
}

static void add_char16(uint16_t svc, uint16_t uuid, uint8_t len, uint8_t props, uint16_t *h)
{
    Char_UUID_t u;
    u.Char_UUID_16 = uuid;
    aci_gatt_add_char(svc, UUID_TYPE_16, &u, len, props, ATTR_PERMISSION_NONE, GATT_DONT_NOTIFY_EVENTS, 10, 0, h);
}

static void update(uint16_t svc, uint16_t ch, uint8_t len, const void *v)
{
    aci_gatt_update_char_value(svc, ch, 0, len, (const uint8_t *)v);
}

static bool may_notify(void) { return ctx.connected && ctx.encrypted; }

static SVCCTL_EvtAckStatus_t event_handler(void *evt)
{
    hci_event_pckt *ev = (hci_event_pckt *)(((hci_uart_pckt *)evt)->data);
    if (ev->evt != HCI_VENDOR_SPECIFIC_DEBUG_EVT_CODE) return SVCCTL_EvtNotAck;
    evt_blecore_aci *core = (evt_blecore_aci *)ev->data;
    if (core->ecode != ACI_GATT_ATTRIBUTE_MODIFIED_VSEVT_CODE) return SVCCTL_EvtNotAck;

    aci_gatt_attribute_modified_event_rp0 *m = (aci_gatt_attribute_modified_event_rp0 *)core->data;
    const uint8_t *d = m->Attr_Data;
    uint16_t h = m->Attr_Handle;

    if (h == ctx.ch[C_TIME] + 1) {
        if (m->Attr_Data_Length >= WSVC_TIME_LEN) {
            uint32_t t = (uint32_t)d[0] | (uint32_t)d[1] << 8 | (uint32_t)d[2] << 16 | (uint32_t)d[3] << 24;
            int16_t tz = (int16_t)(d[4] | d[5] << 8);
            watch_app_ble_time_written(t, tz);
        }
        return SVCCTL_EvtAckFlowEnable;
    }
    if (h == ctx.ch[C_SET] + 1) {
        if (m->Attr_Data_Length >= WSVC_SETTINGS_LEN) watch_app_ble_settings_written(d);
        return SVCCTL_EvtAckFlowEnable;
    }
    if (h == ctx.ch[C_CTRL] + 1) {
        if (m->Attr_Data_Length > 0) watch_app_ble_command(d, (uint8_t)m->Attr_Data_Length);
        return SVCCTL_EvtAckFlowEnable;
    }
    return SVCCTL_EvtNotAck;     /* CCCD writes etc. are handled by the stack */
}

void WSVC_Init(void)
{
    Char_UUID_t u;
    static const uint8_t zero[20] = {0};
    static const uint8_t wrist = 2;   /* Body Sensor Location: wrist */

    SVCCTL_RegisterSvcHandler(event_handler);

    /* Battery Service */
    u.Char_UUID_16 = 0x180F;
    aci_gatt_add_service(UUID_TYPE_16, (Service_UUID_t *)&u, PRIMARY_SERVICE, 4, &ctx.bas_svc);
    add_char16(ctx.bas_svc, 0x2A19, 1, CHAR_PROP_READ | CHAR_PROP_NOTIFY, &ctx.bas_level);

    /* Heart Rate Service: spot-check results are sent as one measurement */
    u.Char_UUID_16 = 0x180D;
    aci_gatt_add_service(UUID_TYPE_16, (Service_UUID_t *)&u, PRIMARY_SERVICE, 6, &ctx.hrs_svc);
    add_char16(ctx.hrs_svc, 0x2A37, 2, CHAR_PROP_NOTIFY, &ctx.hrs_meas);
    add_char16(ctx.hrs_svc, 0x2A38, 1, CHAR_PROP_READ, &ctx.hrs_loc);
    update(ctx.hrs_svc, ctx.hrs_loc, 1, &wrist);

    /* F-91W service: 1 + time 2 + steps 3 + hist 2 + hr 3 + ctrl 2 + ppg 3 + settings 2 = 18 */
    uuid128(&u, 0x0001);
    aci_gatt_add_service(UUID_TYPE_128, (Service_UUID_t *)&u, PRIMARY_SERVICE, 18, &ctx.svc);
    add_char(ctx.svc, 0x0002, WSVC_TIME_LEN, CHAR_PROP_READ | CHAR_PROP_WRITE, SECURE_R | SECURE_W,
             GATT_NOTIFY_ATTRIBUTE_WRITE, &ctx.ch[C_TIME]);
    add_char(ctx.svc, 0x0003, 4, CHAR_PROP_READ | CHAR_PROP_NOTIFY, SECURE_R, GATT_DONT_NOTIFY_EVENTS, &ctx.ch[C_STEPS]);
    add_char(ctx.svc, 0x0004, 4 * WSVC_HIST_DAYS, CHAR_PROP_READ, SECURE_R, GATT_DONT_NOTIFY_EVENTS, &ctx.ch[C_HIST]);
    add_char(ctx.svc, 0x0005, WSVC_HR_LEN, CHAR_PROP_READ | CHAR_PROP_NOTIFY, SECURE_R, GATT_DONT_NOTIFY_EVENTS, &ctx.ch[C_HR]);
    add_char(ctx.svc, 0x0006, 20, CHAR_PROP_WRITE, SECURE_W, GATT_NOTIFY_ATTRIBUTE_WRITE, &ctx.ch[C_CTRL]);
    add_char(ctx.svc, 0x0007, 2 + 3 * WSVC_PPG_MAX, CHAR_PROP_NOTIFY, SECURE_R, GATT_DONT_NOTIFY_EVENTS, &ctx.ch[C_PPG]);
    add_char(ctx.svc, 0x0008, WSVC_SETTINGS_LEN, CHAR_PROP_READ | CHAR_PROP_WRITE, SECURE_R | SECURE_W,
             GATT_NOTIFY_ATTRIBUTE_WRITE, &ctx.ch[C_SET]);
    update(ctx.svc, ctx.ch[C_HIST], 4 * WSVC_HIST_DAYS, zero);
}

void WSVC_SetConnected(uint16_t conn_handle, bool connected)
{
    ctx.conn = conn_handle;
    ctx.connected = connected;
    if (!connected) ctx.encrypted = false;
}

void WSVC_SetEncrypted(bool encrypted) { ctx.encrypted = encrypted; }

void WSVC_UpdateBattery(uint8_t percent) { update(ctx.bas_svc, ctx.bas_level, 1, &percent); }

static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

void WSVC_UpdateTime(uint32_t unix_utc, int16_t tz_min)
{
    uint8_t b[WSVC_TIME_LEN];
    put32(b, unix_utc);
    b[4] = (uint8_t)tz_min;
    b[5] = (uint8_t)((uint16_t)tz_min >> 8);
    update(ctx.svc, ctx.ch[C_TIME], sizeof b, b);
}

void WSVC_UpdateSteps(uint32_t steps)
{
    uint8_t b[4];
    put32(b, steps);
    update(ctx.svc, ctx.ch[C_STEPS], 4, b);   /* only notifies clients that subscribed (paired companion) */
}

void WSVC_UpdateHistory(const uint32_t *days)
{
    uint8_t b[4 * WSVC_HIST_DAYS];
    for (int i = 0; i < WSVC_HIST_DAYS; i++) put32(b + 4 * i, days[i]);
    update(ctx.svc, ctx.ch[C_HIST], sizeof b, b);
}

void WSVC_UpdateHeartRate(const uint8_t *hr8, uint8_t bpm_for_hrs)
{
    update(ctx.svc, ctx.ch[C_HR], WSVC_HR_LEN, hr8);
    if (bpm_for_hrs && ctx.connected) {
        uint8_t m[2] = {0x00, bpm_for_hrs};   /* flags: 8-bit bpm, no contact/energy/RR fields */
        update(ctx.hrs_svc, ctx.hrs_meas, 2, m);
    }
}

void WSVC_UpdateSettings(const uint8_t *s4) { update(ctx.svc, ctx.ch[C_SET], WSVC_SETTINGS_LEN, s4); }

void WSVC_StreamPpg(uint16_t seq, const uint32_t *samples, int n)
{
    uint8_t b[2 + 3 * WSVC_PPG_MAX];
    if (!may_notify()) return;
    while (n > 0) {
        int k = n > WSVC_PPG_MAX ? WSVC_PPG_MAX : n;
        b[0] = (uint8_t)seq; b[1] = (uint8_t)(seq >> 8);
        for (int i = 0; i < k; i++) {
            b[2 + 3 * i] = (uint8_t)samples[i];
            b[3 + 3 * i] = (uint8_t)(samples[i] >> 8);
            b[4 + 3 * i] = (uint8_t)(samples[i] >> 16);
        }
        update(ctx.svc, ctx.ch[C_PPG], (uint8_t)(2 + 3 * k), b);
        seq = (uint16_t)(seq + k);
        samples += k;
        n -= k;
    }
}
