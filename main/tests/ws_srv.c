/*
 * Copyright (c) 2024, Jacques Gagnon
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include <esp_event.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "esp_netif.h"
#include "esp_mac.h"
#include "esp_eth.h"
#include <esp_http_server.h>
#include "ws_srv.h"
#include "cmds.h"

static esp_eth_handle_t s_eth_handle = NULL;
static esp_eth_mac_t *s_mac = NULL;
static esp_eth_phy_t *s_phy = NULL;
static esp_eth_netif_glue_handle_t s_eth_glue = NULL;
static httpd_handle_t server = NULL;

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        printf("# Handshake done, the new connection was opened\n");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        printf("# httpd_ws_recv_frame failed to get frame len with %d\n", ret);
        return ret;
    }

    if (ws_pkt.len) {
        buf = calloc(1, ws_pkt.len);
        if (buf == NULL) {
            printf("# Failed to calloc memory for buf\n");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;

        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            printf("# httpd_ws_recv_frame failed with %d\n", ret);
            free(buf);
            return ret;
        }

        /* Execute command and get back logs */
        char *rsp = tests_cmds_hdlr((struct tests_cmds_pkt *)ws_pkt.payload);
        ws_pkt.payload = (uint8_t *)rsp;
        ws_pkt.len = strlen(rsp);
        ret = httpd_ws_send_frame(req, &ws_pkt);
        if (ret != ESP_OK) {
            printf("# httpd_ws_send_frame failed with %d\n", ret);
        }
        free(buf);
    }

    return ret;
}

static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Start the httpd server */
    printf("# Starting ws server on port: '%d'\n", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        /* Registering the ws handler */
        printf("# Registering URI handlers\n");
        httpd_register_uri_handler(server, &ws);
        return server;
    }

    printf("# Error starting ws server!\n");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server) {
    /* Stop the httpd server */
    return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
        int32_t event_id, void* event_data) {
    httpd_handle_t* server = (httpd_handle_t*)arg;
    if (*server) {
        printf("# Stopping webserver\n");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        }
        else {
            printf("# Failed to stop http server\n");
        }
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
        int32_t event_id, void* event_data) {
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        printf("# Starting webserver\n");
        *server = start_webserver();
    }
}

static void eth_on_got_ip(void *arg, esp_event_base_t event_base,
        int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    printf("Got IPv4 event: Interface \"%s\" address: " IPSTR "\n",
        esp_netif_get_desc(event->esp_netif), IP2STR(&event->ip_info.ip));
}

void ws_srv_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_config.if_desc = "qemu_eth";
    esp_netif_config.route_prio = 64;
    esp_netif_config_t netif_config = {
        .base = &esp_netif_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };
    esp_netif_t *netif = esp_netif_new(&netif_config);
    assert(netif);

    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    mac_config.rx_task_stack_size = 2048;
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = 1;
    phy_config.reset_gpio_num = -1;

    phy_config.autonego_timeout_ms = 100;
    s_mac = esp_eth_mac_new_openeth(&mac_config);
    s_phy = esp_eth_phy_new_dp83848(&phy_config);

    /* Install Ethernet driver */
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(s_mac, s_phy);
    ESP_ERROR_CHECK(esp_eth_driver_install(&config, &s_eth_handle));

    uint8_t eth_mac[6] = {0};
    ESP_ERROR_CHECK(esp_read_mac(eth_mac, ESP_MAC_ETH));
    ESP_ERROR_CHECK(esp_eth_ioctl(s_eth_handle, ETH_CMD_S_MAC_ADDR, eth_mac));

    /* combine driver with netif */
    s_eth_glue = esp_eth_new_netif_glue(s_eth_handle);
    esp_netif_attach(netif, s_eth_glue);

    /* Register user defined event handers */
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &eth_on_got_ip, NULL));

    esp_eth_start(s_eth_handle);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));

    server = start_webserver();
}