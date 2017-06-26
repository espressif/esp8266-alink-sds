/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on ESPRESSIF SYSTEMS ESP8266 only, in which case,
 * it is free of charge, to any person obtaining a copy of this software and associated
 * documentation files (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
 * to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "esp_system.h"
#include "esp_misc.h"
#include "esp_wifi.h"
#include "esp_sta.h"
#include "esp_libc.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "alink_platform.h"
#include "esp_alink.h"
#include "esp_alink_log.h"
#include "esp_info_store.h"

static platform_awss_recv_80211_frame_cb_t g_sniffer_cb = NULL;
static const char *TAG = "alink_wifi";
/**
 * @brief Get timeout interval, in millisecond, of per awss.
 *
 * @param None.
 * @return The timeout interval.
 * @see None.
 * @note The recommended value is 60,000ms.
 */
int platform_awss_get_timeout_interval_ms(void)
{
    return 3 * 60 * 1000;
}

/**
 * @brief Get timeout interval in millisecond to connect the default SSID if awss timeout happens.
 *
 * @param None.
 * @return The timeout interval.
 * @see None.
 * @note The recommended value is 0ms, which mean forever.
 */
int platform_awss_get_connect_default_ssid_timeout_interval_ms(void)
{
    return 0;
}

/**
 * @brief Get time length, in millisecond, of per channel scan.
 *
 * @param None.
 * @return The timeout interval.
 * @see None.
 * @note None. The recommended value is between 200ms and 400ms.
 */
int platform_awss_get_channelscan_interval_ms(void)
{
    return 300;
}

/**
 * @brief Switch to specific wifi channel.
 *
 * @param[in] primary_channel @n Primary channel.
 * @param[in] secondary_channel @n Auxiliary channel if 40Mhz channel is supported, currently
 *              this param is always 0.
 * @param[in] bssid @n A pointer to wifi BSSID on which awss lock the channel, most platform
 *              may ignore it.
 */
void platform_awss_switch_channel(char primary_channel,
                                  char secondary_channel, uint8_t bssid[ETH_ALEN])
{
    int ret = wifi_set_channel(primary_channel);
    ALINK_ERROR_CHECK(ret != TRUE, ; , "wifi_set_channel,channel: %d", primary_channel)
}

void vendor_data_callback(unsigned char *buf, int length)
{
    g_sniffer_cb(buf, length, AWSS_LINK_TYPE_NONE, 0);
}

struct RxControl {
    signed rssi: 8;
    unsigned rate: 4;
    unsigned is_group: 1;
    unsigned: 1;
    unsigned sig_mode: 2;
    unsigned legacy_length: 12;
    unsigned damatch0: 1;
    unsigned damatch1: 1;
    unsigned bssidmatch0: 1;
    unsigned bssidmatch1: 1;
    unsigned MCS: 7;
    unsigned CWB: 1;
    unsigned HT_length: 16;
    unsigned Smoothing: 1;
    unsigned Not_Sounding: 1;
    unsigned: 1;
    unsigned Aggregation: 1;
    unsigned STBC: 2;
    unsigned FEC_CODING: 1;
    unsigned SGI: 1;
    unsigned rxend_state: 8;
    unsigned ampdu_cnt: 8;
    unsigned channel: 4;
    unsigned: 12;
};

struct Ampdu_Info {
    uint16 length;
    uint16 seq;
    uint8  address3[6];
};

struct sniffer_buf {
    struct RxControl rx_ctrl;       //12Byte
    uint8_t  buf[36];
    uint16_t cnt;
    struct Ampdu_Info ampdu_info[1];
};

struct sniffer_buf2 {
    struct RxControl rx_ctrl;
    //uint8 buf[112];
    uint8 buf[496];
    uint16 cnt;
    uint16 len; //length of packet
};

struct ieee80211_hdr {
    u16 frame_control;
    u16 duration_id;
    u8 addr1[ETH_ALEN];
    u8 addr2[ETH_ALEN];
    u8 addr3[ETH_ALEN];
    u16 seq_ctrl;
    u8 addr4[ETH_ALEN];
};

struct ht40_ctrl {
    uint16_t length;
    uint8_t filter;
    char rssi;
};

static void wifi_promiscuous_rx(uint8_t *buf, uint16_t buf_len)
{
    u8 *data;
    u32 data_len;

    if (buf_len == sizeof(struct sniffer_buf2)) {/* managment frame */
        struct sniffer_buf2 *sniffer = (struct sniffer_buf2 *)buf;
        data_len = sniffer->len;

        if (data_len > sizeof(sniffer->buf)) {
            data_len = sizeof(sniffer->buf);
        }

        data = sniffer->buf;
        vendor_data_callback(data, data_len);
    } else if (buf_len == sizeof(struct RxControl)) {/* mimo, HT40, LDPC */
        struct RxControl *rx_ctrl = (struct RxControl *)buf;
        struct ht40_ctrl ht40;
        ht40.rssi = rx_ctrl->rssi;

        if (rx_ctrl->Aggregation) {
            ht40.length = rx_ctrl->HT_length - 4;
        } else {
            ht40.length = rx_ctrl->HT_length;
        }

        ht40.filter = rx_ctrl->Smoothing << 5
                      | rx_ctrl->Not_Sounding << 4
                      | rx_ctrl->Aggregation << 3
                      | rx_ctrl->STBC << 1
                      | rx_ctrl->FEC_CODING;
        aws_80211_frame_handler((char *)&ht40, ht40.length, AWSS_LINK_TYPE_HT40_CTRL, 1);
    } else {//if (buf_len % 10 == 0) {
        struct sniffer_buf *sniffer = (struct sniffer_buf *)buf;
        data = buf + sizeof(struct RxControl);
        struct ieee80211_hdr *hdr = (struct ieee80211_hdr *)data;

        if (sniffer->cnt == 1) {
            data_len = sniffer->ampdu_info[0].length - 4;
            vendor_data_callback(data, data_len);
        } else {
            int i;

            //aws_printf("rx ampdu %d\n", sniffer->cnt);
            for (i = 1; i < sniffer->cnt; i++) {
                hdr->seq_ctrl = sniffer->ampdu_info[i].seq;
                memcpy(&hdr->addr3, sniffer->ampdu_info[i].address3, 6);

                data_len = sniffer->ampdu_info[i].length - 4;
                vendor_data_callback(data, data_len);
            }
        }
    }
}

/**
 * @brief Set wifi running at monitor mode,
   and register a callback function which will be called when wifi receive a frame.
 *
 * @param[in] cb @n A function pointer, called back when wifi receive a frame.
 */
void platform_awss_open_monitor(_IN_ platform_awss_recv_80211_frame_cb_t cb)
{
    g_sniffer_cb = cb;
    wifi_set_opmode(STATION_MODE);
    wifi_set_channel(6);
    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(wifi_promiscuous_rx);
    wifi_promiscuous_enable(1);
}

/**
 * @brief Close wifi monitor mode, and set running at station mode.
 */
void platform_awss_close_monitor(void)
{
    ALINK_LOGI("sniffer close");
    wifi_promiscuous_enable(0);
    wifi_set_promiscuous_rx_cb(NULL);
}

int platform_wifi_get_rssi_dbm(void)
{
    uint8_t rssi = wifi_station_get_rssi();
    return rssi;
}

char *platform_wifi_get_mac(_OUT_ char mac_str[PLATFORM_MAC_LEN])
{
    ALINK_PARAM_CHECK(mac_str == NULL);
    uint8_t mac[6] = {0};
    ESP_ERROR_CHECK(wifi_get_macaddr(STATION_IF, mac));
    snprintf(mac_str, PLATFORM_MAC_LEN, MACSTR, MAC2STR(mac));
    return mac_str;
}

uint32_t platform_wifi_get_ip(_OUT_ char ip_str[PLATFORM_IP_LEN])
{
    ALINK_PARAM_CHECK(ip_str == NULL);
    struct ip_info sta_ipconfig;
    wifi_get_ip_info(STATION_IF, &sta_ipconfig);
    memcpy(ip_str, inet_ntoa(sta_ipconfig.ip.addr), PLATFORM_IP_LEN);
    return sta_ipconfig.ip.addr;
}

static alink_err_t sys_net_is_ready = ALINK_FALSE;
int platform_sys_net_is_ready(void)
{
    return sys_net_is_ready;
}

static xSemaphoreHandle xSemConnet = NULL;
static void event_handler(System_Event_t *event)
{
    ALINK_ERROR_CHECK(!event, ; , "event == NULL");

    switch (event->event_id) {
        case EVENT_STAMODE_CONNECTED:
            ALINK_LOGI("EVENT_STAMODE_CONNECTED");
            break;

        case EVENT_STAMODE_GOT_IP:
            sys_net_is_ready = ALINK_TRUE;
            ALINK_LOGI("SYSTEM_EVENT_STA_GOT_IP");
            xSemaphoreGive(xSemConnet);
            alink_event_send(ALINK_EVENT_STA_GOT_IP);
            break;

        case EVENT_STAMODE_DISCONNECTED:
            ALINK_LOGI("SYSTEM_EVENT_STA_DISCONNECTED");
            sys_net_is_ready = ALINK_FALSE;
            alink_event_send(ALINK_EVENT_STA_DISCONNECTED);
            ESP_ERROR_CHECK(wifi_station_connect());
            break;

        default:
            break;
    }
}

int platform_awss_connect_ap(
    _IN_ uint32_t connection_timeout_ms,
    _IN_ char ssid[PLATFORM_MAX_SSID_LEN],
    _IN_ char passwd[PLATFORM_MAX_PASSWD_LEN],
    _IN_OPT_ enum AWSS_AUTH_TYPE auth,
    _IN_OPT_ enum AWSS_ENC_TYPE encry,
    _IN_OPT_ uint8_t bssid[ETH_ALEN],
    _IN_OPT_ uint8_t channel)
{
    struct station_config wifi_config;
    memset(&wifi_config, 0, sizeof(struct station_config));

    if (xSemConnet == NULL) {
        vSemaphoreCreateBinary(xSemConnet);
        wifi_set_event_handler_cb(event_handler);
        xSemaphoreTake(xSemConnet, 0);
    }

    strncpy(wifi_config.ssid, ssid, sizeof(wifi_config.ssid));
    strncpy(wifi_config.password, passwd, sizeof(wifi_config.password));
    ALINK_LOGI("ap ssid: %s, password: %s", wifi_config.ssid, wifi_config.password);
    ESP_ERROR_CHECK(wifi_set_opmode(STATION_MODE));
    ESP_ERROR_CHECK(wifi_station_set_config(&wifi_config));
    ESP_ERROR_CHECK(wifi_station_connect());

    ALINK_LOGD("connection_timeout_ms :%u",  connection_timeout_ms);
    alink_err_t err = xSemaphoreTake(xSemConnet, connection_timeout_ms / portTICK_RATE_MS);
    // if (err != pdTRUE) ESP_ERROR_CHECK( esp_wifi_stop() );
    ALINK_ERROR_CHECK(err != pdTRUE, ALINK_ERR, "xSemaphoreTake ret:%x wait: %d", err, connection_timeout_ms);

    if (!strcmp(ssid, "aha")) {
        return ALINK_OK;
    }

    err = esp_info_save(NVS_KEY_WIFI_CONFIG, &wifi_config, sizeof(struct station_config));

    if (err < 0) {
        ALINK_LOGE("alink information save failed");
    }

    return ALINK_OK;
}

/**
 * @brief send 80211 raw frame in current channel with basic rate(1Mbps)
 *
 * @param[in] type @n see enum platform_awss_frame_type, currently only FRAME_BEACON
 *                      FRAME_PROBE_REQ is used
 * @param[in] buffer @n 80211 raw frame, include complete mac header & FCS field
 * @param[in] len @n 80211 raw frame length
 * @return
   @verbatim
   =  0, send success.
   = -1, send failure.
   = -2, unsupported.
   @endverbatim
 * @see None.
 * @note awss use this API send raw frame in wifi monitor mode & station mode
 */
int platform_wifi_send_80211_raw_frame(_IN_ enum platform_awss_frame_type type,
                                       _IN_ uint8_t *buffer, _IN_ int len)
{
    return -2;
}

/**
 * @brief enable/disable filter specific management frame in wifi station mode
 *
 * @param[in] filter_mask @n see mask macro in enum platform_awss_frame_type,
 *                      currently only FRAME_PROBE_REQ_MASK & FRAME_BEACON_MASK is used
 * @param[in] vendor_oui @n oui can be used for precise frame match, optional
 * @param[in] callback @n see platform_wifi_mgnt_frame_cb_t, passing 80211
 *                      frame or ie to callback. when callback is NULL
 *                      disable sniffer feature, otherwise enable it.
 * @return
   @verbatim
   =  0, success
   = -1, fail
   = -2, unsupported.
   @endverbatim
 * @see None.
 * @note awss use this API to filter specific mgnt frame in wifi station mode
 */

int platform_wifi_enable_mgnt_frame_filter(
    _IN_ uint32_t filter_mask,
    _IN_OPT_ uint8_t vendor_oui[3],
    _IN_ platform_wifi_mgnt_frame_cb_t callback)
{
    return -2;
}

/**
 * @brief launch a wifi scan operation
 *
 * @param[in] cb @n pass ssid info(scan result) to this callback one by one
 * @return 0 for wifi scan is done, otherwise return -1
 * @see None.
 * @note
 *      This API should NOT exit before the invoking for cb is finished.
 *      This rule is something like the following :
 *      platform_wifi_scan() is invoked...
 *      ...
 *      for (ap = first_ap; ap <= last_ap; ap = next_ap){
 *        cb(ap)
 *      }
 *      ...
 *      platform_wifi_scan() exit...
 */
static platform_wifi_scan_result_cb_t g_wifi_scan_result_cb = NULL;
static struct bss_info *g_ap_info = NULL;
static xSemaphoreHandle xSemScan = NULL;

static is_ap_node(char *ssid)
{
    struct bss_info *p = g_ap_info;

    while (p != NULL) {
        ALINK_LOGI("ssid: %s", p->ssid);

        if (!strcmp(ssid, p->ssid)) {
            return ALINK_TRUE;
        }

        p = STAILQ_NEXT(p, next);
    }

    return ALINK_FALSE;
}

static void scan_done_cb(void *arg, STATUS status)
{
    struct bss_info *ap_info = (struct bss_info *)arg;
    struct bss_info *p = NULL;

    for (; ap_info != NULL; ap_info = STAILQ_NEXT(ap_info, next)) {
        if (is_ap_node(ap_info->ssid) == ALINK_TRUE) {
            continue;
        }

        p = malloc(sizeof(struct bss_info));
        memcpy(p, ap_info, sizeof(struct bss_info));
        STAILQ_NEXT(ap_info, next) = g_ap_info;
        ALINK_LOGD("ssid: %s, free: %d", p->ssid, system_get_free_heap_size());
        g_ap_info = p;
    }

    xSemaphoreGive(xSemScan);
}

int platform_wifi_scan(platform_wifi_scan_result_cb_t cb)
{
    ALINK_PARAM_CHECK(!cb);

    alink_err_t ret = 0;
    g_wifi_scan_result_cb = cb;
    struct scan_config config;
    memset(&config, 0, sizeof(struct scan_config));
    ret = wifi_station_scan(&config, scan_done_cb);
    ALINK_ERROR_CHECK(ret != TRUE, ALINK_ERR, "wifi_station_scan");

    if (xSemScan == NULL) {
        vSemaphoreCreateBinary(xSemScan);
        xSemaphoreTake(xSemScan, 0);
    }

    xSemaphoreTake(xSemScan, portMAX_DELAY);

    struct bss_info *p = NULL;

    while (g_ap_info != NULL) {
        p = g_ap_info;
        ALINK_LOGD("ssid: %s, bssid: %s, authmode: %d, channel: %d, rssi: %d",
                   (char *)p->ssid, (uint8_t *)p->bssid, p->authmode, p->channel, p->rssi);
        g_ap_info = STAILQ_NEXT(g_ap_info, next);

        if (is_ap_node(p->ssid) == ALINK_FALSE) {
            g_wifi_scan_result_cb((char *)p->ssid, (uint8_t *)p->bssid, p->authmode,
                                  AWSS_ENC_TYPE_INVALID, p->channel, p->rssi, 1);
        }

        free(p);
    }

    g_ap_info = NULL;
    return ALINK_OK;
}

#define AES_BLOCK_SIZE 16
/**
 * @brief initialize AES struct.
 *
 * @param[in] key:
 * @param[in] iv:
 * @param[in] dir: AES_ENCRYPTION or AES_DECRYPTION
 * @return AES128_t
 */
#include "mbedtls/aes.h"
typedef struct {
    mbedtls_aes_context ctx;
    uint8_t iv[16];
} platform_aes_t;

p_aes128_t platform_aes128_init(
    _IN_ const uint8_t *key,
    _IN_ const uint8_t *iv,
    _IN_ AES_DIR_t dir)
{
    ALINK_PARAM_CHECK(!key);
    ALINK_PARAM_CHECK(!iv);

    alink_err_t ret = 0;
    platform_aes_t *p_aes128 = NULL;
    p_aes128 = (platform_aes_t *)calloc(1, sizeof(platform_aes_t));
    ALINK_ERROR_CHECK(!p_aes128, NULL, "calloc");

    mbedtls_aes_init(&p_aes128->ctx);

    if (dir == PLATFORM_AES_ENCRYPTION) {
        ret = mbedtls_aes_setkey_enc(&p_aes128->ctx, key, 128);
    } else {
        ret = mbedtls_aes_setkey_dec(&p_aes128->ctx, key, 128);
    }

    if (ret != ALINK_OK) {
        free(p_aes128);
    }

    ALINK_ERROR_CHECK(ret != ALINK_OK, NULL, "mbedtls_aes_setkey_enc");

    memcpy(p_aes128->iv, iv, 16);
    return (p_aes128_t *)p_aes128;
}

/**
 * @brief release AES struct.
 *
 * @param[in] aes:
 * @return
   @verbatim
     = 0: succeeded
     = -1: failed
   @endverbatim
 * @see None.
 * @note None.
 */
int platform_aes128_destroy(_IN_ p_aes128_t aes)
{
    ALINK_PARAM_CHECK(!aes);
    mbedtls_aes_free(&((platform_aes_t *)aes)->ctx);
    free(aes);
    return ALINK_OK;
}

/**
 * @brief encrypt data with aes (cbc/128bit key).
 *
 * @param[in] aes: AES handler
 * @param[in] src: plain data
 * @param[in] blockNum: plain data number of 16 bytes size
 * @param[out] dst: cipher data
 * @return
   @verbatim
     = 0: succeeded
     = -1: failed
   @endverbatim
 * @see None.
 * @note None.
 */
int platform_aes128_cbc_encrypt(
    _IN_ p_aes128_t aes,
    _IN_ const void *src,
    _IN_ size_t blockNum,
    _OUT_ void *dst)
{
    ALINK_PARAM_CHECK(!aes);
    ALINK_PARAM_CHECK(!src);
    ALINK_PARAM_CHECK(!dst);

    alink_err_t ret = 0;
    int i = 0;
    platform_aes_t *p_aes128 = (platform_aes_t *)aes;

    for (i = 0; i < blockNum; ++i) {
        ret = mbedtls_aes_crypt_cbc(&p_aes128->ctx, MBEDTLS_AES_ENCRYPT, AES_BLOCK_SIZE,
                                    p_aes128->iv, src, dst);
        src = (uint8_t *)src + 16;
        dst = (uint8_t *)dst + 16;
    }

    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR, "mbedtls_aes_crypt_cbc, ret: %d", ret);
    return ALINK_OK;
}

