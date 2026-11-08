// rtc-probe: does the watch's PCF85063A answer, and is it already counting?
//
// This is throwaway diagnostic firmware for the ESP32-S3 on the watch, built
// with ESP-IDF. It exists because the Linux port is a *board* port: its device
// tree puts I2C on GPIO26/27, while this watch wires it to GPIO3/2, so the
// kernel sees an empty bus and nothing about the RTC can be established from
// userspace.
//
// What it answers, in order:
//
//   1. Is there anything on the bus at all, on the watch's real pins?
//   2. Does the PCF85063A answer at 0x51?
//   3. Is it running or stopped -- and is it counting? Two reads a second apart
//      that differ prove the oscillator is alive and VBACKUP is holding it.
//   4. What is actually in its registers, so the Linux driver can be trusted
//      rather than assumed.
//
// Nothing here is meant to be kept. The real integration is an RTC node in the
// device tree plus rtc-pcf85063 in the kernel; this only establishes the facts
// that integration depends on.

#include <stdio.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "rtcprobe";

#define SDA_GPIO 3          // measured on this board (TinyPetz board.h)
#define SCL_GPIO 2

#define PCF85063A_ADDR 0x51
#define AXP2101_ADDR   0x34
#define XL9555_ADDR    0x20
#define CST9217_ADDR   0x1A
#define DRV2605_ADDR   0x5A
#define BHI260AP_ADDR  0x28

// PCF85063A register map (NXP datasheet, Rev. 6).
#define REG_CTRL1   0x00
#define REG_CTRL2   0x01
#define REG_SECONDS 0x04
#define REG_MINUTES 0x05
#define REG_HOURS   0x06
#define REG_DAYS    0x07
#define REG_WEEKDAYS 0x08
#define REG_MONTHS  0x09
#define REG_YEARS   0x0A
#define REG_ALARM   0x0B

static i2c_master_bus_handle_t s_bus;

static inline uint8_t bcd2bin(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }

static esp_err_t rd(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out, size_t n)
{
    return i2c_master_transmit_receive(dev, &reg, 1, out, n, 1000);
}

static esp_err_t wr(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t b[2] = {reg, val};
    return i2c_master_transmit(dev, b, 2, 1000);
}

// ---- 1. scan ---------------------------------------------------------------
static void scan(void)
{
    ESP_LOGW(TAG, "scanning 0x08..0x77 on SDA=%d SCL=%d", SDA_GPIO, SCL_GPIO);
    int found = 0;
    for (uint8_t a = 0x08; a <= 0x77; a++) {
        if (i2c_master_probe(s_bus, a, 200) == ESP_OK) {
            const char *what = "?";
            switch (a) {
            case CST9217_ADDR:  what = "CST9217 touch";      break;
            case XL9555_ADDR:   what = "XL9555 expander";    break;
            case BHI260AP_ADDR: what = "BHI260AP sensor";    break;
            case AXP2101_ADDR:  what = "AXP2101 PMU";        break;
            case PCF85063A_ADDR: what = "PCF85063A RTC";     break;
            case DRV2605_ADDR:  what = "DRV2605 haptic";     break;
            }
            ESP_LOGW(TAG, "  found 0x%02X  %s", a, what);
            found++;
        }
    }
    if (!found) {
        ESP_LOGE(TAG, "  nothing on the bus -- wrong pins, or no pull-ups");
    }
}

// ---- 3/4. the RTC ----------------------------------------------------------
static void rtc_dump(void)
{
    i2c_master_dev_handle_t dev;
    i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCF85063A_ADDR,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(s_bus, &cfg, &dev) != ESP_OK) {
        ESP_LOGE(TAG, "could not attach to 0x%02X", PCF85063A_ADDR);
        return;
    }

    uint8_t ctrl1 = 0, ctrl2 = 0;
    if (rd(dev, REG_CTRL1, &ctrl1, 1) != ESP_OK ||
        rd(dev, REG_CTRL2, &ctrl2, 1) != ESP_OK) {
        ESP_LOGE(TAG, "no answer at 0x%02X", PCF85063A_ADDR);
        i2c_master_bus_rm_device(dev);
        return;
    }
    ESP_LOGW(TAG, "CTRL1=0x%02X CTRL2=0x%02X", ctrl1, ctrl2);
    // CTRL1 bit 5 = STOP: set means the divider is held in reset and the clock
    // is not advancing. Bit 7 = TEST, must be 0 on a healthy part.
    ESP_LOGW(TAG, "  STOP=%d (1 = clock not running)  TEST=%d",
             (ctrl1 >> 5) & 1, (ctrl1 >> 7) & 1);

    uint8_t t1[8] = {0}, t2[8] = {0};
    uint8_t reg = REG_SECONDS;
    if (i2c_master_transmit_receive(dev, &reg, 1, t1, 7, 1000) != ESP_OK) {
        ESP_LOGE(TAG, "time read failed");
        i2c_master_bus_rm_device(dev);
        return;
    }
    ESP_LOGW(TAG, "raw seconds..years: %02X %02X %02X %02X %02X %02X %02X",
             t1[0], t1[1], t1[2], t1[3], t1[4], t1[5], t1[6]);
    ESP_LOGW(TAG, "decoded 20%02d-%02d-%02d (weekday %d) %02d:%02d:%02d",
             bcd2bin(t1[6]), bcd2bin(t1[5] & 0x1F), bcd2bin(t1[3] & 0x3F),
             t1[4] & 0x07, bcd2bin(t1[2] & 0x3F), bcd2bin(t1[1] & 0x7F),
             bcd2bin(t1[0] & 0x7F));

    // Does it advance? Two reads two seconds apart. If the values differ the
    // oscillator is running and backup power is holding it.
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (i2c_master_transmit_receive(dev, &reg, 1, t2, 7, 1000) == ESP_OK) {
        ESP_LOGW(TAG, "after 2 s: %02d:%02d:%02d  -> %s",
                 bcd2bin(t2[2] & 0x3F), bcd2bin(t2[1] & 0x7F), bcd2bin(t2[0] & 0x7F),
                 memcmp(t1, t2, 7) ? "COUNTING (backup power alive)"
                                   : "STOPPED (same value)");
    }

    // Clears a latched alarm flag so it does not look like a fault later.
    wr(dev, REG_CTRL2, ctrl2 & ~0x40);
    i2c_master_bus_rm_device(dev);
}

void app_main(void)
{
    ESP_LOGW(TAG, "=== PCF85063A probe ===");

    i2c_master_bus_config_t bus = {
        .i2c_port = -1,
        .sda_io_num = SDA_GPIO,
        .scl_io_num = SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus, &s_bus));

    scan();
    rtc_dump();

    ESP_LOGW(TAG, "=== done; flash Linux back with ./scripts/flash.sh --app ===");
}
