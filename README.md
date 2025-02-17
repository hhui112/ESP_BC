# Smart Pillow Boart Program for Esp32

智能枕头板端程序使用ESP32作为CPU，目前实现蓝牙配网、wifi断连、气囊自适应稳压、加热恒温调节、按键识别长短按功能




## How to use this boart program

运行环境

操作系统： Linux

平台： vscode

idf版本：4.3

如无安装Espressif IDF插件，请根据Espressif IDF插件的安装流程提前安装好 python3.8 idf4.3，可参考[window下Espressif IDF插件的安装流程](https://blog.csdn.net/weixin_41594119/article/details/114230462)

如已安装Espressif IDF插件，请直接打开该工程 可通过最下方的图形界面进行构建、烧录、监控、选择串口号、配置菜单等操作


## this project folder contents

整个工程的结构如下所示

```
├── CMakeLists.txt
├── components                  添加自定义组件
│   ├── led                     灯
│   ├── use_adc                 adc
│   ├── use_ble_server          蓝牙
│   ├── use_key                 按键
│   ├── use_pwm                 pwm
│   ├── use_wifi                WiFi
│   └── pillow_control          与气囊相关功能
├── main
│   ├── CMakeLists.txt
│   ├── component.mk          
│   └── smart_pillow_main.c      主函数
├── Makefile                  
├── sdkconfig.defaults          用户自定义修改配置
├── sdkconfig                   编译出的整体配置
└── README.md                  
```


