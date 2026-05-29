#include "network_manager.h"

#include <string.h>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static const char *TAG = "network";
static EventGroupHandle_t s_wifi_event_group;
static esp_event_handler_instance_t s_any_id_handler;
static esp_event_handler_instance_t s_got_ip_handler;
static esp_netif_t *s_sta_netif;
static esp_netif_t *s_ap_netif;
static bool s_initialized;
static bool s_reconnect_enabled;
static int s_retry_count;
static int s_retry_limit;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        return;
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_reconnect_enabled && s_retry_count < s_retry_limit) {
            s_retry_count++;
            ESP_LOGW(TAG, "WiFi reconnect attempt %d/%d", s_retry_count, s_retry_limit);
            esp_wifi_connect();
            return;
        }

        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        return;
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        s_retry_count = 0;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "Station connected with IP " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

esp_err_t network_manager_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &s_any_id_handler));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &s_got_ip_handler));

    s_initialized = true;
    return ESP_OK;
}

static esp_err_t network_manager_stop_checked(void)
{
    esp_err_t err = network_manager_stop();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop WiFi: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t network_manager_stop(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    s_reconnect_enabled = false;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT || err == ESP_ERR_WIFI_NOT_STARTED) {
        return ESP_OK;
    }

    return err;
}

esp_err_t network_manager_start_station(const app_config_t *config, uint32_t timeout_ms)
{
    ESP_RETURN_ON_ERROR(network_manager_stop_checked(), TAG, "failed to stop wifi before station start");

    wifi_config_t wifi_config = {0};
    strlcpy((char *) wifi_config.sta.ssid, config->wifi_ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *) wifi_config.sta.password, config->wifi_password, sizeof(wifi_config.sta.password));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    s_retry_limit = 4;
    s_retry_count = 0;
    s_reconnect_enabled = true;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    if ((bits & WIFI_CONNECTED_BIT) != 0) {
        return ESP_OK;
    }

    if ((bits & WIFI_FAIL_BIT) != 0) {
        return ESP_FAIL;
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t network_manager_start_onboarding_ap(const char *ap_name, const char *ap_password)
{
    ESP_RETURN_ON_ERROR(network_manager_stop_checked(), TAG, "failed to stop wifi before onboarding ap");

    wifi_config_t ap_config = {0};
    strlcpy((char *) ap_config.ap.ssid, ap_name, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen(ap_name);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;

    if (ap_password != NULL && strlen(ap_password) >= 8) {
        strlcpy((char *) ap_config.ap.password, ap_password, sizeof(ap_config.ap.password));
        ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    }

    s_reconnect_enabled = false;
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "Onboarding AP started: %s", ap_name);
    return ESP_OK;
}

esp_err_t network_manager_scan(network_scan_result_t *results, size_t max_results, size_t *result_count)
{
    wifi_scan_config_t scan_config = {
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    uint16_t found = 0;

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&found));

    if (found == 0) {
        *result_count = 0;
        return ESP_OK;
    }

    wifi_ap_record_t scan_results[NETWORK_MANAGER_MAX_SCAN_RESULTS] = {0};
    uint16_t desired = found > max_results ? max_results : found;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&desired, scan_results));

    for (uint16_t i = 0; i < desired; ++i) {
        strlcpy(results[i].ssid, (const char *) scan_results[i].ssid, sizeof(results[i].ssid));
        results[i].rssi = scan_results[i].rssi;
        results[i].auth_mode = scan_results[i].authmode;
    }

    *result_count = desired;
    return ESP_OK;
}
