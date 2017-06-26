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
#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_libc.h"
#include "esp_system.h"

#include "alink_platform.h"
#include "esp_alink.h"
#include "esp_info_store.h"
#include "esp_alink_log.h"

#define ALINK_CHIPID              "esp8266"
#define MODULE_NAME               "ESP-WROOM-02"

#define PLATFORM_TABLE_CONTENT_CNT(table) (sizeof(table)/sizeof(table[0]))

static const char *TAG = "alink_os";

typedef struct task_name_handler_content {
    const char *task_name;
    void *handler;
} task_infor_t;

static task_infor_t task_infor[] = {
    {"wsf_receive_worker", NULL},
    {"alcs_thread", NULL},
    {"work queue", NULL},
    {NULL, NULL}
};

/************************ memory manage ************************/
void *platform_malloc(_IN_ uint32_t size)
{
    void *c = malloc(size);
    // ALINK_LOGV("malloc: ptr: %p, size: %d free_heap :%u", c, size, system_get_free_heap_size());
    ALINK_ERROR_CHECK(c == NULL, NULL, "malloc size :%d free_heap :%u\n",
                      size, system_get_free_heap_size());
    return c;
}

void platform_free(_IN_ void *ptr)
{
    if (ptr == NULL) {
        return;
    }

    free(ptr);
    // ALINK_LOGV("free: ptr: %p free_heap :%u", ptr, system_get_free_heap_size());
}


/************************ mutex manage ************************/
void *platform_mutex_init(void)
{
    xSemaphoreHandle mux_sem = NULL;
    mux_sem = xSemaphoreCreateMutex();
    ALINK_ERROR_CHECK(mux_sem == NULL, NULL, "xSemaphoreCreateMutex");
    return mux_sem;
}

void platform_mutex_destroy(_IN_ void *mutex)
{
    ALINK_PARAM_CHECK(mutex == NULL);
    vSemaphoreDelete(mutex);
}

