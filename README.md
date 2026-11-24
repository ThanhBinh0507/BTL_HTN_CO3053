# Smart Virtual School — Hệ thống Giám sát & Điều khiển Phòng học Thông minh (CPS Digital Twin)

> **Mã môn học:** CO3053 — Thiết kế Hệ thống Nhúng / IoT  
> **Trạng thái:** Đã tích hợp hoàn chỉnh Firmware (ESP32 FreeRTOS) & Hệ thống Dashboard (Docker + Node-RED + InfluxDB + Grafana)

---

## 1. Ý tưởng & Mục tiêu Đề tài

**Smart Virtual School** là hệ thống quản lý cơ sở vật chất trường học thông minh được xây dựng theo kiến trúc **Cyber-Physical System (CPS) & Digital Twin**. Hệ thống giải quyết bài toán tự động hóa điều khiển thiết bị điện (đèn, quạt) theo sự hiện diện thực tế của sinh viên, tối ưu hóa điện năng tiêu thụ, giám sát chất lượng không khí trong lớp học và cảnh báo an toàn cháy nổ theo thời gian thực.

Để tối ưu chi phí triển khai nhưng vẫn đảm bảo tính trực quan toàn diện ở quy mô tòa nhà, hệ thống áp dụng mô hình **mô phỏng lai (Hybrid Digital Twin)**:
* **Node Vật lý/Mạch ảo chi tiết (`A1-201`):** Mô phỏng đầy đủ vi điều khiển ESP32, các cảm biến và cơ cấu chấp hành trên nền tảng **Wokwi**.
* **Các Node Phòng học Ảo (`A1-LAB`, `A1-CORRIDOR`):** Giả lập song song các biến thể không gian khác nhau (phòng máy tính tải cao, hành lang) bằng **Python Simulator**, đổ dữ liệu đồng thời về một trung tâm giám sát duy nhất.

---

## 2. Kiến trúc Tổng thể & Luồng Hoạt động (Workflow)

```text
+-----------------------------------+       +-----------------------------------+
|   WOKWI ESP32 NODE (A1-201)       |       |   PYTHON SIMULATOR                |
|  - FreeRTOS (5 Tasks, Dual-Core)  |       |  - A1-LAB (Computer Lab)          |
|  - People Counter + Room/Alarm FSM|       |  - A1-CORRIDOR (Hành lang)        |
+-----------------+-----------------+       +-----------------+-----------------+
                  |                                           |
                  |   MQTT (JSON: telemetry / event / status) |
                  +---------------------+---------------------+
                                        |
                                        v
                        +-------------------------------+
                        |   MQTT BROKER                 |
                        |   (broker.hivemq.com:1883)    |
                        +---------------+---------------+
                                        |
                         Subscribe: school/+/+/+/+
                         Publish:   school/{b}/{f}/{r}/cmd
                                        |
                                        v
                        +-------------------------------+
                        |   NODE-RED (Port 1880)        |
                        |  - Chuẩn hóa dữ liệu JSON     |
                        |  - API điều khiển 2 chiều     |
                        +---------------+---------------+
                                        |
                                        v
                        +-------------------------------+
                        |   INFLUXDB 2.7 (Port 8086)    |
                        |  - Bucket: smart_school_data  |
                        +---------------+---------------+
                                        |
                                        v
                        +-------------------------------+
                        |   GRAFANA DASHBOARD (Port 3000|
                        |  - Biểu đồ thời gian thực     |
                        |  - Nhật ký sự kiện & Cảnh báo |
                        +-------------------------------+
```

**Luồng xử lý dữ liệu:**
1. **Thu thập & Điều khiển tại biên (Edge Processing):** ESP32 đọc cảm biến, chạy các máy trạng thái (FSM) để tự động bật/tắt đèn, quạt và kích hoạt báo động tại chỗ trong thời gian thực mà không phụ thuộc vào độ trễ mạng.
2. **Truyền thông Hai chiều (Telemetry & Command):** Dữ liệu định kỳ được đóng gói JSON gửi qua MQTT Broker. Ngược lại, quản trị viên có thể gửi lệnh điều khiển thiết bị hoặc xác nhận tắt báo động (`ack_alarm`) từ server xuống từng phòng.
3. **Lưu trữ & Trực quan hóa:** Node-RED tiếp nhận gói tin, phân loại theo từng phòng (`room` tag) và lưu vào cơ sở dữ liệu chuỗi thời gian InfluxDB. Grafana truy vấn bằng ngôn ngữ Flux để hiển thị trạng thái toàn trường.

---

## 3. Cấu trúc Thư mục Dự án

