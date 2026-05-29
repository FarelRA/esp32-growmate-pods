#include "onboarding.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_config.h"
#include "app_build_config.h"
#include "cJSON.h"
#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "network_manager.h"

#define ONBOARDING_AP_PASSWORD "growmate"
#define ONBOARDING_FORM_BUFFER_SIZE 2048
#define ONBOARDING_COMPLETE_BIT BIT0

static const char *TAG = "onboarding";
static const char FAVICON_SVG[] =
    "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'>"
    "<rect width='64' height='64' rx='14' fill='#111827'/>"
    "<path d='M32 12c8 0 14 6 14 14 0 11-14 26-14 26S18 37 18 26c0-8 6-14 14-14Z' fill='#22c55e'/>"
    "<circle cx='32' cy='26' r='6' fill='#0f172a'/></svg>";

static const char INDEX_HTML[] =
    "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>GrowMate Setup</title><style>body{font-family:system-ui,sans-serif;margin:0;background:#0f172a;color:#e2e8f0}"
    ".wrap{max-width:760px;margin:0 auto;padding:24px}.card{background:#111827;border:1px solid #1f2937;border-radius:18px;padding:20px}"
    "h1{margin-top:0}label{display:block;margin:14px 0 6px}input,select{width:100%;padding:12px;border-radius:12px;border:1px solid #334155;background:#020617;color:#e2e8f0}"
    "button{margin-top:18px;padding:12px 16px;border:0;border-radius:12px;background:#22c55e;color:#052e16;font-weight:700;cursor:pointer}"
    "button.secondary{background:#1e293b;color:#e2e8f0}.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}.hint{color:#94a3b8;font-size:14px}"
    "#status{margin-top:16px;white-space:pre-wrap}</style></head><body><div class='wrap'><div class='card'><h1>GrowMate onboarding</h1>"
    "<p class='hint'>Connect this device to WiFi. Device identity and cloud-side configuration are managed by GrowMate, not by the end user.</p>"
    "<label>Device ID</label><input id='deviceId' disabled>"
    "<button class='secondary' id='scanBtn' type='button'>Scan WiFi</button><label>WiFi network</label><select id='ssid'></select>"
    "<label>WiFi password</label><input id='password' type='password' placeholder='WiFi password'>"
    "<button id='saveBtn' type='button'>Save and continue</button><div id='status' class='hint'></div></div></div>"
    "<script>async function getConfig(){const r=await fetch('/api/config');return r.json()}async function scan(){status('Scanning WiFi...');"
    "const r=await fetch('/api/networks');const items=await r.json();const el=document.getElementById('ssid');el.innerHTML='';"
    "items.networks.forEach(n=>{const o=document.createElement('option');o.value=n.ssid;o.textContent=`${n.ssid} (${n.rssi} dBm)`;el.appendChild(o)});"
    "if(!items.networks.length){const o=document.createElement('option');o.value='';o.textContent='No networks found';el.appendChild(o)}status('WiFi scan complete.')}"
    "function status(v){document.getElementById('status').textContent=v}function setValue(id,v){document.getElementById(id).value=v ?? ''}"
    "async function load(){const cfg=await getConfig();setValue('deviceId',cfg.deviceId);await scan()}"
    "async function save(){const body={wifiSsid:document.getElementById('ssid').value,wifiPassword:document.getElementById('password').value};status('Saving configuration...');"
    "const r=await fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});const json=await r.json();status(json.message || 'Saved');}"
    "document.getElementById('scanBtn').onclick=scan;document.getElementById('saveBtn').onclick=save;load();</script></body></html>";

typedef struct {
    app_config_t *config;
    EventGroupHandle_t event_group;
    httpd_handle_t server;
} onboarding_context_t;

static esp_err_t send_json(httpd_req_t *req, cJSON *json)
{
    char *payload = cJSON_PrintUnformatted(json);
    if (payload == NULL) {
        return ESP_ERR_NO_MEM;
    }

    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_sendstr(req, payload);
    free(payload);
    return err;
}

static esp_err_t read_request_body(httpd_req_t *req, char *buffer, size_t buffer_size)
{
    if (req->content_len >= buffer_size) {
        return ESP_ERR_INVALID_SIZE;
    }

    int received = 0;
    while (received < req->content_len) {
        int ret = httpd_req_recv(req, buffer + received, req->content_len - received);
        if (ret <= 0) {
            return ESP_FAIL;
        }
        received += ret;
    }

    buffer[received] = '\0';
    return ESP_OK;
}

static void json_copy_string(cJSON *root, const char *key, char *destination, size_t destination_size)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (cJSON_IsString(item) && item->valuestring != NULL) {
        strlcpy(destination, item->valuestring, destination_size);
    }
}