void platform_mutex_lock(_IN_ void *mutex)
{
    //if can not get the mux,it will wait all the time
    ALINK_PARAM_CHECK(mutex == NULL);
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void platform_mutex_unlock(_IN_ void *mutex)
{
    ALINK_PARAM_CHECK(mutex == NULL);
    ALINK_PARAM_CHECK(mutex == NULL);
    xSemaphoreGive(mutex);
}

/************************ semaphore manage ************************/
void *platform_semaphore_init(void)
{
    xSemaphoreHandle count_handler = NULL;
    count_handler = xSemaphoreCreateCounting(255, 0);
    ALINK_ERROR_CHECK(count_handler == NULL, NULL, "xSemaphoreCreateCounting");
    return count_handler;
}

void platform_semaphore_destroy(_IN_ void *sem)
{
    ALINK_PARAM_CHECK(sem == NULL);
    vSemaphoreDelete(sem);
}

int platform_semaphore_wait(_IN_ void *sem, _IN_ uint32_t timeout_ms)
{
    ALINK_PARAM_CHECK(sem == NULL);

    //Take the Semaphore
    if (pdTRUE == xSemaphoreTake(sem, timeout_ms / portTICK_RATE_MS)) {
        return 0;
    }

    return -1;
}

void platform_semaphore_post(_IN_ void *sem)
{
    ALINK_PARAM_CHECK(sem == NULL);
    xSemaphoreGive(sem);
}

void platform_msleep(_IN_ uint32_t ms)
{
    vTaskDelay(ms / portTICK_RATE_MS);
}

uint32_t platform_get_time_ms(void)
{
    return system_get_time() / 1000;
}

int platform_thread_get_stack_size(_IN_ const char *thread_name)
{
    ALINK_PARAM_CHECK(thread_name == NULL);

    if (0 == strcmp(thread_name, "work queue")) {
        ALINK_LOGD("get work queue");
        return 0xd00;
    } else if (0 == strcmp(thread_name, "wsf_receive_worker")) {
        ALINK_LOGD("get wsf_receive_worker");
        return 0xd00;
    } else if (0 == strcmp(thread_name, "alcs_thread")) {
        ALINK_LOGD("get alcs_thread");
        return 0x1000;
    } else {
        ALINK_LOGE("get othrer thread: %s", thread_name);
        return 0x800;
    }
}

/************************ task ************************/
/*
    return -1: not found the name from the list
          !-1: found the pos in the list
*/
static int get_task_name_location(_IN_ const char *name)
{
    uint32_t i = 0;
    uint32_t len = 0;

    for (i = 0; task_infor[i].task_name != NULL; i++) {
        len = (strlen(task_infor[i].task_name) >= configMAX_TASK_NAME_LEN ? configMAX_TASK_NAME_LEN : strlen(task_infor[i].task_name));

        if (0 == memcmp(task_infor[i].task_name, name, len)) {
            return i;
        }
    }

    return ALINK_ERR;
}

static bool set_task_name_handler(uint32_t pos, _IN_ void *handler)
{
    ALINK_PARAM_CHECK(handler == NULL);
    task_infor[pos].handler = handler;
    return ALINK_OK;
}

typedef void (*TaskFunction_t)(void *);
int platform_thread_create(_OUT_ void **thread,
                           _IN_ const char *name,
                           _IN_ void *(*start_routine)(void *),
                           _IN_ void *arg,
                           _IN_ void *stack,
                           _IN_ uint32_t stack_size,
                           _OUT_ int *stack_used)
{
    ALINK_PARAM_CHECK(name == NULL);
    ALINK_PARAM_CHECK(stack_size == 0);
    ALINK_PARAM_CHECK(!start_routine);
    alink_err_t ret;

    uint8_t task_priority = DEFAULU_TASK_PRIOTY;

    if (!strcmp(name, "work queue")) {
        task_priority++;
    }

    ret = xTaskCreate((TaskFunction_t)start_routine, name, (stack_size) / 4, arg, task_priority, thread);
    ALINK_ERROR_CHECK(ret != pdTRUE, ALINK_ERR, "thread_create name: %s, stack_size: %d, ret: %d", name, stack_size, ret);
    ALINK_LOGD("thread_create name: %s, stack_size: %d, priority:%d, thread_handle: %p",
               name, stack_size, task_priority, *thread);

    int pos = get_task_name_location(name);

    if (pos == ALINK_ERR) {
        ALINK_LOGE("get_task_name_location name: %s", name);
        vTaskDelete(*thread);
    }

    set_task_name_handler(pos, *thread);
    return ALINK_OK;
}

void platform_thread_exit(_IN_ void *thread)
{
    ALINK_LOGD("thread_handle: %p", thread);
    vTaskDelete(thread);
    thread = NULL;
}

void platform_thread_info_print()
{
    int i = 0;

    for (i = 0; task_infor[i].task_name != NULL; i++) {
        if (task_infor[i].handler) {
            ALINK_LOGD("name: %-20s handle: %p free_heap_size: %dByte",
                       task_infor[i].task_name, task_infor[i].handler, uxTaskGetStackHighWaterMark(task_infor[i].handler) * 4);
        }
    }
}

/************************ config ************************/
int platform_config_read(_OUT_ char *buffer, _IN_ int length)
{
    ALINK_PARAM_CHECK(buffer == NULL);
    ALINK_PARAM_CHECK(length < 0);
    ALINK_LOGD("buffer: %p, length: %d", buffer, length);

    int ret = 0;
    ret = esp_info_load(ALINK_CONFIG_KEY, buffer, length);
    ALINK_ERROR_CHECK(ret < 0, ALINK_ERR, "esp_info_load");

    return ALINK_OK;
}

int platform_config_write(_IN_ const char *buffer, _IN_ int length)
{
    ALINK_PARAM_CHECK(buffer == NULL);
    ALINK_PARAM_CHECK(length < 0);
    ALINK_LOGD("buffer: %p, length: %d", buffer, length);

    int ret = 0;
    ret = esp_info_save(ALINK_CONFIG_KEY, buffer, length);
    ALINK_ERROR_CHECK(ret < 0, ALINK_ERR, "esp_info_load");

    return ALINK_ERR;
}


char *platform_get_chipid(_OUT_ char cid_str[PLATFORM_CID_LEN])
{
    ALINK_PARAM_CHECK(cid_str == NULL);
    memcpy(cid_str, ALINK_CHIPID, PLATFORM_CID_LEN);
    return cid_str;
}

char *platform_get_os_version(_OUT_ char version_str[STR_SHORT_LEN])
{
    ALINK_PARAM_CHECK(version_str == NULL);
    const char *idf_version = system_get_sdk_version();
    memcpy(version_str, idf_version, STR_SHORT_LEN);
    return version_str;
}

char *platform_get_module_name(_OUT_ char name_str[STR_SHORT_LEN])
{
    ALINK_PARAM_CHECK(name_str == NULL);
    memcpy(name_str, MODULE_NAME, STR_SHORT_LEN);
    return name_str;
}

void platform_sys_reboot(void)
{
    system_restart();
}