```text
Smart_Virtual_School/
├── README.md                   # Tài liệu tổng hợp, kiến trúc và hướng dẫn nghiệm thu
├── platformio.ini              # Cấu hình biên dịch PlatformIO (ESP32 Arduino Core 3.x)
├── wokwi.toml                  # Cấu hình liên kết firmware với trình mô phỏng Wokwi
├── diagram.json                # Sơ đồ đấu nối phần cứng ESP32-DevKitC-V4 và cảm biến
├── include/
│   ├── config.h                # Định nghĩa chân GPIO, ngưỡng cảm biến, Wi-Fi & MQTT Topics
│   └── app_types.h             # Cấu trúc dữ liệu trạng thái phòng, sự kiện và hàng đợi
├── lib/                        # Thư viện lõi xử lý nghiệp vụ (Core FSMs)
│   ├── people_counter/         # Thuật toán đếm người 2 chiều và chống trôi
│   ├── room_fsm/               # Máy trạng thái điều khiển tải phòng học
│   └── alarm_fsm/              # Máy trạng thái cảnh báo 4 mức và chốt an toàn
├── src/                        # Mã nguồn hệ thống & giao tiếp phần cứng
│   ├── main.cpp                # Khởi tạo FreeRTOS đa nhiệm (5 Tasks) & Hardware Watchdog
│   ├── sensors.cpp / .h        # Đọc cảm biến, lọc nhiễu và phát hiện lỗi phần cứng
│   ├── actuators.cpp / .h      # Điều khiển rơ-le đèn, quạt, còi buzzer và LED trạng thái
│   └── mqtt_comm.cpp / .h      # Giao tiếp MQTT 2 chiều, LWT và Ring Buffer ngoại tuyến
├── test/                       # Mã nguồn kiểm thử đơn vị (Unit Test) cho các FSM
└── dashboard/                  # Cụm hạ tầng Backend & Digital Twin (Docker + Simulator)
    ├── docker-compose.yml      # Khởi chạy 4 services: Mosquitto, InfluxDB, Node-RED, Grafana
    ├── start.bat / stop.bat    # Script khởi chạy/dừng nhanh trên Windows
    ├── run_simulator.bat       # Script chạy bộ giả lập phòng ảo trên Windows
    ├── test_cmd.bat            # Script kiểm thử gửi lệnh điều khiển từ xa
    ├── mosquitto/              # Cấu hình MQTT Broker nội bộ (port 1883 / 9001)
    ├── nodered/                # Luồng xử lý dữ liệu (flows.json) & HTTP API /api/cmd
    ├── grafana/                # Cấu hình Datasource InfluxDB & Dashboard JSON tự động nạp
    └── simulator/              # simulator.py giả lập phòng máy (A1-LAB) & hành lang (A1-CORRIDOR)
```

---

## 4. Kiến trúc Firmware ESP32 (6 Modules)

Phần firmware trên ESP32 được thiết kế theo kiến trúc hướng sự kiện (Event-Driven) kết hợp hệ điều hành thời gian thực **FreeRTOS**, chia thành 6 module chuyên biệt:

### Module 1: Điều phối Đa nhiệm FreeRTOS & Giám sát An toàn (`src/main.cpp`, `include/`)
* Phân bổ **5 tác vụ (Tasks)** chạy song song trên 2 lõi CPU của ESP32:
  * `counting_task` (Core 1, chu kỳ $25\text{ ms}$, ưu tiên cao): Quét liên tục cảm biến cửa ra/vào để không bỏ lỡ bước chân người đi qua.
  * `control_task` (Core 1): Lắng nghe `Event Queue` để cập nhật các FSM và xuất tín hiệu điều khiển ngay lập tức.
  * `sensor_task` (Core 0): Đọc các cảm biến môi trường chậm, tách biệt khỏi Core 1 để tránh xung đột định thời giao tiếp 1-Wire của DHT22.
  * `comm_task` (Core 0): Duy trì kết nối Wi-Fi/MQTT và gửi bản tin định kỳ mỗi $5\text{ s}$.
  * `health_task` (Watchdog): Kiểm tra cờ nhịp tim (`g_task_heartbeat`) của cả 4 tác vụ trên; chỉ nạp lại **Hardware Watchdog (`esp_task_wdt`)** khi toàn bộ hệ thống hoạt động bình thường (tự động reset vi điều khiển nếu treo quá $10\text{ s}$).

