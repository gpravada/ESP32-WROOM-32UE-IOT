#include "ble_app.h"
#include "bmp_app.h"

#include "blehr_sens.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "modlog/modlog.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

char *gap_device_name = "ESP32-WROOM-BLE-Server";

static TimerHandle_t ble_tx_timer;

uint8_t ble_addr_type;

static bool temperature_notify_state;
static bool pressure_notify_state;
static bool humidity_notify_state;

static double temperature = 0.0;
static double pressure    = 0.0;
static double humidity = 0.0;

static uint16_t conn_handle;

#define BUFFER_SIZE 100
static char buffer[BUFFER_SIZE];

// Define the timer period (in milliseconds)
#define TIMER_PERIOD_MS 1000

static void ble_app_advertise(void);

void print_addr(const void *addr)
{
    const uint8_t *u8p;

    u8p = addr;
    MODLOG_DFLT(INFO, "%02x:%02x:%02x:%02x:%02x:%02x",
                u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

static void ble_app_tx_stop(void)
{
    xTimerStop( ble_tx_timer, 1000 / portTICK_PERIOD_MS );
}

/* Reset heart rate measurement */
static void ble_app_tx_reset(void)
{
    assert(xTimerReset(ble_tx_timer, 1000 / portTICK_PERIOD_MS ) == pdPASS);
}

static void ble_app_tx_data(const void *buffer, uint16_t chr_val_handle)
{
    int rc;
    struct os_mbuf *om;

    om = ble_hs_mbuf_from_flat(buffer, strlen(buffer));
    rc = ble_gatts_notify_custom(conn_handle, chr_val_handle, om);

    assert(rc == 0);
}

/* This function simulates heart beat and notifies it to the client */
static void ble_app_tx_sensor_data(TimerHandle_t ev)
{
    if (!temperature_notify_state &&
        !pressure_notify_state &&
        !humidity_notify_state)
    {
        ble_app_tx_stop();
        temperature = 0.0;
        pressure = 0.0;
        humidity = 0.0;
        return;
    }

    if (temperature_notify_state)
    {
        float temperature = bmp280_get_temperature();
        ESP_LOGI("BLE_APP", "BMP280: Temperature: %.2f degC\n", temperature);

        // Format the data into a string and store it in the buffer
        snprintf(buffer, BUFFER_SIZE, "%.2f degC", temperature);

        ble_app_tx_data(buffer, temperature_handle);
    }

    if (pressure_notify_state)
    {
        float pressure = bmp280_get_pressure();
        ESP_LOGI("GAP", "BMP280: Pressure: %.2f hPa\n", pressure);

        // Format the data into a string and store it in the buffer
        snprintf(buffer, BUFFER_SIZE, "%.2f hPa", pressure);

        ble_app_tx_data(buffer, pressure_handle);
    }

    if (humidity_notify_state)
    {
        float humidity = bmp280_get_humidity();
        ESP_LOGI("GAP", "BMP280: Humidity: %.2f %%\n", humidity);

        // Format the data into a string and store it in the buffer
        snprintf(buffer, BUFFER_SIZE, "%.2f %%", humidity);

        ble_app_tx_data(buffer, humidity_handle);
    }

    ble_app_tx_reset();
}

// BLE event handling
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type)
    {
    // Advertise if connected
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT CONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        conn_handle = event->connect.conn_handle;
        break;
    // Disconnect event
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI("GAP", "BLE GAP EVENT DISCONNECT %s", event->connect.status == 0 ? "OK!" : "FAILED!");
        if (event->connect.status != 0)
        {
            ble_app_advertise();
        }
        break;
    // Advertise again after completion of the event
    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI("GAP", "BLE GAP EVENT");
        ble_app_advertise();
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        if (event->subscribe.attr_handle == temperature_handle)
        {
            temperature_notify_state = event->subscribe.cur_notify;
            ble_app_tx_reset();
        }
        else if (event->subscribe.attr_handle == pressure_handle)
        {
            pressure_notify_state = event->subscribe.cur_notify;
            ble_app_tx_reset();
        }
        else if (event->subscribe.attr_handle == humidity_handle)
        {
            humidity_notify_state = event->subscribe.cur_notify;
            ble_app_tx_reset();
        }
        else
        {
            ble_app_tx_stop();
        }
        break;
    case BLE_GAP_EVENT_MTU:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.value);
        break;
    case BLE_GAP_EVENT_NOTIFY_RX:
        MODLOG_DFLT(INFO, "mtu update event; conn_handle=%d mtu=%d\n",
                    event->mtu.conn_handle,
                    event->mtu.value);
        break;

    default:
        break;
    }
    return 0;
}

// Define the BLE connection
static void ble_app_advertise(void)
{
    // GAP - device name definition
    struct ble_hs_adv_fields fields;
    const char *device_name;
    memset(&fields, 0, sizeof(fields));
    device_name = ble_svc_gap_device_name(); // Read the BLE device name
    fields.name = (uint8_t *)device_name;
    fields.name_len = strlen(device_name);
    fields.name_is_complete = 1;
    ble_gap_adv_set_fields(&fields);

    // GAP - device connectivity definition
    struct ble_gap_adv_params adv_params;
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND; // connectable or non-connectable
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN; // discoverable or non-discoverable
    ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, ble_gap_event, NULL);
}

// The application
static void ble_app_on_sync(void)
{
    int rc;

    // Determines the best address type automatically
    rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    assert(rc == 0);

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(ble_addr_type, addr_val, NULL);

    MODLOG_DFLT(INFO, "Device Address: ");
    print_addr(addr_val);
    MODLOG_DFLT(INFO, "\n");

    /* Begin advertising */
    ble_app_advertise();
}

static void ble_app_on_reset(int reason)
{
    MODLOG_DFLT(INFO, "Resetting state; reason=%d\n", reason);
}

// The infinite task
static void ble_host_task(void *param)
{
    ESP_LOGI("BLE-APP", "BLE Host Task Started");
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();

    nimble_port_freertos_deinit();
}

void ble_init()
{
    int rc;

    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        MODLOG_DFLT(INFO, "Failed to init nimble %d \n", ret);
        return;
    }

    /* Initialize the NimBLE host configuration */
    ble_hs_cfg.sync_cb = ble_app_on_sync;
    ble_hs_cfg.reset_cb = ble_app_on_reset;

    /* name, period/time, auto reload, timer ID, callback */
    ble_tx_timer = xTimerCreate("ble_tx_timer", pdMS_TO_TICKS(TIMER_PERIOD_MS), pdTRUE, (void *)0, ble_app_tx_sensor_data);

    rc = gatt_svr_init();
    assert(rc == 0);

    /* Set the default device name */
    rc = ble_svc_gap_device_name_set(gap_device_name);
    assert(rc == 0);

    /* Start the task */
    nimble_port_freertos_init(ble_host_task);
}