static esp_err_t handle_index(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_favicon(httpd_req_t *req)
{
    httpd_resp_set_type(req, "image/svg+xml");
    return httpd_resp_send(req, FAVICON_SVG, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_get_config(httpd_req_t *req)
{
    onboarding_context_t *context = (onboarding_context_t *) req->user_ctx;
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddStringToObject(root, "deviceId", APP_DEVICE_ID);
    cJSON_AddStringToObject(root, "wifiSsid", context->config->wifi_ssid);

    esp_err_t err = send_json(req, root);
    cJSON_Delete(root);
    return err;
}

static esp_err_t handle_scan_networks(httpd_req_t *req)
{
    network_scan_result_t results[NETWORK_MANAGER_MAX_SCAN_RESULTS] = {0};
    size_t count = 0;
    ESP_RETURN_ON_ERROR(network_manager_scan(results, NETWORK_MANAGER_MAX_SCAN_RESULTS, &count), TAG, "wifi scan failed");

    cJSON *root = cJSON_CreateObject();
    cJSON *networks = cJSON_CreateArray();
    if (root == NULL || networks == NULL) {
        cJSON_Delete(root);
        cJSON_Delete(networks);
        return ESP_ERR_NO_MEM;
    }

    cJSON_AddItemToObject(root, "networks", networks);
    for (size_t i = 0; i < count; ++i) {
        cJSON *entry = cJSON_CreateObject();
        if (entry == NULL) {
            continue;
        }
        cJSON_AddStringToObject(entry, "ssid", results[i].ssid);
        cJSON_AddNumberToObject(entry, "rssi", results[i].rssi);
        cJSON_AddNumberToObject(entry, "authMode", results[i].auth_mode);
        cJSON_AddItemToArray(networks, entry);
    }

    esp_err_t err = send_json(req, root);
    cJSON_Delete(root);
    return err;
}

static esp_err_t handle_save_config(httpd_req_t *req)
{
    onboarding_context_t *context = (onboarding_context_t *) req->user_ctx;
    char body[ONBOARDING_FORM_BUFFER_SIZE] = {0};
    ESP_RETURN_ON_ERROR(read_request_body(req, body, sizeof(body)), TAG, "failed to read onboarding request body");

    cJSON *root = cJSON_Parse(body);
    if (root == NULL) {
        httpd_resp_set_status(req, "400 Bad Request");
        return httpd_resp_sendstr(req, "{\"message\":\"Invalid JSON\"}");
    }

    app_config_t updated = *context->config;
    json_copy_string(root, "wifiSsid", updated.wifi_ssid, sizeof(updated.wifi_ssid));
    json_copy_string(root, "wifiPassword", updated.wifi_password, sizeof(updated.wifi_password));

    cJSON_Delete(root);
    updated.provisioned = true;
    app_config_sanitize(&updated);

    if (!app_config_is_complete(&updated)) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"message\":\"WiFi SSID is required\"}");
    }

    esp_err_t save_err = app_config_save(&updated);
    if (save_err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_set_type(req, "application/json");
        return httpd_resp_sendstr(req, "{\"message\":\"Failed to save configuration\"}");
    }
    *context->config = updated;
    xEventGroupSetBits(context->event_group, ONBOARDING_COMPLETE_BIT);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, "{\"message\":\"Configuration saved. Device will continue with the new settings.\"}");
}

static esp_err_t start_server(onboarding_context_t *context)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_RETURN_ON_ERROR(httpd_start(&context->server, &config), TAG, "failed to start onboarding web server");

    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(context->server, &(httpd_uri_t) {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_index,
        .user_ctx = context,
    }), TAG, "failed to register index handler");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(context->server, &(httpd_uri_t) {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = handle_favicon,
        .user_ctx = context,
    }), TAG, "failed to register favicon handler");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(context->server, &(httpd_uri_t) {
        .uri = "/api/config",
        .method = HTTP_GET,
        .handler = handle_get_config,
        .user_ctx = context,
    }), TAG, "failed to register config get handler");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(context->server, &(httpd_uri_t) {
        .uri = "/api/config",
        .method = HTTP_POST,
        .handler = handle_save_config,
        .user_ctx = context,
    }), TAG, "failed to register config post handler");
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(context->server, &(httpd_uri_t) {
        .uri = "/api/networks",
        .method = HTTP_GET,
        .handler = handle_scan_networks,
        .user_ctx = context,
    }), TAG, "failed to register network scan handler");

    return ESP_OK;
}

esp_err_t onboarding_run(app_config_t *config)
{
    EventGroupHandle_t event_group = xEventGroupCreate();
    if (event_group == NULL) {
        return ESP_ERR_NO_MEM;
    }

    char ap_name[33];
    snprintf(ap_name, sizeof(ap_name), "GrowMate-%s", APP_DEVICE_ID + (strlen(APP_DEVICE_ID) > 6 ? strlen(APP_DEVICE_ID) - 6 : 0));

    onboarding_context_t context = {
        .config = config,
        .event_group = event_group,
        .server = NULL,
    };

    ESP_LOGW(TAG, "Starting onboarding access point %s", ap_name);
    esp_err_t err = network_manager_start_onboarding_ap(ap_name, ONBOARDING_AP_PASSWORD);
    if (err != ESP_OK) {
        vEventGroupDelete(event_group);
        return err;
    }

    err = start_server(&context);
    if (err != ESP_OK) {
        network_manager_stop();
        vEventGroupDelete(event_group);
        return err;
    }

    xEventGroupWaitBits(event_group, ONBOARDING_COMPLETE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);

    if (context.server != NULL) {
        httpd_stop(context.server);
    }
    network_manager_stop();
    vEventGroupDelete(event_group);
    return ESP_OK;
}