### Module 2: Thu thập Dữ liệu Cảm biến & Chẩn đoán Lỗi (`src/sensors.cpp`)
* Đọc và chuẩn hóa giá trị từ các cảm biến mô phỏng trên Wokwi:
  * **DHT22** (mô phỏng SCD40): Nhiệt độ ($^\circ\text{C}$) và độ ẩm ($\%\text{RH}$), tích hợp cơ chế đọc lặp lại (retry) khi gặp nhiễu xung `NaN`.
  * **Cảm biến $\text{CO}_2$ & Quang trở LDR** (mô phỏng BH1750): Nồng độ $\text{CO}_2$ ($\text{ppm}$) và cường độ sáng ($\text{lux}$).
  * **2 Cảm biến siêu âm HC-SR04** (mô phỏng ToF VL53L1X): Đo khoảng cách tại 2 vùng cửa A và B.
  * **Biến trở tại chân `GPIO36 (VP)`** (mô phỏng INA226): Đo công suất tức thời $P$ ($\text{W}$) và tích lũy điện năng tiêu thụ $E$ ($\text{Wh}$).
* Xuất mã lỗi phần cứng thông qua bitmask (`FAULT_BIT_DHT`, `FAULT_BIT_TOF`, ...) nếu cảm biến mất phản hồi nhiều chu kỳ liên tiếp.

### Module 3: Thuật toán Đếm người Hai chiều & Chống trôi (`lib/people_counter`)
* Phân tích trình tự ngắt tia tại hai vùng cảm biến A và B để phân biệt hướng **Đi vào ($A \rightarrow B$: tăng biến đếm)** và **Đi ra ($B \rightarrow A$: giảm biến đếm)**.
* Tích hợp tín hiệu cảm biến hiện diện (mmWave): Nếu trạng thái hiện diện bằng $0$ liên tục trong một khoảng thời gian quy định nhưng biến đếm vẫn lớn hơn $0$, hệ thống tự động đặt lại biến đếm về $0$ và phát sự kiện `COUNT_RESET` để triệt tiêu sai số tích lũy.

### Module 4: Máy trạng thái Điều khiển Phòng học (`lib/room_fsm`)
* Quản lý trạng thái sử dụng phòng giữa hai chế độ **`EMPTY`** (Trống) và **`OCCUPIED`** (Có người), hỗ trợ chuyển đổi linh hoạt giữa chế độ tự động (`AUTO`) và điều khiển tay từ xa (`MANUAL`).
* **Điều khiển chiếu sáng:** Tự động bật đèn khi có người hiện diện và cường độ sáng môi trường $< 300\text{ lux}$; tự tắt đèn khi phòng trống.
* **Điều khiển quạt theo cơ chế trễ nhiệt (Hysteresis):** Bật quạt khi có người và nhiệt độ $T \ge 28^\circ\text{C}$; chỉ tắt quạt khi nhiệt độ giảm xuống $T \le 27^\circ\text{C}$ hoặc phòng trống, giúp tránh đóng/ngắt rơ-le liên tục tại điểm ngưỡng.

### Module 5: Máy trạng thái Cảnh báo 4 Mức & Chốt An toàn (`lib/alarm_fsm`)
* Giám sát an toàn theo 4 cấp độ ưu tiên:
  1. **`NORMAL`:** Mọi thông số nằm trong ngưỡng an toàn (LED xanh sáng).
  2. **`WARNING`:** Nồng độ $\text{CO}_2 \ge 1000\text{ ppm}$, độ ẩm $> 80\%$, hoặc số người vượt sức chứa cho phép.
  3. **`FAULT`:** Phát hiện lỗi phần cứng cảm biến.
  4. **`ALARM` (Khẩn cấp):** Nhiệt độ $T > 50^\circ\text{C}$ (nguy cơ cháy) hoặc $\text{CO}_2 > 1500\text{ ppm}$ kéo dài.
* **Cơ chế cưỡng bức & Chốt báo động (Alarm Latching):** Khi vào trạng thái `ALARM`, hệ thống tự động **ngắt quạt** (tránh thổi thêm oxy vào đám cháy), **bật đèn $100\%$** hỗ trợ thoát hiểm, hú còi Buzzer và nháy đèn đỏ tần số $2\text{ Hz}$. Báo động được giữ chốt ngay cả khi nhiệt độ đã hạ xuống, chỉ tắt khi nhận xác nhận thủ công qua nút bấm **`RESET`** trên mạch hoặc lệnh `ack_alarm` từ Dashboard.

