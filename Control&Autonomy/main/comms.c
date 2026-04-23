#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "comms.h"
#include "events.h"
#include "shared_state.h"
#include "state_machine.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_http_server.h"






#define WIFI_SSID "ESP32_APP"      /* AP name shown on phone */
#define WIFI_PASS "esp32pass"      /* AP password */
#define WIFI_CHANNEL 6              /* Wi-Fi channel */
#define MAX_STA_CONN 1              /* Max clients */

#define HTTP_PORT 80                /* HTTP server port */

/* Placeholder telemetry values */
#define ROVER_WEIGHT_LB 27.6f       /* Rover weight (lb) */
#define BATTERY_PERCENT 87          /* Battery life (%) */

static const char *TAG = "softap_http"; /* Log tag */

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static esp_err_t telemetry_get_handler(httpd_req_t *req);
static esp_err_t command_post_handler(httpd_req_t *req);
static void parse_command_body(const char *body);
static bool json_get_string(const char *body, const char *key, char *out, size_t out_len);
static bool string_to_manual_command(const char *str, ManualCommand_t *out);
static bool string_to_state(const char *str, State_t *out);
static bool string_to_event(const char *str, Event_t *out);


static bool string_to_state(const char *str, State_t *out)
{
    if (!str || !out) return false;

    if (strcmp(str, "STATE_IDLE") == 0 || strcmp(str, "IDLE") == 0) {
        *out = STATE_IDLE;
        return true;
    }
    if (strcmp(str, "STATE_MANUAL_DRIVE") == 0 || strcmp(str, "MANUAL_DRIVE") == 0) {
        *out = STATE_MANUAL_DRIVE;
        return true;
    }
    if (strcmp(str, "STATE_NAVIGATING") == 0 || strcmp(str, "NAVIGATING") == 0) {
        *out = STATE_NAVIGATING;
        return true;
    }
    if (strcmp(str, "STATE_ALARM") == 0 || strcmp(str, "ALARM") == 0) {
        *out = STATE_ALARM;
        return true;
    }
    if (strcmp(str, "STATE_EMERGENCY_STOP") == 0 || strcmp(str, "EMERGENCY_STOP") == 0) {
        *out = STATE_EMERGENCY_STOP;
        return true;
    }
    if (strcmp(str, "STATE_ERROR") == 0 || strcmp(str, "ERROR") == 0) {
        *out = STATE_ERROR;
        return true;
    }

    return false;
}


static bool string_to_event(const char *str, Event_t *out)
{
    if (!str || !out) return false;

    if (strcmp(str, "WAYPOINT_RECEIVED") == 0) {
        *out = EVENT_WAYPOINT_RECEIVED;
        return true;
    }
    if (strcmp(str, "WAYPOINT_REACHED") == 0) {
        *out = EVENT_WAYPOINT_REACHED;
        return true;
    }
    if (strcmp(str, "OBSTACLE_DETECTED") == 0) {
        *out = EVENT_OBSTACLE_DETECTED;
        return true;
    }
    if (strcmp(str, "OBSTACLE_CLEARED") == 0) {
        *out = EVENT_OBSTACLE_CLEARED;
        return true;
    }
    if (strcmp(str, "EMERGENCY_STOP") == 0) {
        *out = EVENT_EMERGENCY_STOP;
        return true;
    }
    if (strcmp(str, "EMERGENCY_CLEARED") == 0) {
        *out = EVENT_EMERGENCY_CLEARED;
        return true;
    }
    if (strcmp(str, "ERROR") == 0) {
        *out = EVENT_ERROR;
        return true;
    }
    if (strcmp(str, "WEIGHT_REMOVED") == 0 || strcmp(str, "EVENT_WEIGHT_REMOVED") == 0) {
        *out = EVENT_WEIGHT_REMOVED;
        return true;
    }
    if (strcmp(str, "WEIGHT_RESTORED") == 0 || strcmp(str, "EVENT_WEIGHT_RESTORED") == 0) {
        *out = EVENT_WEIGHT_RESTORED;
        return true;
    }

    return false;
}

