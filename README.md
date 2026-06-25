# ESP_BC — 盛和养老智能床垫 ESP32 板端固件

ESP32 主控固件，用于盛和养老智能床垫 BC 模块：连接 SU2 睡眠传感器、MFP 床控主控盒，经 WiFi / 蓝牙与涂鸦 IoT（阿里云版）云端通信，实现实时数据上报、睡眠报告、OTA 升级、床控与打鼾干预等功能。

**当前版本：** `BC_ESP_2026_2_3_4`（V2.3.4）  
**芯片：** ESP32  
**工程名：** `bochang-program`

详细变更说明见 [`固件/V2.3.4/BC_ESP_2026_2_3_4更新说明.md`](固件/V2.3.4/BC_ESP_2026_2_3_4更新说明.md)。

---

## 主要功能

| 模块 | 说明 |
|------|------|
| **WiFi** | STA 模式，支持断线重连；获 IP 后 SNTP 对时，再建立 MQTT |
| **蓝牙 BLE** | 配网、数据透传、床控指令下发 |
| **涂鸦 MQTT** | `mqtts://iot.smartbed.ink:1883`，动态 HMAC 签名，keepalive 300s |
| **传感器 SU2** | UART1 与 SU2 模块通信，解析 5s / 60s / 睡眠报告 / 呼吸暂停等 protobuf 数据 |
| **MFP 床控** | UART2 与主控盒通信，支持 MQTT / BLE 床控、打鼾干预、闹钟联动 |
| **OTA** | HTTPS 双分区升级，MQTT 下行 `/ota/device/upgrade`，上线 inform 版本 |
| **数据上报** | 5s（type:1）、60s（type:2）、睡眠报告、打鼾参数等 JSON 经 MQTT / BLE 上报 |

---

## 开发环境