### Module 6: Truyền thông MQTT Hai chiều & Bộ đệm Ngoại tuyến (`src/mqtt_comm.cpp`, `src/actuators.cpp`)
* Kết nối Wi-Fi (chỉ định kênh 6 để tối ưu tốc độ bắt sóng trên Wokwi) tới MQTT Broker, đăng ký **Last Will and Testament (LWT)** để tự động báo `offline` nếu node mất điện hoặc rớt mạng đột ngột.
* Tích hợp **Ring Buffer (bộ đệm vòng)** lưu trữ tối đa $120$ bản tin telemetry trong RAM khi mất kết nối mạng; ngay khi khôi phục kết nối, hệ thống tự động đẩy bù toàn bộ dữ liệu cũ lên server để đảm bảo tính liên tục của biểu đồ.

---

## 5. Hạ tầng Dashboard & Digital Twin (`dashboard/`)

Thư mục `dashboard/` đóng gói toàn bộ hệ thống phía server dưới dạng container Docker để triển khai nhanh trên mọi máy tính:

1. **Mosquitto (`dashboard/mosquitto/`):** MQTT Broker dự phòng chạy nội bộ tại cổng `1883` (TCP) và `9001` (WebSocket).
2. **Node-RED (`dashboard/nodered/`):**
   * Kết nối tới `broker.hivemq.com:1883`, lắng nghe toàn bộ topic `school/+/+/+/+`.
   * Tách các trường dữ liệu số (`count`, `t`, `rh`, `co2`, `p_w`, `e_wh`) và chuỗi (`alarm_state`, `light`, `fan`, `mode`) ghi vào InfluxDB.
   * Mở cổng giao tiếp **`POST http://localhost:1880/api/cmd`** cho phép gửi lệnh điều khiển JSON xuống từng phòng cụ thể.
3. **InfluxDB 2.7 (`docker-compose.yml`):** Lưu trữ dữ liệu chuỗi thời gian với thông số cấu hình sẵn:
   * **URL:** `http://localhost:8086` | **Org:** `HCMUT` | **Bucket:** `smart_school_data`
   * **Measurements:** `room_telemetry`, `room_event`, `room_status`.
4. **Grafana (`dashboard/grafana/`):**
   * Tự động nạp Datasource InfluxDB (Flux) và giao diện **Smart Virtual School - Digital Twin Dashboard** tại `http://localhost:3000` (tài khoản mặc định: `admin` / `admin`).
   * Hỗ trợ biến lọc động **`Room`** (`A1-201`, `A1-LAB`, `A1-CORRIDOR`) để chuyển đổi góc nhìn giữa các phòng theo thời gian thực.
5. **Python Room Simulator (`dashboard/simulator/simulator.py`):**
   * Đã được cấu hình tách riêng phòng `A1-201` (nhường cho mạch ESP32 Wokwi), chỉ giả lập đồng thời 2 biến thể phòng còn lại là **`A1-LAB`** (phòng máy tính sức chứa $30$, tải nền cao $\sim 85\text{--}115\text{ W}$) và **`A1-CORRIDOR`** (hành lang chỉ có đèn và cảm biến hiện diện).

---

## 6. Giao thức Giao tiếp (Firmware $\leftrightarrow$ Dashboard)

Hệ thống sử dụng chuẩn phân cấp topic MQTT: `school/{building}/{floor}/{room}/{channel}`

| Kênh (Topic) | Hướng truyền | QoS / Retain | Mô tả & Định dạng Payload JSON |
| :--- | :--- | :--- | :--- |
| `school/A1/2/A1-201/telemetry` | Node $\rightarrow$ Server | QoS 0 / False | Gửi định kỳ mỗi $5\text{ s}$:<br>`{"count":1,"presence":1,"t":26.5,"rh":60.0,"co2":450,"lux":210,"p_w":18.5,"e_wh":12.4,"light":"on","fan":"off","mode":"auto","alarm_state":"NORMAL","alarm_level":0,"node_type":"classroom"}` |
| `school/A1/2/A1-201/event` | Node $\rightarrow$ Server | QoS 1 / False | Gửi tức thời khi có sự kiện bất thường hoặc chuyển trạng thái FSM:<br>`{"room":"A1-201","type":"ALARM","detail":"TRIGGERED","val":3}` |
| `school/A1/2/A1-201/status` | Node $\rightarrow$ Server | QoS 1 / **True** | Trạng thái kết nối (sử dụng LWT): chuỗi `"online"` hoặc `"offline"` |
| `school/A1/2/A1-201/cmd` | Server $\rightarrow$ Node | QoS 0 / False | Lệnh điều khiển thiết bị và xác nhận báo động từ xa:<br>`{"light":"on","fan":"off","mode":"manual","ack_alarm":true}` |

---

