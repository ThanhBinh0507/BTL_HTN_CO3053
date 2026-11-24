#include "sensors.h"
#include "config.h"
#include <Arduino.h>
#include <Wire.h>

#if SIMULATE_ON_WOKWI == 1
  #include <DHT.h>
  static DHT dht(PIN_DHT_DATA, DHT22);
#else
  #include <DHT20.h>
  #include <Adafruit_INA219.h>
  static DHT20 dht20;
  static Adafruit_INA219 ina219;
#endif

/* Bộ đếm lỗi liên tiếp để kích hoạt cờ FAULT (mục 5 tài liệu) */
static uint8_t err_cnt_dht   = 0;
static uint8_t err_cnt_co2   = 0;
static uint8_t err_cnt_lux   = 0;
static uint8_t err_cnt_power = 0;

void sensors_init(void) {
    pinMode(PIN_US_A_TRIG, OUTPUT);
    pinMode(PIN_US_A_ECHO, INPUT);
    pinMode(PIN_US_B_TRIG, OUTPUT);
    pinMode(PIN_US_B_ECHO, INPUT);
    pinMode(PIN_PIR_OUT, INPUT);

    analogReadResolution(12); /* ESP32 ADC 12-bit: 0 - 4095 */

#if SIMULATE_ON_WOKWI == 1
    dht.begin();
#else
    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
    dht20.begin();
    ina219.begin();
#endif
}

static uint16_t read_hcsr04_mm(uint8_t trig_pin, uint8_t echo_pin) {
    digitalWrite(trig_pin, LOW);
    delayMicroseconds(2);
    digitalWrite(trig_pin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig_pin, LOW);

    /* Timeout 12000 us (~2 mét) để không làm trễ chu kỳ 25 ms của counting_task */
    unsigned long duration = pulseIn(echo_pin, HIGH, 12000);
    if (duration == 0) {
        return 2000; /* Không có vật cản trong tầm 2 m */
    }
    return (uint16_t)((duration * 343UL) / 2000UL);
}

void sensors_read_door_mm(uint16_t *dist_a_mm, uint16_t *dist_b_mm) {
    *dist_a_mm = read_hcsr04_mm(PIN_US_A_TRIG, PIN_US_A_ECHO);
    delayMicroseconds(200); /* Tránh nhiễu sóng siêu âm chéo giữa 2 cảm biến */
    *dist_b_mm = read_hcsr04_mm(PIN_US_B_TRIG, PIN_US_B_ECHO);
}

bool sensors_read_presence(void) {
    return (digitalRead(PIN_PIR_OUT) == HIGH);
}

static void update_fault_flag(uint8_t *fault_flags, uint8_t bit, uint8_t *err_cnt, bool is_error) {
    if (is_error) {
        if (*err_cnt < SENSOR_MAX_ERR_COUNT) {
            (*err_cnt)++;
        }
        if (*err_cnt >= SENSOR_MAX_ERR_COUNT) {
            *fault_flags |= bit;
        }
    } else {
        *err_cnt = 0;
        *fault_flags &= ~bit;
    }
}

void sensors_read_fast(room_state_t *st, uint32_t dt_ms) {
    /* 1. Đọc cảm biến ánh sáng LDR (Wokwi LDR module: ánh sáng càng mạnh điện áp AO càng giảm) */
    int raw_ldr = analogRead(PIN_LDR_ADC);
    bool lux_err = (raw_ldr < 0 || raw_ldr > 4095);
    update_fault_flag(&st->fault_flags, FAULT_BIT_LUX, &err_cnt_lux, lux_err);
    if (!lux_err) {
        /* Ánh xạ ngược 4095..0 sang 0..1000 lux */
        st->illuminance_lux = ((4095.0f - (float)raw_ldr) / 4095.0f) * 1000.0f;
    }

    /* 2. Đọc điện áp, dòng điện & cộng dồn điện năng Wh */
#if SIMULATE_ON_WOKWI == 1
    int raw_cur = analogRead(PIN_CURRENT_ADC);
    bool pwr_err = (raw_cur < 0 || raw_cur > 4095);
    update_fault_flag(&st->fault_flags, FAULT_BIT_POWER, &err_cnt_power, pwr_err);
    if (!pwr_err) {
        st->voltage_v = 12.0f; /* Nguồn tải DC 12V theo thiết kế */
        /* Ánh xạ biến trở 0..4095 sang 0.0A..3.5A (để test được ngưỡng quá dòng > 2.5A) */
        st->current_a = ((float)raw_cur / 4095.0f) * 3.5f;
    }
#else
    float bus_v = ina219.getBusVoltage_V();
    float cur_ma = ina219.getCurrent_mA();
    bool pwr_err = isnan(bus_v) || isnan(cur_ma) || (bus_v < 0.0f);
    update_fault_flag(&st->fault_flags, FAULT_BIT_POWER, &err_cnt_power, pwr_err);
    if (!pwr_err) {
        st->voltage_v = bus_v;
        st->current_a = cur_ma / 1000.0f;
    }
#endif

    if (!(st->fault_flags & FAULT_BIT_POWER)) {
        st->power_w = st->voltage_v * st->current_a;
        /* Tích phân điện năng: E (Wh) += P (W) * dt (giờ) */
        st->energy_wh += st->power_w * ((float)dt_ms / 3600000.0f);
    }
}

void sensors_read_slow(room_state_t *st) {
    /* 1. Đọc nhiệt độ & độ ẩm */
    float t = NAN, rh = NAN;
#if SIMULATE_ON_WOKWI == 1
    t = dht.readTemperature();
    rh = dht.readHumidity();
    if (isnan(t) || isnan(rh)) {
        vTaskDelay(pdMS_TO_TICKS(50));
        t = dht.readTemperature();
        rh = dht.readHumidity();
    }
#else
    int status = dht20.read();
    if (status == DHT20_OK) {
        t = dht20.getTemperature();
        rh = dht20.getHumidity();
    }
#endif

    bool dht_err = isnan(t) || isnan(rh) || (t < -20.0f) || (t > 100.0f) || (rh < 0.0f) || (rh > 100.0f);
    update_fault_flag(&st->fault_flags, FAULT_BIT_DHT, &err_cnt_dht, dht_err);
    if (!dht_err) {
        st->temperature_c = t;
        st->humidity_rh = rh;
    }

    /* 2. Đọc nồng độ CO2 (ppm) */
    int raw_co2 = analogRead(PIN_CO2_ADC);
    /* Ánh xạ 0..4095 sang 400..2500 ppm */
    uint16_t co2 = 400 + (uint16_t)(((float)raw_co2 / 4095.0f) * 2100.0f);
    /* Theo mục 5 tài liệu: CO2 < 350 ppm là ngoài dải vật lý -> lỗi cảm biến */
    bool co2_err = (co2 < 350 || co2 > 5000);
    update_fault_flag(&st->fault_flags, FAULT_BIT_CO2, &err_cnt_co2, co2_err);
    if (!co2_err) {
        st->co2_ppm = co2;
    }
}