# Smart Irrigation — 毕业设计项目提示词

## 项目概述

智能灌溉网关中控屏，大学毕业设计项目。

- **芯片**: ESP32-P4（无原生 WiFi，通过 SDIO 连接 ESP32-C6 实现 WiFi6）
- **框架**: ESP-IDF 5.5.2
- **屏幕**: Waveshare 10.1 寸 IPS 触摸屏（800×1280 原生，旋转后 1280×800 横屏）
- **LCD 驱动 IC**: JD9365（MIPI DSI，2 数据通道，1500Mbps）
- **触摸 IC**: GT911（I2C，需 xy-swap + mirror-x）
- **UI 库**: LVGL 9.4+（RGB565，三缓冲，15ms 刷新）
- **语言**: C（嵌入式 C99）
- **UI 语言**: 中文

## 项目结构

```
smart_irrigation/
├── main/main.c                          # 入口，回调注册，桥接 UI ↔ 硬件
├── components/
│   ├── smart_irrigation_ui/             # LVGL UI 层（16 个源文件）
│   │   ├── ui_main.c                    # 主框架：侧边栏导航、状态栏、页面切换
│   │   ├── ui_home.c                    # 首页：模式弧形控件、设备状态、灌溉计划表
│   │   ├── ui_program.c                 # 程序管理：灌溉程序/配方 CRUD，NVS 持久化
│   │   ├── ui_device.c                  # 设备控制：主机/阀门/分区/传感器 4 标签页
│   │   ├── ui_log.c                     # 日志查看：5 类日志 + 日期范围过滤
│   │   ├── ui_settings.c               # 设置：6 标签页（分区/设备/阀门/传感器/系统/主机）
│   │   ├── ui_maintenance.c             # 维护：通信/标定/算法参数
│   │   ├── ui_alarm.c                   # 报警管理弹窗
│   │   ├── ui_alarm_records.c           # 报警记录查询桥接
│   │   ├── ui_wifi.c                    # WiFi 扫描结果渲染、连接对话框
│   │   ├── ui_keyboard.c               # 26 键软键盘组件
│   │   ├── ui_numpad.c                 # 数字键盘组件
│   │   ├── ui_display.c                # 显示设置桥接
│   │   ├── ui_network.c                # 网络设置桥接
│   │   ├── my_font_cn_16.c             # 中文字体（16px，~3000 字）
│   │   └── my_fontbd_16.c              # 粗体中文字体子集
│   ├── wifi_manager/                    # WiFi 管理（扫描/连接/重连/静态 IP）
│   ├── display_manager/                 # 亮度/自动熄屏/BOOT 按键唤醒
│   ├── event_recorder/                  # NVS 事件记录（环形缓冲区，最多 20 条/类型）
│   └── esp32_p4_wifi6_touch_lcd_x/      # Waveshare BSP（勿随意修改）
├── managed_components/                   # 自动管理的外部依赖（勿手动修改）
├── sdkconfig.defaults                    # 项目配置覆盖项
├── partitions.csv                        # 分区表：nvs(24KB) + phy(4KB) + factory(15MB)
└── build.bat                             # Windows 构建脚本
```

## 架构原则

### 回调解耦模式（最重要的架构规则）

UI 层和硬件层通过 `main.c` 中注册的函数指针回调桥接，**严禁** UI 文件直接 `#include` 硬件头文件：

```c
// ✅ 正确：UI 通过回调通知硬件层
static wifi_scan_cb_t g_wifi_scan_cb = NULL;
void ui_wifi_set_scan_callback(wifi_scan_cb_t cb) { g_wifi_scan_cb = cb; }
// 在 main.c 中注册：ui_wifi_set_scan_callback(wifi_manager_start_scan);

// ❌ 错误：UI 直接调用硬件 API
#include "wifi_manager.h"  // 不要在 UI 文件中这样做
wifi_manager_start_scan();
```

### 跨线程 UI 更新

WiFi/硬件回调在非 LVGL 线程执行，更新 UI 必须使用 `bsp_display_lock()`/`bsp_display_unlock()` 或 `lv_async_call()`：

```c
// ✅ 从非 LVGL 线程安全更新 UI
static void update_ui_async(void *param) {
    lv_label_set_text(label, (const char *)param);
}
lv_async_call(update_ui_async, "已连接");

// 或者使用 BSP 锁
bsp_display_lock(0);
lv_label_set_text(label, "已连接");
bsp_display_unlock();
```

## 编码规范

### 通用规则

