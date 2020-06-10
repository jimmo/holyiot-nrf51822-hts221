# Custom firmware for the HolyIOT nRF51822 HTS221 Temperature/Humidity sensor.

See [Aliexpress listing](https://www.aliexpress.com/item/32864621790.html).

## Overview

The default firmware isn't open source, and involves exchanging data via GATT notifications (after connecting to the device).

This is a much simpler firmware that just broadcasts the current temperature and humidity in the advertising payload (using the "manufacturer data" field).