| 项目 | 要求 |
|------|------|
| 芯片 | ESP32 |
| 框架 | [ESP-IDF](https://docs.espressif.com/projects/esp-idf/)（建议 4.3 及以上，与现有 `sdkconfig` 匹配） |
| 工具 | VS Code + Espressif IDF 插件，或 IDF 命令行 |
| 操作系统 | Windows / Linux / macOS |

### 构建与烧录

```bash
idf.py set-target esp32
idf.py build
idf.py -p COMx flash monitor
```

也可在 VS Code 底部 IDF 面板中选择 **Build / Flash / Monitor**。

### 菜单配置

```bash
idf.py menuconfig
```

分区表使用根目录 [`my_partitions.csv`](my_partitions.csv)（含 `ota_0` / `ota_1` 双应用分区、`storage` 8M FAT 等）。

---

## 工程目录结构

```
ESP_BC/
├── main/
│   └── main.c                      # 入口：NVS、BLE、WiFi、MFP、app_control
├── components/
│   ├── common/                     # 全局配置、设备信息、NVS
│   ├── app_control/                # 业务主控：SU2 解析、上报、床控、打鼾干预
│   ├── use_wifi/                   # WiFi、SNTP、涂鸦 MQTT、OTA
│   ├── use_ble_server/             # BLE GATT 服务
│   ├── use_uart/                   # UART 驱动封装
│   ├── use_mfp/                    # MFP 床控协议
│   ├── use_adc/                    # ADC / CH423（按需）
│   ├── use_pwm/                    # PWM（按需）
│   └── protobuf/                   # SU2 protobuf 编解码
├── my_partitions.csv               # 自定义分区表
├── sdkconfig / sdkconfig.defaults
├── 固件/                           # 各版本发布 bin 与更新说明
│   └── V2.3.4/                     # 当前版本烧录包
├── flash_download_tool/            # Windows 烧录工具配置
├── id/                             # 设备 ID / 三元组生成工具
└── docs/                           # 架构与设计文档
```

---

## 硬件接口（默认引脚）

| 接口 | 引脚 | 用途 |
|------|------|------|
| UART1 TX / RX | GPIO 22 / 26 | SU2 睡眠传感器 |
| UART2 TX / RX | GPIO 19 / 25 | MFP 主控盒 |
| LED | GPIO 19 / 18 / 5 | 蓝 / 绿 / 红 |

具体以 [`components/common/common.h`](components/common/common.h) 中宏定义为准。

---

## 配置说明

关键编译宏位于 [`components/common/common.h`](components/common/common.h)：

| 宏 | 说明 |
|----|------|
| `INIT_VERSION` | 固件版本字符串，OTA inform 与 NVS 使用 |
| `ALIYUN_BURN` | `1`：三元组从 flash `storage` 分区读取（量产）；`0`：使用下方 `PRODUCT_KEY` / `DEVEICE_ID` / `DEVEICE_SECRET` 宏（开发调试） |
| `PRODUCT_KEY` | 涂鸦 ProductKey，当前为 `ixvaCaIfGla` |
| `OTA_ON_OFF` | OTA 功能开关 |
| `DATA_UP` | 数据上报总开关 |

> 量产设备三元组通过 `storage` 分区烧录，与 `common.h` 中宏无关；开发阶段若 MQTT 报 `bad username or password`，可临时将 `ALIYUN_BURN` 设为 `0` 并使用正确三元组宏。

默认 WiFi SSID / 密码也在 `common.h` 中，量产前请改为现场网络或通过 BLE 配网写入 NVS。

---

## 云端 MQTT Topic

连接成功后设备会订阅 / 发布以下 Topic（`{productKey}`、`{deviceId}` 来自三元组）：

| 方向 | Topic | 说明 |
|------|-------|------|
| 发布 | `/{productKey}/{deviceId}/user/5s/put` | 5s 实时数据 |
| 发布 | `/{productKey}/{deviceId}/user/60s/put` | 60s 统计数据 |
| 发布 | `/{productKey}/{deviceId}/user/sleep/put` | 睡眠报告 |
| 发布 | `/{productKey}/{deviceId}/user/sa/put` | 呼吸暂停数据 |
| 订阅 | `/{productKey}/{deviceId}/user/cli/get` | CLI 指令（sensorCli / deviceCli / mcCli） |
| 发布 | `/{productKey}/{deviceId}/user/cli/put` | CLI 应答 |
| 订阅 | `/ota/device/upgrade/{productKey}/{deviceId}` | OTA 升级下行 |
| 发布 | `/ota/device/inform/{productKey}/{deviceId}` | OTA 版本 inform（每次连上 MQTT 上报） |

### 5s / 60s 数据格式（V2.3.4+）

`ts` 为 ESP 上报时刻，`sensor_ts` 为传感器侧采样时间戳：

```json
{
  "id": "KSPSBED00001057",
  "ts": 1710000000,
  "type": 1,
  "data": {
    "sensor_ts": 1709999995,
    "heart": 72,
    "breath": 16,
    "status": [0, 0, 0, 0, 0]
  }
}
```

### OTA 升级下行示例

```json
{
  "params": {
    "version": "BC_ESP_2026_2_3_4",
    "url": "https://example.com/firmware.bin"
  }
}
```

`data` 与 `params` 字段均可，需包含 `version`、`url`。

---

## 烧录发布包

当前版本预编译固件位于 [`固件/V2.3.4/`](固件/V2.3.4/)：

| 文件 | 说明 |
|------|------|
| `bochang-program.bin` | 应用程序 |
| `bootloader.bin` | Bootloader |
| `partition-table.bin` | 分区表 |
| `ota_data_initial.bin` | OTA 数据初始区 |

可使用 [`flash_download_tool/`](flash_download_tool/) 或 `idf.py flash` 烧录。历史版本见 [`固件/`](固件/) 下各版本目录。

设备 ID / 阿里云三元组可使用 [`id/ID_Generator_Tool/`](id/ID_Generator_Tool/) 生成并写入 `storage` 分区。

---

## 相关文档

| 文档 | 说明 |
|------|------|
| [`固件/V2.3.4/BC_ESP_2026_2_3_4更新说明.md`](固件/V2.3.4/BC_ESP_2026_2_3_4更新说明.md) | V2.3.4 完整更新说明 |
| [`固件/盛和养老固件更新说明.md`](固件/盛和养老固件更新说明.md) | 历史版本变更记录 |
| [`盛和养老系统下发协议V2.5.docx`](盛和养老系统下发协议V2.5.docx) | 云端 / APP 下发协议 |
| [`docs/SU2_通信架构.md`](docs/SU2_通信架构.md) | SU2 通信架构设计（新工程参考） |

---

## Git 分支

| 分支 | 说明 |
|------|------|
| `project` | 当前主开发 / 发布分支（含 V2.3.4） |
| `main` | 合并主线 |

---

## 许可证

内部项目，版权归 KEESON / 盛和相关团队所有。未经授权请勿对外分发固件与三元组。
