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

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>

#include "lwip/sockets.h"
#include "lwip/netdb.h"

#include "esp_misc.h"

#include "alink_platform.h"
#include "esp_alink.h"
#include "esp_alink_log.h"

#define SOMAXCONN            5
#define ALINK_SOCKET_TIMEOUT 20000

static const char *TAG = "alink_network";

static alink_err_t network_create_socket(pplatform_netaddr_t netaddr, int type,
        struct sockaddr_in *paddr, int *psock)
{
    ALINK_PARAM_CHECK(netaddr == NULL);
    ALINK_PARAM_CHECK(paddr == NULL);
    ALINK_PARAM_CHECK(psock == NULL);

    struct hostent *hp;
    uint32_t ip;
    alink_err_t ret;

    if (NULL == netaddr->host) {
        ip = htonl(INADDR_ANY);
    } else {
        ALINK_LOGI("alink server host: %s", netaddr->host);
        hp = gethostbyname(netaddr->host);
        ALINK_ERROR_CHECK(hp == NULL, ALINK_ERR, "gethostbyname ret:%p", hp);

        struct ip4_addr *ip4_addr = (struct ip4_addr *)hp->h_addr;
        char ipaddr_tmp[64] = {0};
        sprintf(ipaddr_tmp, IPSTR, IP2STR(ip4_addr));
        ALINK_LOGI("ip: %s, port: %d", ipaddr_tmp, netaddr->port);
        ip = inet_addr(ipaddr_tmp);
    }

    *psock = socket(AF_INET, type, 0);

    if (*psock < 0) {
        return -1;
    }

    memset(paddr, 0, sizeof(struct sockaddr_in));

    int opt_val = 1;
    ret = setsockopt(*psock, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
    ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "setsockopt SO_REUSEADDR errno: %d", errno);

    if (type == SOCK_DGRAM) {
        ret = setsockopt(*psock, SOL_SOCKET, SO_BROADCAST, &opt_val, sizeof(opt_val));

        if (ret != 0) {
            close((int)*psock);
        }

        ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "setsockopt SO_BROADCAST errno: %d", errno);
    }

    struct timeval timeout = {ALINK_SOCKET_TIMEOUT, 0};

    ret = setsockopt((int) * psock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(struct timeval));

    ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "setsockopt SO_RCVTIMEO errno: %d", errno);

    ALINK_LOGD("setsockopt: recv timeout %dms", ALINK_SOCKET_TIMEOUT);

    ret = setsockopt((int) * psock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(struct timeval));

    ALINK_ERROR_CHECK(ret != 0, ALINK_ERR, "setsockopt SO_SNDTIMEO errno: %d", errno);

    ALINK_LOGD("setsockopt: send timeout %dms", ALINK_SOCKET_TIMEOUT);

    paddr->sin_addr.s_addr = ip;

    paddr->sin_family = AF_INET;

    paddr->sin_port = htons(netaddr->port);

    return ALINK_OK;
}

void *platform_udp_server_create(_IN_ uint16_t port)
{
    struct sockaddr_in addr;
    int server_socket;
    alink_err_t ret;
    platform_netaddr_t netaddr = {NULL, port};

    ret = network_create_socket(&netaddr, SOCK_DGRAM, &addr, &server_socket);
    ALINK_ERROR_CHECK(ret != ALINK_OK, NULL, "network_create_socket");

    ret = bind(server_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

    if (-1 == bind(server_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
        platform_udp_close((void *)server_socket);
        ALINK_LOGE("socket bind");
        // perror("socket bind");
        return NULL;
    }

    return (void *)server_socket;
}

void *platform_udp_client_create(void)
{
    struct sockaddr_in addr;
    int sock;
    platform_netaddr_t netaddr = {NULL, 0};
    alink_err_t ret;

    ret = network_create_socket(&netaddr, SOCK_DGRAM, &addr, &sock);
    ALINK_ERROR_CHECK(ret != ALINK_OK, NULL, "network_create_socket");
    return (void *)sock;
}

void *platform_udp_multicast_server_create(pplatform_netaddr_t netaddr)
{
    ALINK_PARAM_CHECK(netaddr == NULL);
    struct sockaddr_in addr;
    int sock;
    struct ip_mreq mreq;
    alink_err_t ret = 0;

    platform_netaddr_t netaddr_client = {NULL, netaddr->port};

    memset(&addr, 0, sizeof(addr));
    memset(&mreq, 0, sizeof(mreq));

    ret = network_create_socket(&netaddr_client, SOCK_DGRAM, &addr, &sock);
    ALINK_ERROR_CHECK(ret != ALINK_OK, NULL, "network_create_socket");

    if (-1 == bind(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
        ALINK_LOGE("socket bind, errno: %d", errno);
        // perror("socket bind");
        platform_udp_close((void *)sock);
        return NULL;
    }

    mreq.imr_multiaddr.s_addr = inet_addr(netaddr->host);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);

    if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *) &mreq, sizeof(mreq)) < 0) {
        ALINK_LOGE("setsockopt IP_ADD_MEMBERSHIP");
        platform_udp_close((void *)sock);
        return NULL;
    }

    return (void *)sock;
}

