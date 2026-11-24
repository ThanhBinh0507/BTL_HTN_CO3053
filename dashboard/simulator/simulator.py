"""
Smart Virtual School - Local Room Node Simulator (CO3053)
Bám sát 100% tài liệu thiết kế phần cứng, cảm biến và đếm người (CO3053):
  1. Hỗ trợ 3 biến thể Node (Mục 4.1):
     - Classroom (A1-201): Sức chứa 40, đủ cảm biến (ToF VL53L1X, radar mmWave LD2410C, SCD40, BH1750, INA226).
     - Lab (A1-LAB): Sức chứa 30, phòng máy tính (phụ tải điện ngầm cao hơn do dàn PC).
     - Corridor (A1-CORRIDOR): Hành lang (chỉ có mmWave + BH1750 + đèn LED, không đếm người, không quạt).
  2. Triển khai đầy đủ Schema MQTT (Mục 3):
     - .../telemetry : Gửi mẫu định kỳ 5s
     - .../event     : Báo sự kiện COUNT_RESET, OVERCAPACITY, WARNING, ALARM, CLEAR
     - .../cmd       : Nhận lệnh điều khiển đèn, quạt, mode, và ack_alarm
     - .../status    : Báo online/offline qua MQTT Last Will and Testament (LWT)
  3. FSM Cảnh báo 4 trạng thái (Mục 7.2):
     - NORMAL  : Đèn LED xanh sáng
     - WARNING : CO2 1000-1500, RH > 80%, quá tải sức chứa, lãng phí điện
     - ALARM   : T > 50°C hoặc CO2 > 1500 kéo dài. Ghi đè: Quạt tắt, Đèn bật 100% thoát hiểm. Cần ack_alarm để reset.
     - FAULT   : Lỗi cảm biến
"""

import json
import math
import random
import time
from datetime import datetime
import paho.mqtt.client as mqtt

BROKER_HOST = "broker.hivemq.com"
BROKER_PORT = 1883
INTERVAL_SEC = 5