/* Start the ESP32 as a Wi-Fi Access Point */
void wifi_init_softap(void)
{
    ESP_ERROR_CHECK(esp_netif_init());                 /* Init TCP/IP stack */
    ESP_ERROR_CHECK(esp_event_loop_create_default()); /* Init event loop */
    esp_netif_create_default_wifi_ap();                /* Create AP netif */

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT(); /* Default Wi-Fi config */
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));              /* Init Wi-Fi driver */

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL)); /* Register handler */

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = WIFI_SSID,
            .ssid_len = strlen(WIFI_SSID),
            .password = WIFI_PASS,
            .channel = WIFI_CHANNEL,
            .max_connection = MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    if (strlen(WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN; /* No password if empty */
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));           /* AP mode */
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config)); /* Apply config */
    ESP_ERROR_CHECK(esp_wifi_start());                           /* Start Wi-Fi */

    ESP_LOGI(TAG, "SoftAP started. SSID=%s password=%s channel=%d", WIFI_SSID, WIFI_PASS, WIFI_CHANNEL);
}


/* Handle Wi-Fi AP events */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg; 

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "Station joined, AID=%d", event->aid);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        state_machine_set_state(STATE_IDLE);
        set_manual_command(MANUAL_CMD_STOP);
        ESP_LOGI(TAG, "Rover forced to STATE_IDLE/stop (app disconnected)");
    }
}


/* HTTP GET handler for telemetry */
static esp_err_t telemetry_get_handler(httpd_req_t *req)
{
    char payload[160];
    
    State_t state = get_rover_state();
    const char *state_str = state_to_string(state);
    ManualCommand_t cmd = get_manual_command();
    const char *cmd_str = manual_command_to_string(cmd);

    

    int len = snprintf(payload,
                       sizeof(payload),
                       "{\"battery_pct\":%d,\"weight_lb\":%.1f,\"state\":\"%s\",\"command\":\"%s\"}\n",
                       BATTERY_PERCENT,
                       ROVER_WEIGHT_LB,
                       state_str,
                       cmd_str);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, payload, len);
    return ESP_OK;
}


/* HTTP POST handler for commands */
static esp_err_t command_post_handler(httpd_req_t *req)
{
    char body[256]; /* Request body */
    int total = req->content_len; /* Body length */
    if (total <= 0 || total >= (int)sizeof(body)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad body");
        return ESP_FAIL;
    }

    int received = 0; /* Bytes read so far */
    while (received < total) {
        int r = httpd_req_recv(req, body + received, total - received); /* Read chunk */
        if (r <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv failed");
            return ESP_FAIL;
        }
        received += r;
    }
    body[received] = '\0'; /* Null-terminate */

    parse_command_body(body); /* Update state */

    httpd_resp_set_type(req, "text/plain"); /* Plain response */
    httpd_resp_send(req, "OK\n", 3);        /* ACK */
    return ESP_OK;
}

/* Start HTTP server and register handlers */
void start_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); /* Default config */
    config.server_port = HTTP_PORT;                 /* Set port */

    httpd_handle_t server = NULL; /* Server handle */
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server start failed");
        return;
    }

    httpd_uri_t telemetry_uri = {
        .uri = "/telemetry",        /* GET endpoint */
        .method = HTTP_GET,          /* GET method */
        .handler = telemetry_get_handler,
        .user_ctx = NULL,
    };

    httpd_uri_t command_uri = {
        .uri = "/command",          /* POST endpoint */
        .method = HTTP_POST,         /* POST method */
        .handler = command_post_handler,
        .user_ctx = NULL,
    };

    httpd_register_uri_handler(server, &telemetry_uri); /* Register GET */
    httpd_register_uri_handler(server, &command_uri);   /* Register POST */

    ESP_LOGI(TAG, "HTTP server started on port %d", HTTP_PORT);
}