void platform_udp_close(void *handle)
{
    close((int)handle);
}


int platform_udp_sendto(
    _IN_ void *handle,
    _IN_ const char *buffer,
    _IN_ uint32_t length,
    _IN_ pplatform_netaddr_t netaddr)
{
    ALINK_PARAM_CHECK((int)handle < 0);
    ALINK_PARAM_CHECK(buffer == NULL);
    ALINK_PARAM_CHECK(netaddr == NULL);
    int ret_code;
    struct sockaddr_in addr;
    int sockfd = (int)handle;
    // do {
    // struct hostent *hp;
    // ALINK_LOGW("sockfd: %d, host: %s, port: %d", sockfd, netaddr->host, netaddr->port);
    // hp = gethostbyname(netaddr->host);
    // ALINK_ERROR_CHECK(hp == NULL, ALINK_ERR, "gethostbyname Can't resolute the host address hp:%p", hp);
    // ALINK_LOGW("sendto crash debug");

    // addr.sin_addr.s_addr = *((u_int *)(hp->h_addr));
    // } while (0);

    addr.sin_addr.s_addr = inet_addr(netaddr->host);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(netaddr->port);

    ret_code = sendto(sockfd,
                      buffer,
                      length,
                      0,
                      (struct sockaddr *)&addr,
                      sizeof(struct sockaddr_in));

    ALINK_ERROR_CHECK(ret_code <= 0, ALINK_ERR, "sendto ret:%d, errno:%d", ret_code, errno);
    return ret_code;
}


int platform_udp_recvfrom(
    _IN_ void *handle,
    _OUT_ char *buffer,
    _IN_ uint32_t length,
    _OUT_OPT_ pplatform_netaddr_t netaddr)
{
    ALINK_PARAM_CHECK((int)handle < 0);
    ALINK_PARAM_CHECK(buffer == NULL);
    int ret_code;
    struct sockaddr_in addr;
    unsigned int addr_len = sizeof(addr);

    ret_code = recvfrom((int)handle, buffer, length, 0, (struct sockaddr *)&addr, &addr_len);
    ALINK_ERROR_CHECK(ret_code <= 0, ALINK_ERR, "recvfrom ret:%d, errno:%d", ret_code, errno);

    if (netaddr != 0) {
        ALINK_LOGD("port :%u, host: %d, ret_code: %d", netaddr->host, addr.sin_port, ret_code);
    }

    if (NULL != netaddr) {
        netaddr->port = ntohs(addr.sin_port);

        if (NULL != netaddr->host) {
            strcpy(netaddr->host, inet_ntoa(addr.sin_addr));
        }
    }

    return ret_code;
}


void *platform_tcp_server_create(_IN_ uint16_t port)
{
    struct sockaddr_in addr;
    int server_socket;
    alink_err_t ret = 0;
    platform_netaddr_t netaddr = {NULL, port};

    ret = network_create_socket(&netaddr, SOCK_STREAM, &addr, &server_socket);
    ALINK_ERROR_CHECK(ret != ALINK_OK, NULL, "network_create_socket");

    if (-1 == bind(server_socket, (struct sockaddr *)&addr, sizeof(addr))) {
        ALINK_LOGE("bind");
        platform_tcp_close((void *)server_socket);
        return NULL;
    }

    if (0 != listen(server_socket, SOMAXCONN)) {
        ALINK_LOGE("listen");
        platform_tcp_close((void *)server_socket);
        return NULL;
    }

    return (void *)server_socket;
}


void *platform_tcp_server_accept(_IN_ void *server)
{
    ALINK_PARAM_CHECK(server == NULL);
    struct sockaddr_in addr;
    unsigned int addr_length = sizeof(addr);
    int new_client;

    new_client = accept((int)server, (struct sockaddr *)&addr, &addr_length);
    ALINK_ERROR_CHECK(new_client <= 0, NULL, "accept errno:%d", errno);

    return (void *)new_client;
}


