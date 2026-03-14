# Smart Irrigation Gateway

ESP32-P4 智能灌溉网关中控屏。

## 硬件平台

- 主控：ESP32-P4（SDIO 连接 ESP32-C6 实现 WiFi6）
- 屏幕：Waveshare 10.1" IPS 触摸屏（1280×800，MIPI DSI）
- 触摸：GT911（I2C）

## 软件框架

- ESP-IDF 5.5.2
- LVGL 9.5
- FreeRTOS

## 构建

```bash
idf.py build
idf.py -p COMx flash monitor
```

## 恢复依赖

```bash
idf.py reconfigure
```