class RoomNode:
    def __init__(self, building: str, floor: str, room: str, variant: str = "classroom", capacity: int = 40):
        self.building = building
        self.floor = floor
        self.room = room
        self.variant = variant          # "classroom", "lab", "corridor"
        self.capacity = capacity        # 40 (classroom), 30 (lab)
        self.topic_base = f"school/{building}/{floor}/{room}"

        # Cảm biến & Đo lường
        self.count = 25 if variant != "corridor" else 0
        self.presence = 1 if self.count > 0 else 0
        self.t = 28.2
        self.rh = 60.0
        self.co2 = 780.0
        self.lux = 250.0
        
        # Chấp hành (Actuators)
        self.light = "on"
        self.fan = "on" if variant != "corridor" else "off"
        self.mode = "auto"              # "auto" hoặc "manual"

        # Đo điện năng (INA226)
        self.p_w = 25.0
        self.e_wh = 100.0
        self.last_calc_time = time.time()

        # FSM Cảnh báo (Mục 7.2): NORMAL, WARNING, ALARM, FAULT
        self.alarm_state = "NORMAL"
        self.alarm_level = 0            # 0: NORMAL, 1: WARNING, 2: FAULT, 3: ALARM
        self.empty_timer_sec = 0
        self.co2_high_sec = 0

    def update(self, client: mqtt.Client):
        now = time.time()
        dt = (now - self.last_calc_time) / 3600.0
        self.last_calc_time = now

        # --- 1. MÔ PHỎNG ĐẾM NGƯỜI (VL53L1X ToF + mmWave LD2410C) ---
        if self.variant != "corridor":
            # Thay đổi ngẫu nhiên người ra vào
            if random.random() < 0.25:
                delta = random.choice([-2, -1, 0, 1, 2])
                self.count = max(0, min(self.capacity + 5, self.count + delta))
            
            # mmWave radar phát hiện người
            self.presence = 1 if self.count > 0 else (1 if random.random() < 0.05 else 0)

            # Quy tắc sửa trôi bằng mmWave (Mục 6.4)
            if self.presence == 0 and self.count > 0:
                self.empty_timer_sec += INTERVAL_SEC
                if self.empty_timer_sec >= 15: # mmWave báo trống liên tục -> reset count
                    self.count = 0
                    self.empty_timer_sec = 0
                    self.publish_event(client, "COUNT_RESET", "mmWave phat hien phong trong, reset bo dem ToF ve 0", 0)
            else:
                self.empty_timer_sec = 0

            # Cảnh báo quá tải phòng (Mục 5 & 10)
            if self.count > self.capacity:
                self.publish_event(client, "OVERCAPACITY", f"So nguoi ({self.count}) vuot qua suc chua ({self.capacity})", self.count)
        else:
            # Hành lang không đếm người, chỉ có người đi qua (mmWave)
            self.count = 0
            self.presence = 1 if (random.random() < 0.4) else 0

        # --- 2. CẢM BIẾN MÔI TRƯỜNG (SCD40, BH1750) ---
        # Nhiệt độ & Độ ẩm
        self.t = round(self.t + random.uniform(-0.15, 0.15), 1)
        self.t = max(24.0, min(35.0, self.t))
        self.rh = round(self.rh + random.uniform(-0.4, 0.4), 1)
        self.rh = max(40.0, min(85.0, self.rh))

        # CO2 SCD40
        if self.variant != "corridor":
            target_co2 = 450 + (self.count * 25) + random.uniform(-15, 15)
        else:
            target_co2 = 420.0
        self.co2 = round(self.co2 + (target_co2 - self.co2) * 0.1, 1)

        # Ánh sáng BH1750 (lux): ban ngày ~200-300 lux, cộng thêm 150 lux nếu đèn sáng
        ambient_lux = 180.0 + 80.0 * math.sin(time.time() / 50.0)
        self.lux = round(ambient_lux + (160.0 if self.light == "on" else 0.0), 1)

        # --- 3. FSM CẢNH BÁO (Mục 7.2) ---
        prev_alarm = self.alarm_state

        if self.t > 50.0:
            self.alarm_state = "ALARM"
            self.alarm_level = 3
        elif self.co2 > 1500.0:
            self.co2_high_sec += INTERVAL_SEC
            if self.co2_high_sec >= 15:
                self.alarm_state = "ALARM"
                self.alarm_level = 3
        elif self.co2 >= 1000.0 or self.rh > 80.0 or (self.count > self.capacity):
            if self.alarm_state != "ALARM":
                self.alarm_state = "WARNING"
                self.alarm_level = 1
        else:
            self.co2_high_sec = 0
            if self.alarm_state != "ALARM":
                self.alarm_state = "NORMAL"
                self.alarm_level = 0

        # Phát event khi chuyển đổi trạng thái FSM cảnh báo
        if self.alarm_state != prev_alarm:
            self.publish_event(client, f"ALARM_{self.alarm_state}", 
                               f"FSM chuyen trang thai sang {self.alarm_state} (CO2={self.co2}, T={self.t})", 
                               self.alarm_level)

        # --- 4. FSM ĐIỀU KHIỂN PHÒNG (Mục 7.1) VÀ GHI ĐÈ KHI ALARM ---
        if self.alarm_state == "ALARM":
            # Ghi đè FSM phòng: Quạt tắt (không thổi oxy), Đèn bật 100% để thoát hiểm!
            self.fan = "off"
            self.light = "on"
        elif self.mode == "auto":
            # REQ-03: Bật đèn khi có người và lux < 300
            if self.presence == 1 and self.lux < 300:
                self.light = "on"
            elif self.presence == 0:
                self.light = "off"

            # REQ-05: Quạt hysteresis (Bật khi >= 28°C, Tắt khi <= 27°C)
            if self.variant != "corridor":
                if self.t >= 28.0 and self.presence == 1:
                    self.fan = "on"
                elif self.t <= 27.0 or self.presence == 0:
                    self.fan = "off"
            else:
                self.fan = "off"

        # --- 5. TÍNH TOÁN ĐIỆN NĂNG (INA226 / PZEM-004T - Mục 5 & 10) ---
        base_power = 6.0 if self.variant != "lab" else 85.0 # Lab có dàn PC chạy ngầm
        light_power = 12.0 if self.light == "on" else 0.0
        fan_power = 18.0 if self.fan == "on" else 0.0
        self.p_w = round(base_power + light_power + fan_power + random.uniform(-0.5, 0.5), 1)
        self.e_wh = round(self.e_wh + (self.p_w * dt), 2)

        # Cảnh báo lãng phí điện: P > 5W trong khi phòng EMPTY
        if self.presence == 0 and self.p_w > 15.0 and self.variant != "lab":
            self.publish_event(client, "ENERGY_WASTE", f"Lang phi dien: P={self.p_w}W khi phong trong", self.p_w)

    def publish_telemetry(self, client: mqtt.Client):
        payload = {
            "count": int(self.count),
            "presence": int(self.presence),
            "t": float(self.t),
            "rh": float(self.rh),
            "co2": float(self.co2),
            "lux": float(self.lux),
            "p_w": float(self.p_w),
            "e_wh": float(self.e_wh),
            "light": self.light,
            "fan": self.fan,
            "mode": self.mode,
            "alarm_state": self.alarm_state,
            "alarm_level": self.alarm_level,
            "node_type": self.variant
        }
        topic = f"{self.topic_base}/telemetry"
        client.publish(topic, json.dumps(payload), qos=0)

    def publish_event(self, client: mqtt.Client, event_type: str, detail: str, val: float):
        payload = {
            "timestamp": datetime.now().isoformat(),
            "type": event_type,
            "detail": detail,
            "val": val
        }
        topic = f"{self.topic_base}/event"
        client.publish(topic, json.dumps(payload), qos=1)
        print(f" [!] EVENT -> [{self.room}] {event_type}: {detail}")

    def handle_command(self, client: mqtt.Client, cmd: dict):
        print(f"\n[CMD RECEIVED] -> Phong {self.room}: {cmd}")
        # Lệnh xác nhận báo động ack_alarm (Mục 7.2)
        if cmd.get("ack_alarm"):
            if self.alarm_state == "ALARM":
                self.alarm_state = "NORMAL"
                self.alarm_level = 0
                self.co2_high_sec = 0
                self.publish_event(client, "ALARM_ACK", "Nguoi quan tri da xac nhan bao dong (ack_alarm)", 0)
                print(f" -> [{self.room}] Da xac nhan bao dong! Reset FSM ve NORMAL.")

        if "mode" in cmd and cmd["mode"]:
            self.mode = cmd["mode"]
        if "light" in cmd and cmd["light"]:
            self.light = cmd["light"]
            if self.mode == "auto":
                self.mode = "manual"
        if "fan" in cmd and cmd["fan"] and self.variant != "corridor":
            self.fan = cmd["fan"]
            if self.mode == "auto":
                self.mode = "manual"


