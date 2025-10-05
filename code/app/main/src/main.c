#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/i2c_master.h>
#include <esp_log.h>
#include <string.h>
#include <math.h>

#include <ssd1306.h>
#include <bme280.h>

static const char *TAG = "MAIN";

/* SSD1306 */
static i2c_master_bus_config_t i2c_master_bus_config = {
    .i2c_port = I2C_NUM_0,
    .scl_io_num = GPIO_NUM_8,
    .sda_io_num = GPIO_NUM_9,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .flags.enable_internal_pullup = true};
static i2c_master_bus_handle_t i2c_master_bus;
static ssd1306_config_t dev_cfg = I2C_SSD1306_128x32_CONFIG_DEFAULT;
static ssd1306_handle_t dev_hdl;

/* BME280 */
static i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = GPIO_NUM_9,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_io_num = GPIO_NUM_8,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 100000,
};
static i2c_bus_handle_t i2c_bus;
static bme280_handle_t bme280;

/* Fault counters */
static int sensor_fail_consec = 0;
static int display_fail_consec = 0;

void app_main(void)
{
    float temperature = 0.0, humidity = 0.0, pressure = 0.0;
    char line[22];

    /* I2C + Display init */
    if (i2c_new_master_bus(&i2c_master_bus_config, &i2c_master_bus) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init I2C master bus");
        return;
    }

    if (ssd1306_init(i2c_master_bus, &dev_cfg, &dev_hdl) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init OLED display");
        dev_hdl = NULL;
    } else {
        ssd1306_clear_display(dev_hdl, false);
        ssd1306_set_contrast(dev_hdl, 0xff);
    }

    /* BME280 init */
    i2c_bus = i2c_bus_create(I2C_NUM_0, &conf);
    bme280 = bme280_create(i2c_bus, BME280_I2C_ADDRESS_DEFAULT);

    esp_err_t init_err = bme280_default_init(bme280);
    if (init_err != ESP_OK) {
        ESP_LOGW(TAG, "BME280 not detected at startup (err=0x%x)", init_err);
    }

    while (1)
    {
        esp_err_t err = ESP_OK;

        if (bme280 != NULL) {
            err |= bme280_read_temperature(bme280, &temperature);
            err |= bme280_read_humidity(bme280, &humidity);
            err |= bme280_read_pressure(bme280, &pressure);
        } else {
            err = ESP_FAIL;
        }

        if (err != ESP_OK || isnan(temperature) || isnan(humidity) || isnan(pressure) ||
            humidity < 0.0f || humidity > 100.0f) {
            sensor_fail_consec++;
            ESP_LOGW(TAG, "Sensor read error (consec=%d, err=0x%x)", sensor_fail_consec, err);

            if (dev_hdl) {
                /* Clear + placeholders */
                ssd1306_clear_display(dev_hdl, false);
                snprintf(line, sizeof(line), "T: --");
                ssd1306_display_text(dev_hdl, 0, line, false);
                snprintf(line, sizeof(line), "H: --");
                ssd1306_display_text(dev_hdl, 1, line, false);
                snprintf(line, sizeof(line), "P: -- ERR");
                ssd1306_display_text(dev_hdl, 2, line, false);
            }

            if (sensor_fail_consec >= 3) {
                ESP_LOGW(TAG, "Reinitializing BME280 after 3 consecutive failures");
                if (bme280_default_init(bme280) == ESP_OK) {
                    sensor_fail_consec = 0;
                }
            }
        } else {
            sensor_fail_consec = 0;

            if (dev_hdl) {
                snprintf(line, sizeof(line), "T: %.2f C", temperature);
                ssd1306_display_text(dev_hdl, 0, line, false);

                snprintf(line, sizeof(line), "H: %.2f%%", humidity);
                ssd1306_display_text(dev_hdl, 1, line, false);

                snprintf(line, sizeof(line), "P: %.2f hPa", pressure);
                ssd1306_display_text(dev_hdl, 2, line, false);
            }
        }

        /* Display recovery */
        if (display_fail_consec >= 3) {
            ESP_LOGW(TAG, "Reinitializing OLED after consecutive failures");
            if (ssd1306_init(i2c_master_bus, &dev_cfg, &dev_hdl) == ESP_OK) {
                ssd1306_clear_display(dev_hdl, false);
                ssd1306_set_contrast(dev_hdl, 0xff);
                display_fail_consec = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
