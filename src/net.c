#include "net.h"
#include "comm.h"

static const char* TAG = "net";

static httpd_handle_t server = NULL;
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGE(TAG,"Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void net_wifi_init() {
    // Initialize wifi peripheral
    ESP_LOGI(TAG, "Initializing WiFi");
    s_wifi_event_group = xEventGroupCreate();
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid     = COMM_WIFI_SSID,
            .password = COMM_WIFI_PASSWD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialized");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to AP (SSID: %s password: %s)", COMM_WIFI_SSID, COMM_WIFI_PASSWD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "Failed to connect to AP (SSID: %s, password: %s)", COMM_WIFI_SSID, COMM_WIFI_PASSWD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

// -------------------
// ---- Websocket ----
// -------------------

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }

        ws_pkt.payload = buf;

        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }

        comm_parse_packet(buf, ws_pkt.len);
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

static void net_broadcast_ws_msg_queue_work(void *arg) {
    net_async_resp_arg* resp_arg = (net_async_resp_arg*) arg;
    httpd_ws_frame_t ws_pkt;

    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;

    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = resp_arg->msg;
    ws_pkt.len = resp_arg->len;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);

    free(ws_pkt.payload);
    free(resp_arg);
}

void net_broadcast_ws_msg(uint8_t* buf, size_t len) {
    // Send async message to all connected clients that use websocket protocol every 10 seconds
    size_t clients = NET_MAX_CLIENTS;
    int    client_fds[NET_MAX_CLIENTS];

    if (httpd_get_client_list(server, &clients, client_fds) == ESP_OK) {
        for (size_t i=0; i < clients; ++i) {
            int sock = client_fds[i];

            if (httpd_ws_get_fd_info(server, sock) == HTTPD_WS_CLIENT_WEBSOCKET) {
                ESP_LOGI(TAG, "Active client (fd=%d) -> sending async message", sock);
                ESP_LOGI(TAG, "%s;%d", buf, len);

                net_async_resp_arg* resp_arg = malloc(sizeof(net_async_resp_arg));
                resp_arg->hd = server;
                resp_arg->fd = sock;
                resp_arg->len = len;

                resp_arg->msg = malloc(len);
                memcpy(resp_arg->msg, buf, len);

                if (httpd_queue_work(resp_arg->hd, net_broadcast_ws_msg_queue_work, resp_arg) != ESP_OK)
                    ESP_LOGE(TAG, "httpd_queue_work failed!");
            }
        }
    } else {
        ESP_LOGE(TAG, "httpd_get_client_list failed!");
    }

    free(buf);
}

static void net_server_init() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //config.max_open_sockets = NET_MAX_CLIENTS;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    ESP_ERROR_CHECK(httpd_start(&server, &config));
    
    // Registering the ws handler
    ESP_LOGI(TAG, "Registering URI handlers");
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &ws));
}

void net_init() {
    net_wifi_init();
    net_server_init();
}