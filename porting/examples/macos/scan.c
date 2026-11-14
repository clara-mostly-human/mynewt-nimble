/* Passive scan: count advertising reports and distinct addresses, print every 5 s, exit after N s. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "host/ble_hs.h"
#include "host/util/util.h"

#define MAX_ADDRS 1024
static uint8_t addrs[MAX_ADDRS][7];
static int n_addrs, n_reports, run_seconds;
static time_t started;

static void note(const ble_addr_t *a)
{
    for (int i = 0; i < n_addrs; i++) {
        if (addrs[i][0] == a->type && !memcmp(&addrs[i][1], a->val, 6)) return;
    }
    if (n_addrs < MAX_ADDRS) { addrs[n_addrs][0] = a->type; memcpy(&addrs[n_addrs][1], a->val, 6); n_addrs++; }
}

static int gap_cb(struct ble_gap_event *ev, void *arg)
{
    if (ev->type == BLE_GAP_EVENT_DISC) {
        n_reports++;
        note(&ev->disc.addr);
        static time_t last;
        time_t now = time(NULL);
        if (now - last >= 5) {
            last = now;
            printf("t=%lds reports=%d distinct=%d\n", (long)(now - started), n_reports, n_addrs);
            fflush(stdout);
        }
        if (now - started >= run_seconds) {
            printf("DONE %d s: %d advertising reports from %d distinct addresses\n", run_seconds, n_reports, n_addrs);
            fflush(stdout);
            exit(0);
        }
    } else if (ev->type == BLE_GAP_EVENT_DISC_COMPLETE) {
        printf("discovery complete (reason %d)\n", ev->disc_complete.reason);
    }
    return 0;
}

static void on_sync(void)
{
    uint8_t own;
    struct ble_gap_disc_params p;
    uint8_t addr[6];
    int rc = ble_hs_util_ensure_addr(0);
    if (rc) { printf("ensure_addr rc=%d\n", rc); exit(1); }
    rc = ble_hs_id_infer_auto(0, &own);
    if (rc) { printf("infer_auto rc=%d\n", rc); exit(1); }
    if (ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, addr, NULL) == 0) {
        printf("host synced; controller public address %02X:%02X:%02X:%02X:%02X:%02X\n",
               addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);
    }
    memset(&p, 0, sizeof p);
    p.passive = 1;
    p.filter_duplicates = 0;
    p.itvl = 240;     /* 150 ms in 0.625 ms units */
    p.window = 48;    /* 30 ms */
    started = time(NULL);
    rc = ble_gap_disc(own, BLE_HS_FOREVER, &p, gap_cb, NULL);
    printf("ble_gap_disc rc=%d (passive, window 30 ms every 150 ms)\n", rc);
    fflush(stdout);
    if (rc) exit(1);
}

static void on_reset(int reason) { printf("host reset, reason %d\n", reason); fflush(stdout); }

void scan_init(int seconds)
{
    run_seconds = seconds;
    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;
}

void nimble_port_run(void);

void
nimble_host_task(void *param)
{
    nimble_port_run();
}