## 7. Hướng dẫn Cài đặt & Chạy Demo Nghiệm thu

### Bước 1: Khởi chạy cụm Backend (Docker)
Mở terminal tại thư mục `Smart_Virtual_School/dashboard` và khởi chạy 4 container dịch vụ:
```bash
cd dashboard
docker compose up -d --build
```
*(Trên Windows có thể nhấp đúp trực tiếp file `dashboard/start.bat`).*  
Chờ khoảng $30\text{--}60\text{ s}$, kiểm tra bằng lệnh `docker ps` đảm bảo 4 container (`mosquitto`, `influxdb`, `nodered`, `grafana`) đều ở trạng thái `Up`.

### Bước 2: Chạy bộ giả lập các phòng ảo (`A1-LAB`, `A1-CORRIDOR`)
Mở một cửa sổ terminal mới để chạy song song script Python:
```bash
cd dashboard
pip install -r simulator/requirements.txt
python3 simulator/simulator.py
```
Terminal hiển thị `[MQTT] Ket noi thanh cong toi Mosquitto Broker!` và bắt đầu phát dữ liệu cho 2 phòng ảo mỗi $5\text{ s}$.

### Bước 3: Khởi chạy mạch ESP32 trên Wokwi (`A1-201`)
1. Mở thư mục gốc `Smart_Virtual_School` bằng **VS Code** (đã cài extension **PlatformIO IDE** và **Wokwi Simulator**).
2. Nhấn **Build** (`Ctrl + Alt + B`) để biên dịch firmware.
3. Mở file `diagram.json` và nhấn nút **Play (màu xanh lá)** để chạy mạch.
4. Quan sát Serial Monitor xác nhận dòng thông báo: `[MQTT] Ket noi thanh cong! Da gui status ONLINE.`

### Bước 4: Kịch bản Kiểm thử & Trình diễn trên Grafana
Truy cập **`http://localhost:3000`** (đăng nhập `admin` / `admin`), mở dashboard **Smart Virtual School - Digital Twin Dashboard**:

1. **Kiểm tra mô hình Digital Twin đa phòng:**
   * Lần lượt chọn biến **Room** ở góc trên bên trái qua `A1-201`, `A1-LAB`, `A1-CORRIDOR`. Cả 3 phòng đều hiển thị trạng thái **`ONLINE`** màu xanh lá với đặc tính phụ tải và cảm biến riêng biệt.
2. **Kiểm thử tự động hóa đếm người & bật đèn (`A1-201`):**
   * Trên mạch Wokwi, giảm thanh trượt cảm biến ánh sáng (`ldr`) về vùng tối ($< 300\text{ lux}$) và vặn nhẹ biến trở công suất (`pot_current`).
   * Nhấp vào cảm biến siêu âm trái (`us_a`) giảm khoảng cách xuống $< 100\text{ cm}$ $\rightarrow$ giảm siêu âm phải (`us_b`) xuống $< 100\text{ cm}$ $\rightarrow$ trả `us_a` về $200\text{ cm}$ $\rightarrow$ trả `us_b` về $200\text{ cm}$.
   * **Kết quả:** Trên Grafana, số người tăng lên `1`, trạng thái hiện diện đổi thành `Có người` và đèn (`light`) tự động chuyển sang `ON`.
3. **Kiểm thử báo cháy khẩn cấp & Điều khiển 2 chiều (`ALARM` & `ack_alarm`):**
   * Nhấp vào cảm biến **DHT22** trên Wokwi và kéo nhiệt độ lên **`> 50°C`** (ví dụ $55^\circ\text{C}$).
   * **Kết quả:** Còi trên Wokwi hú, đèn đỏ nháy $2\text{ Hz}$, quạt tắt (`OFF`), đèn bật sáng (`ON`); ô **FSM Cảnh báo** trên Grafana chuyển sang màu đỏ **`ALARM`**.
   * Kéo nhiệt độ DHT22 hạ xuống dưới $50^\circ\text{C}$ (về lại $26^\circ\text{C}$), sau đó gửi lệnh xác nhận tắt báo động từ xa qua Node-RED API:
     ```bash
     curl -X POST http://localhost:1880/api/cmd \
       -H "Content-Type: application/json" \
       -d '{"building":"A1","floor":"2","room":"A1-201","ack_alarm":true}'
     ```
   * **Kết quả:** Còi trên Wokwi tắt hẳn, ô **FSM Cảnh báo** trên Grafana trở về màu xanh **`NORMAL`** và bảng **Nhật ký Sự kiện** ghi nhận thêm dòng sự kiện `CLEAR` (`ALARM_ACK`).