/**
 * @brief decrypt data with aes (cbc/128bit key).
 *
 * @param[in] aes: AES handler
 * @param[in] src: cipher data
 * @param[in] blockNum: plain data number of 16 bytes size
 * @param[out] dst: plain data
 * @return
   @verbatim
     = 0: succeeded
     = -1: failed
   @endverbatim
 * @see None.
 * @note None.
 */
int platform_aes128_cbc_decrypt(
    _IN_ p_aes128_t aes,
    _IN_ const void *src,
    _IN_ size_t blockNum,
    _OUT_ void *dst)
{
    ALINK_PARAM_CHECK(!aes);
    ALINK_PARAM_CHECK(!src);
    ALINK_PARAM_CHECK(!dst);

    alink_err_t ret = 0;
    int i = 0;
    platform_aes_t *p_aes128 = (platform_aes_t *)aes;

    for (i = 0; i < blockNum; ++i) {
        ret = mbedtls_aes_crypt_cbc(&p_aes128->ctx, MBEDTLS_AES_DECRYPT, AES_BLOCK_SIZE,
                                    p_aes128->iv, src, dst);
        src = (uint8_t *)src + 16;
        dst = (uint8_t *)dst + 16;
    }

    ALINK_ERROR_CHECK(ret != ALINK_OK, ALINK_ERR,
                      "mbedtls_aes_crypt_cbc, ret: %d, blockNum: %d", ret, blockNum);
    return ALINK_OK;
}

