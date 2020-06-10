/**
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>

#include "sysinit/sysinit.h"
#include "os/os.h"
#include "bsp/bsp.h"
#include "hal/hal_gpio.h"
#include "console/console.h"
#include "host/ble_hs.h"

static volatile int g_task1_loops;

/* For LED toggling */
int g_led_pin = 13;

static void
ble_app_set_addr(void)
{
    ble_addr_t addr;
    int rc;

    rc = ble_hs_id_gen_rnd(1, &addr);
    assert(rc == 0);

    rc = ble_hs_id_set_rnd(addr.val);
    assert(rc == 0);
}

static void
ble_app_advertise(int32_t duration_ms);

static int gap_event_cb(struct ble_gap_event *event, void *arg) {
    if (event->type == BLE_GAP_EVENT_ADV_COMPLETE) {
        ble_app_advertise(5000);
    }
    return 0;
}

static uint8_t adv_data[] = {
    0x02, 0x01, 0x06,
    0x07, 0xff, 0xff, 0xff, 0x01, 0x02, 0x01, 0x02,
    0x05, 0x09, 't', 'e', 'm', 'p',
};

static void
ble_app_advertise(int32_t duration_ms)
{
    int rc;

    if (ble_gap_adv_active()) {
        ble_gap_adv_stop();
    }

    hal_gpio_toggle(g_led_pin);

    uint16_t* temp = (uint16_t*)(adv_data + 7);
    uint16_t* humidity = (uint16_t*)(adv_data + 9);

    *temp += 1;
    *humidity -= 1;

    rc = ble_gap_adv_set_data(adv_data, sizeof(adv_data));

    rc = ble_gap_adv_rsp_set_data(NULL, 0);

    int32_t interval_us = 250000;

    struct ble_gap_adv_params adv_params = {
        .conn_mode = BLE_GAP_CONN_MODE_NON,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min = interval_us / BLE_HCI_ADV_ITVL, // convert to 625us units.
        .itvl_max = interval_us / BLE_HCI_ADV_ITVL,
        .channel_map = 7, // all 3 channels.
    };

    rc = ble_gap_adv_start(BLE_OWN_ADDR_RANDOM, NULL, duration_ms, &adv_params, gap_event_cb, NULL);

    assert(rc == 0);
}

static void
ble_app_on_sync(void)
{
    /* Generate a non-resolvable private address. */
    ble_app_set_addr();

    /* Advertise indefinitely. */
    ble_app_advertise(5000);
}


/**
 * main
 *
 * The main task for the project. This function initializes packages,
 * and then blinks the BSP LED in a loop.
 *
 * @return int NOTE: this function should never return!
 */
int
main(int argc, char **argv)
{
    int rc;

    sysinit();

    hal_gpio_init_out(g_led_pin, 1);
    hal_gpio_init_out(4, 0);

    ble_hs_cfg.sync_cb = ble_app_on_sync;

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);

    return rc;
}