/* Parse a simple JSON command body */
/* Extract a quoted JSON string value for a given key */
static bool json_get_string(const char *body, const char *key, char *out, size_t out_len)
{
    if (out_len == 0) {
        return false;
    }

    /* Find key token */
    char key_token[32];
    snprintf(key_token, sizeof(key_token), "\"%s\"", key);
    const char *p = strstr(body, key_token);
    if (!p) {
        return false;
    }

    /* Find ':' after key */
    p = strchr(p + strlen(key_token), ':');
    if (!p) {
        return false;
    }
    p++; /* Move past ':' */

    /* Skip whitespace */
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
        p++;
    }

    /* Expect quoted string */
    if (*p != '"') {
        return false;
    }
    p++; /* Move past opening quote */

    /* Copy until closing quote or buffer full */
    size_t i = 0;
    while (*p && *p != '"' && i + 1 < out_len) {
        out[i++] = *p++;
    }
    out[i] = '\0';

    /* Require closing quote */
    return (*p == '"');
}

/* Parse a simple JSON command body */
static void parse_command_body(const char *body)
{
    /*
     * Supported examples:
     * {"state":"STATE_MANUAL_DRIVE","command":"forward"}
     * {"state":"STATE_IDLE"}
     * {"event":"EVENT_EMERGENCY_STOP"}
     * {"event":"EVENT_WAYPOINT_RECEIVED"}
     */

    char state_str[32] = {0};
    char event_str[40] = {0};
    char command[32] = {0};
    bool parsed = false;

    if (json_get_string(body, "state", state_str, sizeof(state_str))) {
        State_t state;
        if (string_to_state(state_str, &state)) {
            state_machine_set_state(state);
            ESP_LOGI(TAG, "Cmd updated: state=%s", state_to_string(state));
            parsed = true;
        } else {
            ESP_LOGW(TAG, "Invalid state: %s", state_str);
        }
    }

    if (json_get_string(body, "event", event_str, sizeof(event_str))) {
        Event_t event;
        if (string_to_event(event_str, &event)) {
            state_machine_handle_event(event);
            ESP_LOGI(TAG, "Event received: %s", event_str);
            parsed = true;
        } else {
            ESP_LOGW(TAG, "Invalid event: %s", event_str);
        }
    }
    /* Optional explicit alarm clear command from app */
    if (json_get_string(body, "alarm", command, sizeof(command))) {
        if (strcmp(command, "off") == 0 || strcmp(command, "clear") == 0) {
            state_machine_handle_event(EVENT_WEIGHT_RESTORED);
            ESP_LOGI(TAG, "Alarm cleared from app");
            parsed = true;
        } else {
            ESP_LOGW(TAG, "Invalid alarm command: %s", command);
        }
    }

    if (json_get_string(body, "command", command, sizeof(command))) {
        ManualCommand_t manual_cmd;
        if (string_to_manual_command(command, &manual_cmd)) {
            set_manual_command(manual_cmd);
            ESP_LOGI(TAG, "Cmd updated: command=%s", command);
            parsed = true;
        } else {
            ESP_LOGW(TAG, "Invalid manual command: %s", command);
        }
    }

    if (!parsed) {
        ESP_LOGW(TAG, "Unrecognized command body: %s", body);
    }
}


/* Allowlist for manual drive commands */
static bool string_to_manual_command(const char *str, ManualCommand_t *out)
{
    if (!str || !out) return false;

    if (strcmp(str, "stop") == 0) {
        *out = MANUAL_CMD_STOP;
        return true;
    }
    if (strcmp(str, "forward") == 0 || strcmp(str, "go") == 0) {
        *out = MANUAL_CMD_FORWARD;
        return true;
    }
    if (strcmp(str, "reverse") == 0) {
        *out = MANUAL_CMD_REVERSE;
        return true;
    }
    if (strcmp(str, "turnleft") == 0) {
        *out = MANUAL_CMD_TURNLEFT;
        return true;
    }
    if (strcmp(str, "turnright") == 0) {
        *out = MANUAL_CMD_TURNRIGHT;
        return true;
    }

    return false;
}
