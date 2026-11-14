/* NimBLE host on macOS, HCI over TCP to 127.0.0.1:14433 (the zigbee-ble bridge's raw HCI port). */
#include <stdio.h>
#include <stdlib.h>
#include "nimble/nimble_npl.h"
#include "nimble/nimble_port.h"
#include "services/gap/ble_svc_gap.h"

static struct ble_npl_task s_task_host;
static struct ble_npl_task s_task_hci;

void nimble_host_task(void *param);
void ble_hci_sock_ack_handler(void *param);
void scan_init(int seconds);
void scan_set_name(const char *n) __attribute__((weak));

static void *hci_task(void *p) { ble_hci_sock_ack_handler(p); return NULL; }
static void *host_task(void *p) { nimble_host_task(p); return NULL; }

int main(int argc, char *argv[])
{
    int seconds = argc > 1 ? atoi(argv[1]) : 20;
    if (argc > 2 && scan_set_name) scan_set_name(argv[2]);
    nimble_port_init();
    ble_svc_gap_init();
    scan_init(seconds);
    ble_npl_task_init(&s_task_hci, "hci_sock", hci_task, NULL, 1, BLE_NPL_TIME_FOREVER, NULL, 400);
    ble_npl_task_init(&s_task_host, "ble_host", host_task, NULL, 1, BLE_NPL_TIME_FOREVER, NULL, 400);
    pthread_exit(NULL);
    return 0;
}
