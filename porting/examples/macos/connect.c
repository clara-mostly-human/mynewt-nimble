/* Connect test: scan for a device whose name starts with argv[1], connect, exchange MTU,
 * discover services, read standard Device Information / GAP name characteristics, disconnect.
 * Read-only, no pairing. Exits 0 on success, 1 on failure or timeout. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include "host/ble_hs.h"
#include "host/util/util.h"

static const char *want = "Aranet";
static int deadline_s = 60;
static uint8_t own_addr;
static uint16_t conn_h;
static int n_svcs, reads_left;
static const uint16_t read_uuids[] = { 0x2A00, 0x2A29, 0x2A24, 0x2A26, 0x2A28 };
static const char *read_names[] = { "device name", "manufacturer", "model", "firmware", "software" };

static void fail(const char *what, int rc) { printf("FAIL %s rc=%d\n", what, rc); fflush(stdout); exit(1); }

static void *watchdog(void *arg) { sleep(deadline_s); printf("FAIL timeout after %d s\n", deadline_s); fflush(stdout); exit(1); }

static void finish(void)
{
    printf("all reads done; disconnecting\n"); fflush(stdout);
    ble_gap_terminate(conn_h, BLE_ERR_REM_USER_CONN_TERM);
}

static int read_cb(uint16_t c, const struct ble_gatt_error *err, struct ble_gatt_attr *attr, void *arg)
{
    int i = (int)(intptr_t)arg;
    if (err->status == 0 && attr) {
        char buf[128] = {0};
        uint16_t len = OS_MBUF_PKTLEN(attr->om);
        if (len > sizeof buf - 1) len = sizeof buf - 1;
        os_mbuf_copydata(attr->om, 0, len, buf);
        printf("  read %-12s (0x%04X, handle %u): \"%s\"\n", read_names[i], read_uuids[i], attr->handle, buf);
        return 0;                     /* more values for the same UUID may follow */
    }
    if (err->status != BLE_HS_EDONE && err->status != 0) {
        printf("  read %-12s (0x%04X): not available (status %d)\n", read_names[i], read_uuids[i], err->status);
    }
    if (--reads_left == 0) finish();
    return 0;
}

static void start_reads(void)
{
    reads_left = sizeof read_uuids / sizeof read_uuids[0];
    for (int i = 0; i < reads_left; i++) {
        ble_uuid16_t u = BLE_UUID16_INIT(read_uuids[i]);
        int rc = ble_gattc_read_by_uuid(conn_h, 1, 0xffff, &u.u, read_cb, (void *)(intptr_t)i);
        if (rc) fail("read_by_uuid", rc);
    }
}

static int svc_cb(uint16_t c, const struct ble_gatt_error *err, const struct ble_gatt_svc *svc, void *arg)
{
    if (err->status == 0) {
        char s[BLE_UUID_STR_LEN];
        printf("  service %s handles %u-%u\n", ble_uuid_to_str(&svc->uuid.u, s), svc->start_handle, svc->end_handle);
        n_svcs++;
    } else if (err->status == BLE_HS_EDONE) {
        printf("discovered %d services\n", n_svcs); fflush(stdout);
        start_reads();
    } else {
        fail("service discovery", err->status);
    }
    return 0;
}

static int mtu_cb(uint16_t c, const struct ble_gatt_error *err, uint16_t mtu, void *arg)
{
    printf("MTU exchange: status %d, mtu %u\n", err->status, mtu); fflush(stdout);
    int rc = ble_gattc_disc_all_svcs(conn_h, svc_cb, NULL);
    if (rc) fail("disc_all_svcs", rc);
    return 0;
}

static int gap_cb(struct ble_gap_event *ev, void *arg)
{
    struct ble_hs_adv_fields f;
    switch (ev->type) {
    case BLE_GAP_EVENT_DISC:
        {   /* list the names heard, once each, with their signal strength */
            static char seen[64][32]; static int n_seen;
            if (ble_hs_adv_parse_fields(&f, ev->disc.data, ev->disc.length_data) == 0 && f.name && f.name_len) {
                char nm[32] = {0}; memcpy(nm, f.name, f.name_len < 31 ? f.name_len : 31);
                int dup = 0;
                for (int i = 0; i < n_seen; i++) if (!strcmp(seen[i], nm)) dup = 1;
                if (!dup && n_seen < 64) { strcpy(seen[n_seen++], nm); printf("  heard \"%s\" rssi %d\n", nm, ev->disc.rssi); fflush(stdout); }
            }
        }
        if (ble_hs_adv_parse_fields(&f, ev->disc.data, ev->disc.length_data) == 0 && f.name &&
            f.name_len >= strlen(want) && !memcmp(f.name, want, strlen(want))) {
            const uint8_t *a = ev->disc.addr.val;
            printf("found \"%.*s\" %02X:%02X:%02X:%02X:%02X:%02X type %u rssi %d; connecting\n",
                   f.name_len, (const char *)f.name, a[5], a[4], a[3], a[2], a[1], a[0],
                   ev->disc.addr.type, ev->disc.rssi);
            fflush(stdout);
            ble_gap_disc_cancel();
            int rc = ble_gap_connect(own_addr, &ev->disc.addr, 15000, NULL, gap_cb, NULL);
            if (rc) fail("ble_gap_connect", rc);
        }
        return 0;
    case BLE_GAP_EVENT_CONNECT:
        if (ev->connect.status) fail("connect", ev->connect.status);
        conn_h = ev->connect.conn_handle;
        printf("CONNECTED (handle %u)\n", conn_h); fflush(stdout);
        if (ble_gattc_exchange_mtu(conn_h, mtu_cb, NULL)) fail("exchange_mtu", -1);
        return 0;
    case BLE_GAP_EVENT_DISCONNECT:
        printf("DISCONNECTED reason 0x%03x\n", ev->disconnect.reason);
        printf(reads_left == 0 ? "PASS\n" : "FAIL disconnected early\n");
        fflush(stdout);
        exit(reads_left == 0 ? 0 : 1);
    default:
        return 0;
    }
}

static void on_sync(void)
{
    struct ble_gap_disc_params p = { .itvl = 160, .window = 80, .passive = 0, .filter_duplicates = 0 };
    if (ble_hs_util_ensure_addr(0) || ble_hs_id_infer_auto(0, &own_addr)) fail("address", -1);
    printf("host synced; scanning (active) for a name starting with \"%s\"\n", want); fflush(stdout);
    int rc = ble_gap_disc(own_addr, BLE_HS_FOREVER, &p, gap_cb, NULL);
    if (rc) fail("ble_gap_disc", rc);
}

void scan_init(int seconds) { deadline_s = seconds; ble_hs_cfg.sync_cb = on_sync; }
void scan_set_name(const char *n) { want = n; }

void nimble_port_run(void);
void nimble_host_task(void *param)
{
    pthread_t t;
    pthread_create(&t, NULL, watchdog, NULL);
    nimble_port_run();
}