# Khởi tạo 3 biến thể Node theo Mục 4.1
ROOMS = {
    #"school/A1/2/A1-201": RoomNode("A1", "2", "A1-201", variant="classroom", capacity=40),
    "school/A1/2/A1-LAB": RoomNode("A1", "2", "A1-LAB", variant="lab", capacity=30),
    "school/A1/2/A1-CORRIDOR": RoomNode("A1", "2", "A1-CORRIDOR", variant="corridor", capacity=0)
}

def on_connect(client, userdata, flags, rc, *args):
    if rc == 0:
        print("[MQTT] Ket noi thanh cong toi Mosquitto Broker!")
        # Subscribe lệnh điều khiển
        client.subscribe("school/+/+/+/cmd")
        print("[MQTT] Da subscribe: school/+/+/+/cmd")

        # Báo Status ONLINE (Retained) cho từng phòng
        for topic_base in ROOMS.keys():
            status_topic = f"{topic_base}/status"
            client.publish(status_topic, "online", qos=1, retain=True)
            print(f"[MQTT] Published LWT Online: {status_topic}")
    else:
        print(f"[MQTT] Ket noi that bai, code={rc}")

def on_message(client, userdata, msg):
    try:
        parts = msg.topic.split('/')
        if len(parts) >= 5 and parts[4] == "cmd":
            room_key = "/".join(parts[:4])
            if room_key in ROOMS:
                cmd_data = json.loads(msg.payload.decode('utf-8'))
                ROOMS[room_key].handle_command(client, cmd_data)
    except Exception as e:
        print(f"[MQTT Error] {e}")

def main():
    print("=" * 70)
    print("   SMART VIRTUAL SCHOOL - CPS DIGITAL TWIN SIMULATOR (CO3053)")
    print(f"   Broker: {BROKER_HOST}:{BROKER_PORT} | Chu ky telemetry: {INTERVAL_SEC}s")
    print("   3 Bien the phong (Muc 4.1):")
    print("     1. A1-201     : Classroom (Suc chua 40, ToF, mmWave, SCD40, INA226)")
    print("     2. A1-LAB     : Computer Lab (Suc chua 30, phu tai dien cao)")
    print("     3. A1-CORRIDOR: Corridor (mmWave, BH1750, khong dem nguoi)")
    print("=" * 70)

    if hasattr(mqtt, "CallbackAPIVersion"):
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="smart-school-digital-twin")
    else:
        client = mqtt.Client(client_id="smart-school-digital-twin")

    # Cấu hình Last Will and Testament (LWT) cho topic chung
    client.will_set("school/A1/2/A1-LAB/status", "offline", qos=1, retain=True)

    client.on_connect = on_connect
    client.on_message = on_message

    connected = False
    retries = 0
    while not connected and retries < 10:
        try:
            client.connect(BROKER_HOST, BROKER_PORT, 60)
            connected = True
        except Exception:
            retries += 1
            print(f"[WARN] Dang cho broker tai {BROKER_HOST}:{BROKER_PORT} ({retries}/10)...")
            time.sleep(3)

    if not connected:
        print("[ERROR] Khong the ket noi MQTT! Hay chay start.bat de bat Docker truoc.")
        return

    client.loop_start()

    try:
        cycle = 0
        while True:
            cycle += 1
            t_str = datetime.now().strftime("%H:%M:%S")
            print(f"\n--- [Chu ky #{cycle} luc {t_str}] ---")

            for room in ROOMS.values():
                room.update(client)
                room.publish_telemetry(client)
                
                print(f"[{room.room:11s} ({room.variant:9s})] Cnt:{room.count:2d}/{room.capacity:<2d} | "
                      f"mmWave:{room.presence} | CO2:{room.co2:5.1f}ppm | T:{room.t}°C | "
                      f"P:{room.p_w:4.1f}W | FSM:{room.alarm_state:7s} | Light:{room.light:3s} Fan:{room.fan:3s}")

            time.sleep(INTERVAL_SEC)

    except KeyboardInterrupt:
        print("\n[STOP] Dung simulator...")
        for topic_base in ROOMS.keys():
            client.publish(f"{topic_base}/status", "offline", qos=1, retain=True)
    finally:
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()
