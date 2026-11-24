#include "mqtt_comm.h"
#include "config.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient s_wifi_client;
static PubSubClient s_mqtt(s_wifi_client);
static QueueHandle_t s_evt_queue = NULL;

/* Ring Buffer lưu tối đa 120 bản tin telemetry (10 phút * 12 bản tin/phút) khi mất mạng */
static char s_ring_buf[OFFLINE_BUFFER_SIZE][256];
static uint16_t s_rb_head = 0;
static uint16_t s_rb_tail = 0;
static uint16_t s_rb_count = 0;

static void rb_push(const char *json_str) {
    strncpy(s_ring_buf[s_rb_head], json_str, 255);
    s_ring_buf[s_rb_head][255] = '\0';
    s_rb_head = (s_rb_head + 1) % OFFLINE_BUFFER_SIZE;
    if (s_rb_count < OFFLINE_BUFFER_SIZE) {
        s_rb_count++;
    } else {
        /* Tràn bộ đệm -> ghi đè bản ghi cũ nhất */
        s_rb_tail = (s_rb_tail + 1) % OFFLINE_BUFFER_SIZE;
    }
}

static bool rb_pop(char *out_buf) {
    if (s_rb_count == 0) return false;
    strcpy(out_buf, s_ring_buf[s_rb_tail]);
    s_rb_tail = (s_rb_tail + 1) % OFFLINE_BUFFER_SIZE;
    s_rb_count--;
    return true;
}

/* Callback xử lý lệnh từ Node-RED / Grafana gửi xuống topic .../cmd */
static void on_mqtt_message(char *topic, byte *payload, unsigned int length) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err || s_evt_queue == NULL) return;

    /* 1. Lệnh xác nhận tắt báo động */
    if ((doc["ack_alarm"].is<bool>() && doc["ack_alarm"].as<bool>() == true) ||
        (doc["ack_alarm"].is<int>() && doc["ack_alarm"].as<int>() == 1)) {
        app_event_t ev = { .type = EVT_BTN_RESET_ALARM, .param1 = 0, .param2 = 0 };
        xQueueSend(s_evt_queue, &ev, 0);
    }

    /* 2. Lệnh điều khiển đèn / quạt (-1: giữ nguyên, 0: off, 1: on, 2: auto) */
    int light_cmd = -1;
    int fan_cmd = -1;

    if (doc["light"].is<const char *>()) {
        const char *l = doc["light"];
        if (strcmp(l, "off") == 0) light_cmd = 0;
        else if (strcmp(l, "on") == 0) light_cmd = 1;
        else if (strcmp(l, "auto") == 0) light_cmd = 2;
    }

    if (doc["fan"].is<const char *>()) {
        const char *f = doc["fan"];
        if (strcmp(f, "off") == 0) fan_cmd = 0;
        else if (strcmp(f, "on") == 0) fan_cmd = 1;
        else if (strcmp(f, "auto") == 0) fan_cmd = 2;
    }

    if (light_cmd != -1 || fan_cmd != -1) {
        app_event_t ev = { .type = EVT_MQTT_CMD_RECEIVED, .param1 = light_cmd, .param2 = fan_cmd };
        xQueueSend(s_evt_queue, &ev, 0);
    }
}

void mqtt_comm_init(QueueHandle_t event_queue) {
    s_evt_queue = event_queue;

    WiFi.mode(WIFI_STA);
    /* Chỉ định kênh 6 giúp ESP32 trên Wokwi bắt được Wokwi-GUEST ngay lập tức */
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 6);

    s_mqtt.setServer(MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    s_mqtt.setCallback(on_mqtt_message);
    s_mqtt.setBufferSize(512);
}

static void ensure_connected(void) {
    static uint32_t last_retry_ms = 0;
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - last_retry_ms > 3000) {
            last_retry_ms = millis();
            Serial.printf("[WIFI] Dang ket noi %s (status=%d)...\r\n", WIFI_SSID, WiFi.status());
        }
        return;
    }

    if (!s_mqtt.connected()) {
        if (millis() - last_retry_ms > 3000) {
            last_retry_ms = millis();
            String client_id = "RoomNode-" + String(ROOM_ID) + "-" + String(random(0xffff), HEX);
            Serial.printf("[MQTT] Dang ket noi %s:%d...\r\n", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
            if (s_mqtt.connect(client_id.c_str(), NULL, NULL, TOPIC_STATUS, 1, true, "offline")) {
                Serial.println("[MQTT] Ket noi thanh cong! Da gui status ONLINE.");
                s_mqtt.publish(TOPIC_STATUS, "online", true);
                s_mqtt.subscribe(TOPIC_CMD);
            } else {
                Serial.printf("[MQTT] Ket noi that bai, rc=%d\r\n", s_mqtt.state());
            }
        }
    }
}

void mqtt_comm_loop(void) {
    ensure_connected();
    if (s_mqtt.connected()) {
        s_mqtt.loop();

        /* Đẩy bù tối đa 5 bản ghi/lần từ Ring Buffer để không làm nghẽn task */
        char cached_msg[256];
        uint8_t flushed = 0;
        while (s_rb_count > 0 && flushed < 5) {
            if (rb_pop(cached_msg)) {
                if (!s_mqtt.publish(TOPIC_TELEMETRY, cached_msg)) {
                    rb_push(cached_msg); /* Gửi lỗi thì cất lại */
                    break;
                }
                flushed++;
            }
        }
    }
}

void mqtt_comm_publish_telemetry(const room_state_t *st) {
    const char *alarm_str = "NORMAL";
    int alarm_lvl = 0;
    if (st->alarm_state == ALARM_ST_WARNING) { alarm_str = "WARNING"; alarm_lvl = 1; }
    else if (st->alarm_state == ALARM_ST_FAULT) { alarm_str = "FAULT"; alarm_lvl = 2; }
    else if (st->alarm_state == ALARM_ST_ALARM) { alarm_str = "ALARM"; alarm_lvl = 3; }

    JsonDocument doc;
    doc["count"]       = st->people_count;
    doc["presence"]    = st->presence_detected ? 1 : 0;
    doc["t"]           = round(st->temperature_c * 10.0f) / 10.0f;
    doc["rh"]          = round(st->humidity_rh * 10.0f) / 10.0f;
    doc["co2"]         = st->co2_ppm;
    doc["lux"]         = round(st->illuminance_lux);
    doc["p_w"]         = round(st->power_w * 100.0f) / 100.0f;
    doc["e_wh"]        = round(st->energy_wh * 1000.0f) / 1000.0f;
    doc["light"]       = st->light_on ? "on" : "off";
    doc["fan"]         = st->fan_on ? "on" : "off";
    doc["mode"]        = (st->mode == CTRL_MODE_AUTO) ? "auto" : "manual";
    doc["alarm_state"] = alarm_str;
    doc["alarm_level"] = alarm_lvl;
    doc["node_type"]   = "classroom";

    char payload[320];
    serializeJson(doc, payload, sizeof(payload));

    if (s_mqtt.connected()) {
        if (!s_mqtt.publish(TOPIC_TELEMETRY, payload)) {
            rb_push(payload);
        }
    } else {
        rb_push(payload);
    }
}

void mqtt_comm_publish_event(const char *ev_type, const char *detail, int32_t value) {
    if (!s_mqtt.connected()) return;

    JsonDocument doc;
    doc["room"]   = ROOM_ID;
    doc["type"]   = ev_type;
    doc["detail"] = detail;
    doc["val"]    = value;

    char payload[192];
    serializeJson(doc, payload, sizeof(payload));
    s_mqtt.publish(TOPIC_EVENT, payload);
}