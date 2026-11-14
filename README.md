# NguyenDucDuong_Lab3_De1
# LAB 3 – MẠCH GIÁM SÁT VÀ CẢNH BÁO NGƯỜI CHẠY

## 1. Thông tin sinh viên

- **Họ và tên:** Nguyễn Đức Dương
- **Mã sinh viên:** 231432201
- **Lớp:** N02
- **Đề tài:** Xây dựng hệ thống giám sát và cảnh báo người chạy sử dụng ESP32, BMP180 và FreeRTOS.

## 2. Giới thiệu đề tài

Đề tài xây dựng hệ thống giám sát các thông số môi trường sử dụng ESP32 và cảm biến BMP180. Hệ thống có khả năng đo nhiệt độ, áp suất khí quyển và tính toán độ cao tương đối so với mực nước biển.

FreeRTOS được sử dụng để chia chương trình thành các Task độc lập, đảm bảo việc đọc cảm biến, truyền dữ liệu MQTT và xử lý cảnh báo có thể hoạt động đồng thời.

Dữ liệu được gửi lên giao diện Web theo thời gian thực. Người dùng có thể theo dõi thông số và điều khiển LED, còi từ xa thông qua MQTT.

## 3. Công nghệ sử dụng

- ESP32 DevKit.
- Cảm biến áp suất BMP180.
- Hệ điều hành thời gian thực FreeRTOS.
- Giao thức truyền thông MQTT.
- MQTT Broker: broker.hivemq.com.
- Giao diện giám sát Web.
- Nền tảng mô phỏng Wokwi.
- Ngôn ngữ lập trình C/C++ (Arduino).

## 4. Sơ đồ kết nối phần cứng

| Thiết bị | Chân thiết bị | Chân ESP32 |
|---|---|---|
| BMP180 | VCC | 3V3 |
| BMP180 | GND | GND |
| BMP180 | SCL | GPIO22 |
| BMP180 | SDA | GPIO21 |
| Buzzer | + | GPIO25 |
| Buzzer | - | GND |
| LED đỏ | Anode (+) qua điện trở 220 Ω | GPIO26 |
| LED đỏ | Cathode (-) | GND |

## 5. Kiến trúc hệ thống

Hệ thống gồm 3 Task chính:

- **TaskRead:** Đọc dữ liệu từ cảm biến BMP180 mỗi 1 giây và đưa dữ liệu vào Queue.
- **TaskTransmit:** Kết nối Wi-Fi, MQTT và gửi dữ liệu lên Broker.
- **TaskAlert:** Phân tích dữ liệu, xử lý cảnh báo và điều khiển LED, Buzzer.

Hai Queue được sử dụng:
- `dataQueue`: Truyền dữ liệu cảm biến từ TaskRead đến TaskTransmit.
- `alertQueue`: Truyền dữ liệu nhiệt độ và độ cao từ TaskRead đến TaskAlert.

## 6. Chức năng hệ thống

- Đo nhiệt độ, áp suất và độ cao.
- Gửi dữ liệu lên MQTT Broker.
- Hiển thị thông số theo thời gian thực trên Web.
- Cảnh báo khi nhiệt độ vượt ngưỡng.
- Phát hiện độ cao thay đổi đột ngột.
- Điều khiển LED và Buzzer từ xa.
- Hỗ trợ hai chế độ AUTO và MANUAL.

### Ngưỡng cảnh báo

| Điều kiện | Ngưỡng | Mã cảnh báo |
|---|---|---|
| Nhiệt độ cao | > 38°C | TEMP_HIGH |
| Nhiệt độ thấp | < 5°C | TEMP_LOW |
| Độ cao thay đổi đột ngột | Chênh lệch > 3m | ALT_JUMP |

## 7. Giao tiếp MQTT

MQTT Broker sử dụng: `broker.hivemq.com`

Cổng kết nối: `1883`

Các Topic sử dụng:

| Topic | Chức năng |
|---|---|
| `runner_demo_01/data` | ESP32 gửi dữ liệu lên Web |
| `runner_demo_01/cmd/mode` | Điều khiển chế độ AUTO/MANUAL |
| `runner_demo_01/cmd/led` | Điều khiển LED |
| `runner_demo_01/cmd/buzzer` | Điều khiển Buzzer |

## 8. Mô phỏng trên Wokwi

**Link dự án Wokwi:**

https://wokwi.com/projects/480231836838865921

## 9. Kết quả thực hiện

Hệ thống được kiểm thử với các trường hợp:

1. Đọc và gửi dữ liệu cảm biến lên MQTT.
2. Kiểm thử cảnh báo nhiệt độ cao.
3. Kiểm thử cảnh báo nhiệt độ thấp.
4. Kiểm thử độ cao thay đổi đột ngột.
5. Kiểm thử chế độ điều khiển MANUAL.

## 10. Hình ảnh minh họa

Các hình ảnh mô phỏng và kết quả thực hiện được lưu trong thư mục `images` và báo cáo.

## 11. Kết luận

Hệ thống đã ứng dụng ESP32 kết hợp FreeRTOS để thực hiện giám sát và cảnh báo thông số môi trường. Việc sử dụng MQTT giúp truyền dữ liệu và điều khiển thiết bị từ xa. Qua bài thực hành, sinh viên hiểu rõ hơn về lập trình đa nhiệm, giao tiếp giữa các Task bằng Queue và ứng dụng IoT trong thực tế.
