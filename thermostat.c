/*
 * HTTP server example.
 *
 * This sample code is in the public domain.
 */
#include <espressif/esp_common.h>
#include <esp8266.h>
#include <esp/uart.h>
#include <string.h>
#include <stdio.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ssid_config.h>
#include <httpd/httpd.h>
#include <ds18b20/ds18b20.h>
#include <math.h>

enum {
    SSI_TEMPERATURE,
    SSI_THERMOSTAT_STATE,
    SSI_ERROR_COUNT
};

#define SENSOR_GPIO 14
#define LOOP_DELAY_MS 250

volatile int error_count = 0;
volatile float temperature = NAN;

typedef enum {
    Cooling,
    Heating
} ThermostatState;

ThermostatState ts = Cooling;
#define RELAY_PIN 12

int32_t ssi_handler(int32_t iIndex, char *pcInsert, int32_t iInsertLen)
{
    switch (iIndex) {
        case SSI_TEMPERATURE:
            snprintf(pcInsert, iInsertLen, "%.2f", temperature);
            break;
        case SSI_ERROR_COUNT:
            snprintf(pcInsert, iInsertLen, "%d", error_count);
            break;
        case SSI_THERMOSTAT_STATE:
            snprintf(pcInsert, iInsertLen, ts == Heating ? "Heating" : "Cooling");
            break;
        default:
            snprintf(pcInsert, iInsertLen, "N/A");
            break;
    }

    /* Tell the server how many characters to insert */
    return (strlen(pcInsert));
}

char *gpio_cgi_handler(int iIndex, int iNumParams, char *pcParam[], char *pcValue[])
{
    return "/index.ssi";
}

void httpd_task(void *pvParameters)
{
    tCGI pCGIs[] = {
        {"/gpio", (tCGIHandler) gpio_cgi_handler},
    };

    const char *pcConfigSSITags[] = {
        "temp", // SSI_TEMPERATURE
        "stat", // SSI_THERMOSTAT_STATE
        "err"   // SSI_ERROR_COUNT
    };

    /* register handlers and start the server */
    http_set_cgi_handlers(pCGIs, sizeof (pCGIs) / sizeof (pCGIs[0]));
    http_set_ssi_handler((tSSIHandler) ssi_handler, pcConfigSSITags,
            sizeof (pcConfigSSITags) / sizeof (pcConfigSSITags[0]));
    httpd_init();

    for (;;);
}

void measure_task(void *pvParameters)
{
    ds18b20_addr_t addr = 0x28FF0CA301170383ULL;

    while (1) {
        temperature = ds18b20_measure_and_read(SENSOR_GPIO, addr);

        if (isnan(temperature)) {
            error_count++;
        } else {
            error_count = 0;
        }

        vTaskDelay(LOOP_DELAY_MS / portTICK_PERIOD_MS);
    }
}

void setThermostatState(ThermostatState state) {
    if (ts == state) {
        return;
    }

    ts = state;

    if (state == Heating) {
        gpio_write(RELAY_PIN, 1);
    } else {
        gpio_write(RELAY_PIN, 0);
    }
}

void thermostat_task(void *pvParameters) {
    gpio_enable(RELAY_PIN, GPIO_OUTPUT);

    while (1) {
      if (error_count > 10) {
          setThermostatState(Cooling);
      } else {
          switch (ts) {
              case Cooling:
                  if (temperature < 60.0) {
                      setThermostatState(Heating);
                  }
                  break;
              case Heating:
                  if (temperature > 80.0) {
                      setThermostatState(Cooling);
                  }
                  break;
          }
      }

      vTaskDelay(LOOP_DELAY_MS / portTICK_PERIOD_MS);
    }
}

void user_init(void)
{
    uart_set_baud(0, 115200);
    printf("SDK version:%s\n", sdk_system_get_sdk_version());

    struct sdk_station_config config = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
    };

    /* required to call wifi_set_opmode before station_set_config */
    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&config);
    sdk_wifi_station_set_auto_connect(true);
    sdk_wifi_station_connect();

    /* initialize tasks */
    xTaskCreate(&httpd_task, "HTTP Daemon", 256, NULL, 2, NULL);
    xTaskCreate(&measure_task, "Measurement", 256, NULL, 3, NULL);
    xTaskCreate(&thermostat_task, "Thermostat", 256, NULL, 4, NULL);
}