- 所有函数使用 `esp_err_t` 返回值（除 void 回调和 UI 事件处理器外）
- 使用 `ESP_RETURN_ON_ERROR` / `ESP_GOTO_ON_ERROR` 宏简化错误检查
- 日志使用 `ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE`，每个文件定义 `static const char *TAG = "模块名";`
- 头文件使用 `#pragma once` 或 include guard
- 函数命名：`模块名_动作_对象`，如 `wifi_manager_start_scan()`、`ui_home_update_status()`

### 内存管理

- **PSRAM**（外部，大容量）: UI 缓冲区、大数组、字体数据 → 用 `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)`
- **SRAM**（内部，快速）: DMA 缓冲区、中断相关数据 → 用 `heap_caps_malloc(size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA)`
- 分配后必须检查 NULL，释放后置 NULL
- 在注释中标注内存类型：`/* PSRAM: 存放扫描结果列表 */`

### FreeRTOS 任务规范

```c
// 任务创建模板
xTaskCreate(
    task_function,       // 任务函数
    "task_name",         // 名称（最多 16 字符）
    4096,                // 栈大小（字节）——根据实际需要调整
    NULL,                // 参数
    5,                   // 优先级（0-24，LVGL 任务默认为 2）
    &task_handle         // 句柄（用于后续删除/通知）
);
```

任务优先级参考：

- LVGL 刷新任务: 2（由 BSP 管理）
- WiFi 后台任务: 5
- NTP 同步任务: 3
- 显示监控任务: 3

### NVS 存储规范

- 命名空间不超过 15 字符，键名不超过 15 字符
- 写入前先读取，避免不必要的 flash 磨损
- 使用 `nvs_commit()` 确保数据落盘
- 项目已用命名空间：`wifi_mgr`、`disp_mgr`、`evt_rec`、`irrigation`

### LVGL UI 编码规范

- 屏幕布局：左侧边栏 87px + 主内容区 1178×735px + 底部状态栏
- 配色方案：主蓝色 `#3498db`、深色背景 `#2c3e50`、浅色背景 `#ecf5f8`
- 控件创建后立即设置样式，不要分散在多处
- 复杂页面使用 `lv_obj_clean()` 清除子对象再重建，避免内存泄漏
- 表格/列表数据更新时，先清空再填充，不要逐条追加
- 弹窗/对话框使用模态遮罩（半透明黑色背景 `lv_color_black(), LV_OPA_50`）
- 字体：正文使用 `my_font_cn_16`，标题使用 `my_fontbd_16`，英文/数字可用 LVGL 内置 Montserrat

## 新增组件模板

添加新组件时遵循此结构：

```
components/新模块名/
├── CMakeLists.txt
├── include/新模块名.h
└── 新模块名.c
```

CMakeLists.txt 模板：

```cmake
idf_component_register(
    SRCS "新模块名.c"
    INCLUDE_DIRS "include"
    REQUIRES nvs_flash esp_event       # 按需添加依赖
)
```

头文件模板：

```c
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化模块
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t 模块名_init(void);

#ifdef __cplusplus
}
#endif
```

## 硬件引脚速查

| 功能                  | GPIO  |
| --------------------- | ----- |
| I2C SCL               | 8     |
| I2C SDA               | 7     |
| LCD 背光 (PWM)        | 26    |
| LCD 复位              | 27    |
| BOOT 按键（唤醒屏幕） | 35    |
| WiFi SDIO CMD         | 19    |
| WiFi SDIO CLK         | 18    |
| WiFi SDIO D0-D3       | 14-17 |
| WiFi C6 复位          | 54    |
| SD 卡 D0-D3           | 39-42 |
| SD 卡 CMD             | 44    |
| SD 卡 CLK             | 43    |

## 构建与烧录

```bash
# 构建
idf.py build

# 烧录（COM10，UART）
idf.py -p COM10 flash monitor

# 仅监控串口
idf.py -p COM10 monitor

# 清理构建
idf.py fullclean
```

## 待开发功能（毕设剩余工作）

以下功能 UI 已就绪但后端逻辑未实现，开发时注意保持与现有架构一致：

- MQTT 通信（连接云平台，上报传感器数据，接收控制指令）
- Modbus RTU 通信（RS485 连接 EC/pH/压力/流量传感器和电磁阀）
- 灌溉调度引擎（根据程序配置自动执行灌溉任务）
- OTA 升级（固件在线更新）
- 4G 模块支持（备用网络通道）

## 毕设论文辅助

每次给出代码实现后，请附加【毕设论文要点】板块：

- **技术实现亮点**: 用学术化语言描述模块优点（如中断驱动、任务同步机制、事件驱动架构等）
- **稳定性保障**: 描述异常处理和性能优化策略
- **系统设计意义**: 该模块在整体系统架构中的作用和价值