void *platform_tcp_client_connect(_IN_ pplatform_netaddr_t netaddr)
{
    ALINK_PARAM_CHECK(netaddr == NULL);
    struct sockaddr_in addr;
    int sock;
    alink_err_t ret = 0;

    ret = network_create_socket(netaddr, SOCK_STREAM, &addr, &sock);
    ALINK_ERROR_CHECK(ret != ALINK_OK, NULL, "network_create_socket");

    if (-1 == connect(sock, (struct sockaddr *)&addr, sizeof(struct sockaddr_in))) {
        ALINK_LOGE("connect errno: %d", errno);
        // perror("connect");
        platform_tcp_close((void *)sock);
        return NULL;
    }

    ALINK_LOGD("sockfd:%d, free_heap :%u", sock, system_get_free_heap_size());
    return (void *)sock;
}


int platform_tcp_send(_IN_ void *handle, _IN_ const char *buffer, _IN_ uint32_t length)
{
    ALINK_PARAM_CHECK((int)handle < 0);
    ALINK_PARAM_CHECK(buffer == NULL);
    int bytes_sent;

    bytes_sent = send((int)handle, buffer, length, 0);
    ALINK_ERROR_CHECK(bytes_sent <= 0, ALINK_ERR, "send ret:%d", bytes_sent);
    return bytes_sent;
}


int platform_tcp_recv(_IN_ void *handle, _OUT_ char *buffer, _IN_ uint32_t length)
{
    ALINK_PARAM_CHECK((int)handle < 0);
    ALINK_PARAM_CHECK(buffer == NULL);

    int bytes_received;
    ALINK_LOGD("recv start, length: %d", length);
    bytes_received = recv((int)handle, buffer, length, 0);
    ALINK_LOGD("recv end");
    ALINK_ERROR_CHECK(bytes_received <= 0, ALINK_ERR, "recv ret:%d, errno: %d", bytes_received, errno);
    return bytes_received;
}


void platform_tcp_close(_IN_ void *handle)
{
    close((int)handle);
}

int platform_select(void *read_fds[PLATFORM_SOCKET_MAXNUMS],
                    void *write_fds[PLATFORM_SOCKET_MAXNUMS],
                    int timeout_ms)
{
    int i, ret = -1;
    struct timeval timeout_value;
    struct timeval *ptimeval = &timeout_value;
    fd_set r_set, w_set;
    int max_fd = -1;

    if (PLATFORM_WAIT_INFINITE == timeout_ms) {
        ptimeval = NULL;
    } else {
        ptimeval->tv_sec = timeout_ms / 1000;
        ptimeval->tv_usec = (timeout_ms % 1000) * 1000;
    }

    FD_ZERO(&r_set);
    FD_ZERO(&w_set);

    if (read_fds) {
        for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
            if (PLATFORM_INVALID_FD != read_fds[i]) {
                FD_SET((long)read_fds[i], &r_set);
            }

            if ((int)read_fds[i] > max_fd) {
                max_fd = (int)read_fds[i];
            }
        }
    }

    if (write_fds) {
        for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
            if (PLATFORM_INVALID_FD != write_fds[i]) {
                FD_SET((long)write_fds[i], &w_set);
            }

            if ((int)read_fds[i] > max_fd) {
                max_fd = (int)read_fds[i];
            }
        }
    }

    ret = select(max_fd + 1, &r_set, &w_set, NULL, ptimeval);

    if (ret > 0) {
        if (read_fds) {
            for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
                if (PLATFORM_INVALID_FD != read_fds[i]
                        && !FD_ISSET((long)read_fds[i], &r_set)) {
                    read_fds[i] = PLATFORM_INVALID_FD;
                }
            }
        }

        if (write_fds) {
            for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
                if (PLATFORM_INVALID_FD != write_fds[i]
                        && !FD_ISSET((long)write_fds[i], &w_set)) {
                    write_fds[i] = PLATFORM_INVALID_FD;
                }
            }
        }
    } else {/* clear all fd */
        if (read_fds) {
            for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
                read_fds[i] = PLATFORM_INVALID_FD;
            }
        }

        if (write_fds) {
            for (i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i) {
                write_fds[i] = PLATFORM_INVALID_FD;
            }
        }
    }

    ALINK_ERROR_CHECK(ret < 0, ret, "select ret:%d, errno: %d", ret, errno);
    return ret;
}