/**
 * @brief get the information of the connected AP.
 *
 * @param[out] ssid: array to store ap ssid. It will be null if ssid is not required.
 * @param[out] passwd: array to store ap password. It will be null if ap password is not required.
 * @param[out] bssid: array to store ap bssid. It will be null if bssid is not required.
 * @return
   @verbatim
     = 0: succeeded
     = -1: failed
   @endverbatim
 * @see None.
 * @note None.
 */

int platform_wifi_get_ap_info(
    _OUT_ char ssid[PLATFORM_MAX_SSID_LEN],
    _OUT_ char passwd[PLATFORM_MAX_PASSWD_LEN],
    _OUT_ uint8_t bssid[ETH_ALEN])
{
    alink_err_t ret = 0;
    struct station_config ap_info[5];
    memset(ap_info, 0, sizeof(ap_info));
    ret = wifi_station_get_ap_info(ap_info);
    ALINK_LOGI("wifi_station_get_ap_info number: %d", ret);

    if (ssid) {
        memcpy(ssid, ap_info[0].ssid, PLATFORM_MAX_SSID_LEN);
    }

    if (bssid) {
        memcpy(bssid, ap_info[0].bssid, ETH_ALEN);
    }

    if (passwd) {
        memcpy(passwd, ap_info[0].password, PLATFORM_MAX_PASSWD_LEN);
    }

    return ALINK_OK;
}
