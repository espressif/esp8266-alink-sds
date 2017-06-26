ALINK是由阿里推出的智能硬件模组、阿里智能云、阿里智能APP，阿里智能生活事业部为厂商提供一站式设备智能化解决方案。本工程对接ALINK EMBAD版本，并对ALINK底层交互流程进行封装。

## 基本功能:
1. 配网(一键配网、热点配网)
2. 绑定、解绑
3. OTA升级
4. 数据传输非透传

## 文件结构
    esp8266-alink-embed
    ├── adaptation                      // ALINK的底层适配
    ├── application                     // ALINK应用层API的封装
    │   ├── esp_alink_data_transport.c  // alink数据传传输的封装
    │   ├── esp_alink_main.c            // 连接ap、恢复出厂设置、事件回调函数等封装
    │   ├── esp_info_store.c            // 将直接对FLASH的操作，封装为KEY/VALUE的格式
    │   ├── esp_json_parser.c           // 将JSON的操作封装支持泛型参数的两个API
    │   └── Makefile
    ├── include
    │   ├── alink_export.h              // ALINK-SDK提供的原生应用层 API
    │   ├── alink_json_parser.h         // ALINK-SDK提供的原生JSON API
    │   ├── esp_alink.h                 // 封装的应用层API使用说明及配置
    │   ├── esp_alink_log.h             // 定义了打印等级
    │   ├── esp_info_store.h            // 信息存储API详解及EXAMPLE
    │   └── esp_json_parser.h           // JSON API详解及EXAMPLE
    ├── lib                             // ALINK-SDK相关的库文件
    ├── Makefile
    └── README.md
>注：
>   1. esp_info_store仅支持4kB以内的数据存储
>   2. esp_info_parser暂不支持数组类型的解析

## 开发流程
1. 熟悉ESP866:[ESP8266入门指南](http://espressif.com/zh-hans/support/explore/get-started/esp8266/getting-started-guide)
2. 了解阿里智能相关知识：[阿里智能开放平台概述](https://open.aliplus.com/docs/open)
3. [签约入驻](https://open.aliplus.com/docs/open/open/enter/index.html)
4. [产品注册](https://open.aliplus.com/docs/open/open/register/index.html)
5. 产品开发
    - 你可以使用我们的提供封装后的API，也可以基于ALINK提供的原生应用层API进行开发，
    API的使用可参见[alink light demo](https://zzchen@gitlab.espressif.cn:6688/customer/esp8266-ecovacs-alink-embed.git), 具体每一个API的详解参见相关头文件。
6. [发布上架](https://open.aliplus.com/docs/open/open/publish/index.html)

## 注意事项
1. alink使用的esp8266_sdk为定制的, 优化了内存空间，修改了sniffer_buf的大小, ssh://git@gitlab.espressif.cn:27227/customer/esp8266-rtos-sdk-alink.git
2. esp8266内存十分有限，请尽可能减小应用层对内存使用，如有必要可以修改相关配置减少alink_sdk的内存使用，例如在Makefile中添加如下配置：
```
    CCFLAGS += -D CONFIG_DOWN_CMD_QUEUE_NUM=1
    CCFLAGS += -D CONFIG_UP_CMD_QUEUE_NUM=1
    CCFLAGS += -D CONFIG_EVENT_HANDLER_CB_STACK=256
    CCFLAGS += -D CONFIG_ALINK_POST_DATA_STACK=384
    CCFLAGS += -D CONFIG_ALINK_DATA_LEN=256
```
3. 暂不支持零配网的功能，由于ESP266内存的限制，未将可选配网——零配网加入ESP8266_ALNIK中。
4. 暂不支持数据透传的模式，由于alink_embed官方暂不支持，这种数据传输格式

## Related links
- ESP8266入门指南 : http://espressif.com/zh-hans/support/explore/get-started/esp8266/getting-started-guide
- 阿里智能开放平台：https://open.aliplus.com/docs/open/

