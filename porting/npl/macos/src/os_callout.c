/*
 * macOS NPL: callouts (one-shot timers) on libdispatch timer sources, all on one serial
 * queue (macOS does not implement POSIX timers: timer_create/timer_settime).
 * Licensed under the Apache License 2.0, like the Linux NPL this replaces.
 */
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <dispatch/dispatch.h>
#include "nimble/nimble_npl.h"

static dispatch_queue_t
callout_queue(void)
{
    static dispatch_once_t once;
    static dispatch_queue_t q;
    dispatch_once(&once, ^{ q = dispatch_queue_create("nimble.callout", DISPATCH_QUEUE_SERIAL); });
    return q;
}

static void
ble_npl_callout_fire(struct ble_npl_callout *c)
{
    if (!c->c_active) {
        return;                 /* stopped after the source fired but before we ran */
    }
    c->c_active = false;
    if (c->c_evq) {
        ble_npl_eventq_put(c->c_evq, &c->c_ev);
    } else {
        c->c_ev.ev_cb(&c->c_ev);
    }
}

void
ble_npl_callout_init(struct ble_npl_callout *c, struct ble_npl_eventq *evq,
                     ble_npl_event_fn *ev_cb, void *ev_arg)
{
    dispatch_source_t src;

    memset(c, 0, sizeof(*c));
    c->c_ev.ev_cb = ev_cb;
    c->c_ev.ev_arg = ev_arg;
    c->c_evq = evq;
    c->c_active = false;

    src = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, callout_queue());
    dispatch_source_set_event_handler(src, ^{ ble_npl_callout_fire(c); });
    dispatch_source_set_timer(src, DISPATCH_TIME_FOREVER, DISPATCH_TIME_FOREVER, 0);
    dispatch_resume(src);
    c->c_timer = (void *)src;
}

bool
ble_npl_callout_is_active(struct ble_npl_callout *c)
{
    return c->c_active;
}

int
ble_npl_callout_inited(struct ble_npl_callout *c)
{
    return (c->c_timer != NULL);
}

ble_npl_error_t
ble_npl_callout_reset(struct ble_npl_callout *c, ble_npl_time_t ticks)
{
    if (ticks == 0) {
        ticks = 1;
    }
    c->c_ticks = ble_npl_time_get() + ticks;
    c->c_active = true;
    /* NPL ticks are milliseconds (os_time.c) */
    dispatch_source_set_timer((dispatch_source_t)c->c_timer,
                              dispatch_time(DISPATCH_TIME_NOW, (int64_t)ticks * NSEC_PER_MSEC),
                              DISPATCH_TIME_FOREVER, 100 * NSEC_PER_USEC);
    return BLE_NPL_OK;
}

int
ble_npl_callout_queued(struct ble_npl_callout *c)
{
    return c->c_active;
}

void
ble_npl_callout_stop(struct ble_npl_callout *c)
{
    if (!ble_npl_callout_inited(c)) {
        return;
    }
    c->c_active = false;
    dispatch_source_set_timer((dispatch_source_t)c->c_timer, DISPATCH_TIME_FOREVER,
                              DISPATCH_TIME_FOREVER, 0);
}

ble_npl_time_t
ble_npl_callout_get_ticks(struct ble_npl_callout *co)
{
    return co->c_ticks;
}

void
ble_npl_callout_set_arg(struct ble_npl_callout *co, void *arg)
{
    co->c_ev.ev_arg = arg;
}

uint32_t
ble_npl_callout_remaining_ticks(struct ble_npl_callout *co, ble_npl_time_t now)
{
    if (!co->c_active || co->c_ticks <= now) {
        return 0;
    }
    return co->c_ticks - now;
}
