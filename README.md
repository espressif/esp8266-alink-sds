本工程为阿里云 ESP8266 ALINK SDS 接口使用示例，你可以通过本示例了解 ALINK 配网、升级及数据传输等

## 1 ALINK 概述
### 1.1 是什么 ALINK
智能硬件模组、阿里智能云、阿里智能 APP，阿里智能生活事业部为厂商提供一站式设备智能化解决方案。alink 到目前已经过了多次的版本迭代，各版本的区别如下：

1. alink v1.0：为阿里提供最基础的版本，支持一键配网，设备热点配网技术，ota 升级等功能
2. alink v1.1：增强设备身份安全
3. alink embed：增加支持零配发现模式，默认配网激活模式，手机热点配网模式，但需要占更多的内存空间
4. alink sds：APP和服务端允许客户定制化，但是需要收取服务收费

<img src="docs/readme_image/alink_description.png" width="700" />

### 1.2 为什么要使用 ALINK
阿里智能开发平台的提供强大的智能平台，极大的简化的智能家居开发的高难度和复杂性，主要有以下六个特点：低成本、短时间、大安全、大数据、标准化和定制化等。
<img src="docs/readme_image/alink_advantage.png" width="700" />

### 1.3 怎么使用 ALINK
我们已经完成 ALINK EMBED SDK 在模组上的适配工作，您在 产品开发时，只需基于模组基础上进行二次开发。
<img src="docs/readme_image/alink_development.png" width="700" />

