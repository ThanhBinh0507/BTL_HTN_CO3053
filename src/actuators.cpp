#include "actuators.h"
#include "config.h"
#include <Arduino.h>

static QueueHandle_t s_evt_queue = NULL;
static volatile uint32_t last_isr_manual_ms = 0;
static volatile uint32_t last_isr_reset_ms = 0;

/* ISR nút nhấn MANUAL (có chống dội phím DEBOUNCE_MS) */
static void IRAM_ATTR isr_btn_manual(void) {
    uint32_t now = millis();
    if (now - last_isr_manual_ms >= DEBOUNCE_MS) {
        last_isr_manual_ms = now;
        if (s_evt_queue != NULL) {
            app_event_t ev = { .type = EVT_BTN_MANUAL_PRESSED, .param1 = 0, .param2 = 0 };
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(s_evt_queue, &ev, &xHigherPriorityTaskWoken);
            if (xHigherPriorityTaskWoken) {
                portYIELD_FROM_ISR();
            }
        }
    }
}

/* ISR nút nhấn RESET ALARM */
static void IRAM_ATTR isr_btn_reset(void) {
    uint32_t now = millis();
    if (now - last_isr_reset_ms >= DEBOUNCE_MS) {
        last_isr_reset_ms = now;
        if (s_evt_queue != NULL) {
            app_event_t ev = { .type = EVT_BTN_RESET_ALARM, .param1 = 0, .param2 = 0 };
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xQueueSendFromISR(s_evt_queue, &ev, &xHigherPriorityTaskWoken);
            if (xHigherPriorityTaskWoken) {
                portYIELD_FROM_ISR();
            }
        }
    }
}

void actuators_init(QueueHandle_t event_queue) {
    s_evt_queue = event_queue;

    pinMode(PIN_ACT_LIGHT, OUTPUT);
    pinMode(PIN_ACT_FAN, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    digitalWrite(PIN_ACT_LIGHT, LOW);
    digitalWrite(PIN_ACT_FAN, LOW);
    digitalWrite(PIN_LED_GREEN, HIGH);
    digitalWrite(PIN_LED_RED, LOW);
    noTone(PIN_BUZZER);

    /* Nút nhấn nối GND trong diagram.json nên dùng INPUT_PULLUP, kích cạnh xuống FALLING */
    pinMode(PIN_BTN_MANUAL, INPUT_PULLUP);
    pinMode(PIN_BTN_RESET_ALARM, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(PIN_BTN_MANUAL), isr_btn_manual, FALLING);
    attachInterrupt(digitalPinToInterrupt(PIN_BTN_RESET_ALARM), isr_btn_reset, FALLING);
}

void actuators_set_loads(bool light_on, bool fan_on) {
    digitalWrite(PIN_ACT_LIGHT, light_on ? HIGH : LOW);
    digitalWrite(PIN_ACT_FAN, fan_on ? HIGH : LOW);
}

void actuators_update_indicators(alarm_fsm_state_t alarm_state) {
    static uint8_t tick_cnt = 0;
    tick_cnt++; /* Hàm được gọi mỗi 250 ms */

    switch (alarm_state) {
        case ALARM_ST_NORMAL:
            /* LED xanh sáng liên tục, tắt đỏ & còi */
            digitalWrite(PIN_LED_GREEN, HIGH);
            digitalWrite(PIN_LED_RED, LOW);
            noTone(PIN_BUZZER);
            break;

        case ALARM_ST_WARNING:
            /* LED xanh nháy chậm (chu kỳ 1 s), tắt đỏ & còi */
            digitalWrite(PIN_LED_GREEN, (tick_cnt / 2) % 2 == 0 ? HIGH : LOW);
            digitalWrite(PIN_LED_RED, LOW);
            noTone(PIN_BUZZER);
            break;

        case ALARM_ST_FAULT:
            /* LED đỏ sáng liên tục báo lỗi phần cứng, tắt còi */
            digitalWrite(PIN_LED_GREEN, LOW);
            digitalWrite(PIN_LED_RED, HIGH);
            noTone(PIN_BUZZER);
            break;

        case ALARM_ST_ALARM:
            /* Còi hú + LED đỏ nháy 2 Hz (đổi trạng thái mỗi 250 ms) */
            digitalWrite(PIN_LED_GREEN, LOW);
            bool blink = (tick_cnt % 2 == 0);
            digitalWrite(PIN_LED_RED, blink ? HIGH : LOW);
            if (blink) {
                tone(PIN_BUZZER, 2000);
            } else {
                noTone(PIN_BUZZER);
            }
            break;
    }
}