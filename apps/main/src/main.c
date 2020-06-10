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
#include "hal/hal_i2c.h"
#include "console/console.h"
#include "host/ble_hs.h"


#define HTS_DRDY_PIN (28)
#define HTS_CS_PIN (24)
static bool hts_init_complete = 0;

static uint8_t H0_rH_x2 = 0;
static uint8_t H1_rH_x2 = 0;

static uint16_t T0_degC_x8 = 0;
static uint16_t T1_degC_x8 = 0;

static int16_t H0_T0_OUT = 0;
static int16_t H1_T0_OUT = 0;

static int16_t T0_OUT = 0;
static int16_t T1_OUT = 0;

static int16_t H_rH_x2;
static int16_t T_degC_x8;

static void ble_app_set_addr(void) {
    ble_addr_t addr;
    int rc;

    rc = ble_hs_id_gen_rnd(1, &addr);
    assert(rc == 0);

    rc = ble_hs_id_set_rnd(addr.val);
    assert(rc == 0);
}

static void ble_app_advertise(int32_t duration_ms);

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
ble_app_advertise(int32_t duration_ms) {
    int rc;

    if (ble_gap_adv_active()) {
        ble_gap_adv_stop();
    }

    memcpy(adv_data + 7, &H_rH_x2, 2);
    memcpy(adv_data + 9, &T_degC_x8, 2);

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

static void ble_app_on_sync(void) {
    // Generate a non-resolvable private address.
    ble_app_set_addr();

    // Start advertising.
    ble_app_advertise(5000);
}

static void hts_write_reg(uint8_t reg, uint8_t value) {
    int rc;

    uint8_t buf[2] = { reg, value };

    struct hal_i2c_master_data data = {
        .address = 0x5f,
        .len = 2,
        .buffer = buf,
    };
    rc = hal_i2c_master_write(0, &data, OS_TICKS_PER_SEC, 1);
    assert(rc == 0);
}

static uint8_t hts_read_reg(uint8_t reg) {
    int rc;

    struct hal_i2c_master_data data = {
        .address = 0x5f,
        .len = 1,
        .buffer = &reg,
    };
    rc = hal_i2c_master_write(0, &data, OS_TICKS_PER_SEC, 1);
    assert(rc == 0);
    rc = hal_i2c_master_read(0, &data, OS_TICKS_PER_SEC, 1);
    assert(rc == 0);
    return reg;
}

static void hts_read_bytes(uint8_t reg, uint8_t *buf, uint8_t len) {
    int rc;

    buf[0] = reg | 0x80;

    struct hal_i2c_master_data data = {
        .address = 0x5f,
        .len = 1,
        .buffer = buf,
    };
    rc = hal_i2c_master_write(0, &data, OS_TICKS_PER_SEC, 1);
    assert(rc == 0);
    data.len = len;
    rc = hal_i2c_master_read(0, &data, OS_TICKS_PER_SEC, 1);
    assert(rc == 0);
}

static int16_t hts_read_reg_s16(uint8_t reg) {
    uint8_t buf[2];
    hts_read_bytes(reg, buf, 2);
    uint16_t v = (buf[1] << 8) | buf[0];
    return *((int16_t*)&v);
}

void drdy_irq(void *arg) {
    if (!hts_init_complete) {
        return;
    }

    hal_gpio_write(LED_BLINK_PIN, 1);

    int32_t H_OUT = hts_read_reg_s16(0x28);
    int32_t T_OUT = hts_read_reg_s16(0x2a);

    H_rH_x2 = H0_rH_x2 + ((H_OUT - H0_T0_OUT) * (H1_rH_x2 - H0_rH_x2)) / (H1_T0_OUT - H0_T0_OUT);
    T_degC_x8 = T0_degC_x8 + ((T_OUT - T0_OUT) * (T1_degC_x8 - T0_degC_x8)) / (T1_OUT - T0_OUT);

    hal_gpio_write(LED_BLINK_PIN, 0);
}

static int hts_init(void) {
    int rc;

    hal_gpio_irq_init(HTS_DRDY_PIN, &drdy_irq, NULL, HAL_GPIO_TRIG_RISING, HAL_GPIO_PULL_NONE);
    hal_gpio_irq_enable(HTS_DRDY_PIN);

    // Enable I2C mode.
    hal_gpio_init_out(HTS_CS_PIN, 1);

    os_time_delay(OS_TICKS_PER_SEC / 4);

    rc = hal_i2c_master_probe(0, 0x5f, OS_TICKS_PER_SEC);
    assert(rc == 0);

    hts_write_reg(0x10, 0x1b); // Averaging conf, temp=16 hum=32 0.03deg 0.15rH% 2.10uA
    hts_write_reg(0x20, 0x85); // Control reg 1, power-on, block-data-update, 1Hz
    hts_write_reg(0x22, 0x04); // Control reg 3, Data ready active-high, push-pull, enable

    os_time_delay(OS_TICKS_PER_SEC / 2);

    uint8_t whoami = hts_read_reg(0x0f);

    assert(whoami == 0xbc);
    if (whoami == 0xbc) {
        hts_init_complete = true;
    }

    H0_rH_x2 = hts_read_reg(0x30);
    H1_rH_x2 = hts_read_reg(0x31);
    T0_degC_x8 = hts_read_reg(0x32);
    T1_degC_x8 = hts_read_reg(0x33);
    uint16_t T_msb = hts_read_reg(0x35);
    T0_degC_x8 |= (T_msb & 0x3) << 8;
    T1_degC_x8 |= (T_msb & 0x0c) << 6;
    H0_T0_OUT = hts_read_reg_s16(0x36);
    H1_T0_OUT = hts_read_reg_s16(0x3a);
    T0_OUT = hts_read_reg_s16(0x3c);
    T1_OUT = hts_read_reg_s16(0x3e);

    hts_read_reg_s16(0x28);
    hts_read_reg_s16(0x2a);

    return 0;
}

int main(int argc, char **argv) {
    int rc;

    sysinit();

    hal_gpio_init_out(LED_BLINK_PIN, 1);

    hts_init();

    ble_hs_cfg.sync_cb = ble_app_on_sync;

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }
    assert(0);

    return rc;
}