## 2 前置条件
### 2.1 知识储备
- 了解 ALINK
请详细阅读阿里提供的官方[技术文档](https://open.aliplus.com/docs/open/)，了解 ALINK 相关的概念
- 熟悉 ESP32 开发
请详细阅读 [ESP8266入门指南](http://espressif.com/zh-hans/support/explore/get-started/esp8266/getting-started-guide)

### 2.2 硬件准备
- 开发板：一块以上 ESP_WROOM_02 开发板
- 路由器：关闭 5G 网络，且可以连接外网（所有的数据交互必需通过阿里云服务器）
- 手机：内置有 SIM 卡，可连接 4G 移动网络（测试热点配网功能时，设备需要通过手机热点连接阿里云服务器完成的注册、绑定等过程）

## 3 开发板介绍
<img src="docs/readme_image/ESP_WROOM_02.png" width="700" />

    标号 1 开关拨下（拨下为断电状态，拨上为上电状态）；
    标号 2 开关拨下（拨下为下载模式，拨上为运行模式）；
    标号 3 开关拨上（CHIP_EN PIN，默认拨上即可）；
    标号 4 跳线帽插入上方的两个针脚；
    标号 5 插入跳线帽；
    标号 6 配网开关，短按（<3s）激活设备，长按（>=3s）设备进入配网模式；
    标号 9 设置开关，短按（<3s）高频压测模式，长按（>=3s）设备端会解绑并恢复出厂设置；
    标号 10 焊接 GPIO4 与 SW2 旁边的小圆孔，使能 SW2 按键，即出厂设置开关。

## 4 文件结构
    esp8266-alink-embed
    ├── bin                                     // 存放生成的 bin 文件
    ├── docs
    ├── driver
    ├── esp8266-rtos-sdk-alink                  // ESP8266 SDK
    ├── gen_misc.sh                             // 编译脚本
    ├── include
    ├── Makefile                                // 通过 Makefile 来配置 alink 选项
    ├── platforms
    │   ├── alink-embed
    │   │   ├── adaptation                      // ALINK 的底层适配
    │   │   ├── application                     // ALINK 应用层API的封装
    │   │   │   ├── esp_alink_data_transport.c  // ALINK 数据传传输
    │   │   │   ├── esp_alink_main.c            // 连接ap、恢复出厂设置、事件回调函数
    │   │   │   ├── esp_info_store.c            // FLASH 的读写操作
    │   │   │   ├── esp_json_parser.c           // JSON 字符串的生成与解析
    │   │   │   └── Makefile
    │   │   ├── include
    │   │   │   ├── alink_export.h              // ALINK 官方提供的原生应用层 API
    │   │   │   ├── alink_json_parser.h         // ALINK 官方提供的原生JSON API
    │   │   │   ├── esp_alink.h                 // 封装的应用层API使用说明及配置
    │   │   │   ├── esp_alink_log.h             // 定义了打印等级
    │   │   │   ├── esp_info_store.h            // 信息存储API详解及EXAMPLE
    │   │   │   └── esp_json_parser.h           // JSON API详解及EXAMPLE
    │   │   ├── lib                             // 库文件
    │   │   ├── Makefile
    │   │   └── README.md
    │   └── Makefile
    ├── README.md
    └── user
        ├── alink_key_trigger.c                 // 按键触发函数
        ├── Makefile
        └── sample_json.c                       // ALINK 非透传示例

## 5 编译环境的搭建
您可以使用 xcc 和 gcc 来编译项目，建议使用 gcc。对于 gcc，请参考[esp-open-sdk](https://github.com/pfalcon/esp-open-sdk)

## 6 配置
您可以通过修改本工程下的 Makefile 来配置。
```bash
/*!< 打开 json 调试，您可以查看任务的栈空间使用情况判断是事出现栈溢出 */
CCFLAGS += -D SAMPLE_JSON_DEBUG

/*!< 修改模组名称，在修改模组名称前，请确认设备已经解绑 */
CCFLAGS += -D CONFIG_ALINK_MODULE_NAME=\"ESP-WROOM-02\"

/*!< 如果开发时内存空间不足可以参考如下配置，内存将增大4k左右，但此配置不支持超长数据包测试 */
CCFLAGS += -D ALINK_WRITE_NOT_BUFFER
CCFLAGS += -D CONFIG_DOWN_CMD_QUEUE_NUM=1
CCFLAGS += -D CONFIG_EVENT_HANDLER_CB_STACK=384
CCFLAGS += -D CONFIG_READ_TASK_STACK=768
CCFLAGS += -D CONFIG_ALINK_DATA_LEN=512
CCFLAGS += -D CONFIG_SSL_READ_BUFFER_LEN=2048
```

## 7 编译
如果您是在 ubuntu 的平台下开发只需运行脚本 `gen_misc.sh`，其他平台的参见：https://github.com/pfalcon/esp-open-sdk

## 8 固件烧录（windows）
1. 安装[串口驱动](http://www.usb-drivers.org/ft232r-usb-uart-driver.html)
2. 安装[烧写工具](http://espressif.com/en/support/download/other-tools)
3. 烧录相关 bin 文件
    将 GPIO0 开关（GPIO0 Control）拨到内侧开发板置为下载模式，按照如下所示，配置串口号、串口波特率等，按 `START` 即可开始下载程序

    <img src="docs/readme_image/download.png" width="400" />

        boot.bin------------------->0x000000    // 启动程序
        user1.2048.new.5.bin------->0x01000     // 主程序
        blank_12k.bin-------------->0x1F8000    // 初始化用户参数区
        blank.bin------------------>0x1FB000    // 初始化 RF_CAL 参数区。
        esp_init_data_default.bin-->0x1FC000    // 初始化其他射频参数区
        blalk.bin------------------>0x1FE000    // 初始化系统参数区

    > 注：ESP-LAUNCHER 上的 J82 跳针需要用跳线帽短接，否则无法下载
4. 启动设备
    下载完毕后将 GPIO0 开关 (GPIO0 Control) 拨到外侧将 ESP-LAUNCHER 开发板置为工作模式，出现“ENTER SAMARTCONFIG MODE”信息，即表示 ALINK 程序正常运行，进入配网模式

    <img src="docs/readme_image/running.png" width="400" />

## 9 运行与调试
1. 下载阿里[智能厂测包](https://open.aliplus.com/download)
2. 登陆淘宝账号
3. 开启配网模组测试列表：
    - 安卓：点击“环境切换”，勾选“开启配网模组测试列表”
    - 苹果：点击“AKDebug”->测试选项，勾选“仅显示模组认证入口”
4. 添加设备：添加设备->“分类查找”中查找对应的类别->模组认证->V3 配网_热点配网
5. 按键说明：
    - 激活设备：短按（<3s）配网开关
    - 重新配网：长按（>=3s）配网开关
    - 高频压测：短按（<3s）设置开关
    - 出厂设置：长按（>=3s）设置开关

## 10 开发流程
### 10.1 [签约入驻](https://open.aliplus.com/docs/open/open/enter/index.html)
使用淘宝账号签约入驻阿里平台，并完成账号授权
### 10.2 [产品注册](https://open.aliplus.com/docs/open/open/register/index.html)
产品注册是设备上云的必要过程，任何一款产品在上云之前必须完成在平台的注册
### 10.3 [产品开发](https://open.aliplus.com/docs/open/open/develop/index.html)
> 注：1. 除非您有特殊需求，否则您在开发时只需修改 user 下的代码，无需关心其内部实现细节
> 　　2. 您需要先开发手机APP，alink embed 版本需要单独的开发的APP，除demo外使用阿里小智将无法进行设备的配网，控制等操作

1. **初始化**

    阿里服务器后台导出设备 TRD 表格,调用`alink_init()`传入产品注册的信息，注册事件回调函数
    ```c
    /*!< 每一个具体设备都是不一样的，保证设备的秘密性 */
    #define device_key    "xrSJSzVDKPk4UB7BGCIf"
    #define device_secret "cRB3lwgd7zwFg02DK69xxl2lgefDdtYZ"
    const alink_product_t product_info = {
        .name           = "alink_product",
        .version        = "1.0.0",
        .model          = "ALINKTEST_LIVING_LIGHT_ALINK_TEST",
        /*!< 每一种产品键值对都是相同的 */
        .key            = "5gPFl8G4GyFZ1fPWk20m",
        .secret         = "ngthgTlZ65bX5LpViKIWNsDPhOf2As9ChnoL9gQb",
        /*!< 在沙箱环境下使用的键值对 */
        .key_sandbox    = "dpZZEpm9eBfqzK7yVeLq",
        .secret_sandbox = "THnfRRsU5vu6g6m9X6uFyAjUWflgZ0iyGjdEneKm",
    };
    ESP_ERROR_CHECK( alink_init(&product_info, alink_event_handler) );
    ```

2. **配网**
    - 事件回调函数：
        - 设备配网过程中的所有动作，都会传入事件回调函数中，您可以根据实际需求事件回调函数相应的做相应处理，如在当设备进入配置配网模式灯慢闪，等待激活时灯快闪等
        ```c
        typedef enum {
            ALINK_EVENT_CLOUD_CONNECTED = 0,/*!< ESP32 connected from alink cloude */
            ALINK_EVENT_CLOUD_DISCONNECTED, /*!< ESP32 disconnected from alink cloude */
            ALINK_EVENT_GET_DEVICE_DATA,    /*!< Alink cloud requests data from the device */
            ALINK_EVENT_SET_DEVICE_DATA,    /*!< Alink cloud to send data to the device */
            ALINK_EVENT_POST_CLOUD_DATA,    /*!< The device sends data to alink cloud  */
            ALINK_EVENT_WIFI_CONNECTED,     /*!< ESP32 station got IP from connected AP */
            ALINK_EVENT_WIFI_DISCONNECTED,  /*!< ESP32 station disconnected from AP */
            ALINK_EVENT_CONFIG_NETWORK,     /*!< The equipment enters the distribution mode */
            ALINK_EVENT_UPDATE_ROUTER,      /*!< Request to configure the router */
            ALINK_EVENT_FACTORY_RESET,      /*!< Request to restore factory settings */
            ALINK_EVENT_ACTIVATE_DEVICE,    /*!< Request activation device */
            ALINK_EVENT_HIGH_FREQUENCY_TEST,/*!< Enter the high frequency data transceiver test */
        } alink_event_t;
        ```
        - 如果您需要传入自己定义的事件，可以通过调用 `alink_event_send()` 发送自定义事件
        - 事件回调函数的栈默认大小为 1KB, 请避免在事件回调函数使用较大的栈空间，如需修改请使用 Makefile
    - 激活设备：在配网过程中设备需求很服务发送激活指令（具体指令内容由实际产品决定）
    - 主动上报：当设备成功连接到阿里云服务器时需要主动上报设备的状态，以保证云端数据与设备端同步，否则将无法配网成功

3. **修改触发方式**

    您需要根据实际产品，确定以何种方式触发出厂设置、重新配网、激活等操作，本示例使用的是按键触发方式，如果设备没有按键，您可以通过反复开关电源、通过蓝牙等方式触发，具体修改参见 `alink_key_trigger.c`
    ```c
    static void key_13_short_press(void)
    {
        ALINK_LOGD("short press..");
        alink_event_send(ALINK_EVENT_ACTIVATE_DEVICE);
    }

    static void key_13_long_press(void)
    {
        ALINK_LOGD("long press..");
        alink_event_send(ALINK_EVENT_UPDATE_ROUTER);
    }

    static void key_sw2_short_press(void)
    {
        ALINK_LOGD("short press..");
        alink_event_send(ALINK_EVENT_HIGH_FREQUENCY_TEST);
    }

    static void key_sw2_long_press(void)
    {
        ALINK_LOGD("long press..");
        alink_event_send(ALINK_EVENT_FACTORY_RESET);
    }
    ```

4. **数据通信**
    - 数据格式：确认设备通信时的数据格式（透传/非透传），目前 ALINK EMBED 只支持非透传
        - 透传：设备端收到的二进制格式的数据。由阿里云端服务器的 lua 脚本完成 json 格式与二进制数据的转换
        - 非透传：设备端收到的 JSON 格式的数据，JSON 格式的转换由设备端完成，阿里云端服务器的 lua 脚本是空实现
    - 数据长度：由于esp8266内存有限，因此数据长度调整为上报1KB，下发2KB
    - 数据上报：主动上报是由设备端主动发起
    - 数据下发：设备端收的到的数据有设置设备状态和获取设备状态组成

5. **日志等级**

    本工程的日志分为：alink 官方日志和 esp8266 适配层日志。日志共分为了七个级别，每一级别的日志在输出时均有颜色和相应的标识以区分，日志前带有"<>"的为alink 官方日志，设置日志后比当前级别低的日志均输出。
    - 配置

        ```bash
        # 日志等级列表
        # default 0 if LOG_ALINK_LEVEL_NONE
        # default 1 if LOG_ALINK_LEVEL_FATAL
        # default 2 if LOG_ALINK_LEVEL_ERROR
        # default 3 if LOG_ALINK_LEVEL_WARN
        # default 4 if LOG_ALINK_LEVEL_INFO
        # default 5 if LOG_ALINK_LEVEL_DEBUG
        # default 6 if LOG_ALINK_LEVEL_VERBOSE
        # esp8266 适配层日志等级配置
        CCFLAGS += -D CONFIG_LOG_ALINK_LEVEL=4
        # alink 官方日志等级配置
        CCFLAGS += -D CONFIG_ALINK_SDK_LOG_LEVEL=5
        ```
    - 示例
    alink 日志的使用方法与printf一致
        ```c
        /* 定义日志文件标识 */
        static const char *TAG = "sample_json";

        ALINK_LOGI("compile time : %s %s", __DATE__, __TIME__);
        ```
> 注：1. 模组认证时，需将 alink 官方日志等级设置为 bebug 模式;
> 　　2. 在进行高频测试时，需将日志等级都设置成 info 模式，以免过多的日志信息高频测试。

6. **信息存储**

    为了方便您的使用，我们将flash的读写操作进行了封装，您可以通过 key_value 的方式进行数据存储，无需关心 flash 的擦写，4字节对齐等问题。
    - 配置

        ```bash
        # 存储flash的位置
        CCFLAGS += -D CONFIG_INFO_STORE_MANAGER_ADDR=0x1f8000
        # 标识信息字符串 key 的最大长度
        CCFLAGS += -D CONFIG_INFO_STORE_KEY_LEN=16
        # 存储信息的个数，每多一条信息记录需要多占用 60B
        CCFLAGS += -D CONFIG_INFO_STORE_KEY_NUM=5
        ```
    - 示例

        ```c
        /*!< 初始化存储信息管理列表 */
        char buf[16] = "12345678910";
        esp_info_init();

        /*!< 数据保存 */
        esp_info_save("test_buf", buf, sizeof(buf));

        /*!< 数据加载 */
        memset(buf, 0, sizeof(buf));
        esp_info_load("test_buf", buf, sizeof(buf));
        printf("buf: %s\n", buf);

        /*!< 数据擦除 */
        esp_info_erase("test_buf");
        ```
    > 注：总存储信息量不能超过4KB。为了保证在写 flash 时出现突然断电，对数据的破坏，我们采用了12k的空间来存储4KB的数据，
    第一个4KB来记录存储的位置，另两个4KB的空间用于对数据进行存储备份。

7. **json 解析**

    为了方便您快速使用 json 格式数据的解析与生成，我们对 CJson 进行了封装，`esp_json_pack`和`esp_json_parse`支持泛类，字符，整类，浮点类型只需要通过一个API就可以完成。
    - 示例

        ```c
        int ret = 0;
        char *json_root = (char *)calloc(1, 512);
        char *json_sub = (char *)calloc(1, 64);
        int valueint = 0;
        char valuestring[20];
        double valuedouble = 0;
        /*
        {
            "key0": 0,
            "key1": "tmp1",
            "key4": {
                "key2": {
                    "name": "json test"
                },
                "key3": 1
            },
            "key5": 99.00000,
            "key6": 99.23456
        }
         */
        ret = esp_json_pack(json_root, "key0", valueint++);
        ret = esp_json_pack(json_root, "key1", "tmp1");
        ret = esp_json_pack(json_sub, "key2", "{\"name\":\"json test\"}");
        ret = esp_json_pack(json_sub, "key3", valueint++);
        ret = esp_json_pack(json_root, "key4", json_sub);
        ret = esp_json_pack(json_root, "key5", 99.23456);
        ret = esp_json_pack_double(json_root, "key6", 99.23456);
        printf("json_len: %d, json: %s\n", ret, json_root);

        printf("------------- json parse -----------\n");

        esp_json_parse(json_root, "key0", &valueint);
        printf("key0: %d\n", valueint);
        esp_json_parse(json_root, "key1", valuestring);
        printf("key1: %s\n", valuestring);
        esp_json_parse(json_root, "key4", json_sub);
        printf("key4: %s\n", json_sub);

        esp_json_parse(json_sub, "key2", valuestring);
        printf("key2: %s\n", valuestring);
        esp_json_parse(json_sub, "key3", &valueint);
        printf("key3: %d\n", valueint);
        esp_json_parse(json_root, "key5", &valueint);
        printf("key5: %d\n", valueint);
        /*!< CJson floating point type handling problems, later will be fixed */
        esp_json_parse(json_root, "key6", &valuedouble);

        /*!< printf can not output double type */
        char double_char[16] = {0};
        sprintf(double_char, "%lf", valuedouble);
        printf("key6: %s\n", double_char);

        free(json_root);
        free(json_sub);
        ```

## 11 注意事项
- 请定期更新本项目工程，后期将有持续对本工程进行优化改进
- 模组不支持 5G 网络，请确认已关闭路由器 5G 网络功能
- 测试热点配网时，请确认 4G 网络处于开启状态
- 本次更新修改了 chip_id，在烧录之前请先进行设备的解绑
- ALINK 受网络环境影响极大，进行测试时，请保证网络环境良好，否则将无法通过高频压测和稳定性测试。
- alink sds版本，目前不支持数据透传

## 12 Related links
* ESP8266概览 : http://www.espressif.com/zh-hans/support/explore/get-started/esp8266/getting-started-guide
* 烧录工具  : http://espressif.com/en/support/download/other-tools
* 串口驱动  : http://www.usb-drivers.org/ft232r-usb-uart-driver.html
* 阿里智能开放平台：https://open.aliplus.com/docs/open/
