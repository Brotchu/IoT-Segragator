/* Camera Example

    This example code is in the Public Domain (or CC0 licensed, at your option.)
    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "board.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <sys/param.h>
#include <esp_http_server.h>

#include "wifi.h"

static const char *TAG = "main";

#define SERVO_URL "http://192.168.86.157/echo"

#define MOTOR_URL "http://192.168.131.49/turn_on"

#define IMAGE_URL "http://192.168.86.21:10000/"

#define SERVER_URL "http://ec2-54-91-98-238.compute-1.amazonaws.com:80/detectimage/request/"

#define EXAMPLE_ESP_WIFI_SSID      "helmsdeep"
#define EXAMPLE_ESP_WIFI_PASS      "gunpowder"
#define EXAMPLE_ESP_MAXIMUM_RETRY  3

static uint8_t *rgb565;

static camera_config_t camera_config = {
    .pin_pwdn = -1,
    .pin_reset = -1,
    .pin_xclk = CAM_XCLK,
    .pin_sscb_sda = CAM_SDA,
    .pin_sscb_scl = CAM_SCL,

    .pin_d7 = CAM_D7,
    .pin_d6 = CAM_D6,
    .pin_d5 = CAM_D5,
    .pin_d4 = CAM_D4,
    .pin_d3 = CAM_D3,
    .pin_d2 = CAM_D2,
    .pin_d1 = CAM_D1,
    .pin_d0 = CAM_D0,
    .pin_vsync = CAM_VSYNC,
    .pin_href = CAM_HSYNC,
    .pin_pclk = CAM_PCLK,

    // XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,

    // 0-63 lower number means higher quality
    .jpeg_quality = 12,
    .fb_count = 2,
};


void camera_init(void) {
  ESP_ERROR_CHECK(esp_camera_init(&camera_config));

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1);
  if (s->id.PID == OV3660_PID) {
    s->set_brightness(s, 1);
    s->set_saturation(s, -2);
  }

  s->set_framesize(s, FRAMESIZE_QVGA);
  ESP_LOGI(TAG, "Camera Init done");

  ESP_LOGI(TAG, "Camera jpeg mode");
  rgb565 = malloc(240 * 320 * 2);
  if (rgb565 == NULL) {
    ESP_LOGE(TAG, "can't alloc memory for rgb565 buffer");
    return;
  }
}

void camera_free(camera_fb_t *ptr) {
  if (ptr) {
    esp_camera_fb_return(ptr);
  }
}


esp_err_t servo_event_get_handler(esp_http_client_event_handle_t evt)
{
    return ESP_OK;
}

const esp_http_client_config_t motor_post_config = {
    .url = MOTOR_URL,
    .method = HTTP_METHOD_POST,
    .event_handler = servo_event_get_handler,
    .auth_type = HTTP_AUTH_TYPE_NONE,
    .transport_type = HTTP_TRANSPORT_OVER_TCP
};

const esp_http_client_config_t servo_config_post = {
        .url = SERVO_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = servo_event_get_handler,
        // .cert_pem = cert
        .auth_type = HTTP_AUTH_TYPE_NONE,
        .transport_type = HTTP_TRANSPORT_OVER_TCP
};

const esp_http_client_config_t image_config_post = {
        .url = IMAGE_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = servo_event_get_handler,
        // .cert_pem = cert
        .auth_type = HTTP_AUTH_TYPE_NONE,
        .transport_type = HTTP_TRANSPORT_OVER_TCP
};

static void local_image_post(char *payload, int len, const char* url)
{
    esp_http_client_handle_t client = esp_http_client_init(&image_config_post);

    esp_http_client_set_post_field(client, payload, len);
    esp_http_client_set_header(client, "Content-Type:", "image/jpeg");

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

static void post_servo_request(char *payload, int len, const char* url)
{
    esp_http_client_handle_t client = esp_http_client_init(&servo_config_post);

    esp_http_client_set_post_field(client, payload, len);

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP GET EVENT DATA: %s\n", (char *)evt->data);
        if( *(char *)evt->data == '1' ){
          ESP_LOGI(TAG, "Detected red, sending servo request 11");
          post_servo_request( (char *)"11", sizeof((char *)"11"), SERVO_URL);    
        } else if ( *(char *)evt->data == '2' ){
          ESP_LOGI(TAG, "Detected green, sending servo request 12");
          post_servo_request( (char *)"12", sizeof((char *)"12"), SERVO_URL);
        } else if ( *(char *)evt->data == '3' ){
            ESP_LOGI(TAG, "Detected blue, sending servo request 21");
            post_servo_request( (char *)"21", sizeof((char *)"21"), SERVO_URL);
        } else if ( *(char *)evt->data == '4' ){
            ESP_LOGI(TAG, "Detected NA, sending servo request 22");
            post_servo_request( (char *)"22", sizeof((char *)"22"), SERVO_URL);
        }else {
          ESP_LOGI(TAG, "Detected NA, sending servo request 22");
        }
        // post_motor_request( (char *)"1", sizeof( (char *)"1"), MOTOR_URL);
        break;
        //send espnow data
    default:
        printf("not HTTP_EVENT_ON_DATA");
        break;
    }
    return ESP_OK;
}

const esp_http_client_config_t server_config_post = {
        .url = SERVER_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = client_event_get_handler,
        // .cert_pem = cert
        .auth_type = HTTP_AUTH_TYPE_NONE,
        .transport_type = HTTP_TRANSPORT_OVER_TCP
    };

static void post_rest_function( char *payload , int len, const char* url)
{
    esp_http_client_handle_t client = esp_http_client_init(&server_config_post);

    esp_http_client_set_post_field(client, payload, len);
    esp_http_client_set_header(client, "Content-Type:", "image/jpeg");

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}


//SERVER CODE /......................................................................................................
/* An HTTP GET handler */
static esp_err_t hello_get_handler(httpd_req_t *req)
{
    char*  buf;
    size_t buf_len;

    /* Get header value string length and allocate memory for length + 1,
     * extra byte for null termination */
    buf_len = httpd_req_get_hdr_value_len(req, "Host") + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        /* Copy null terminated value string into buffer */
        if (httpd_req_get_hdr_value_str(req, "Host", buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "Found header => Host: %s", buf);
        }
        free(buf);
    }

    camera_fb_t *pic = esp_camera_fb_get();
    if (!pic) {
        ESP_LOGE(TAG, "get frame failed");
        return;
    }
    ESP_LOGI(TAG, "pic->len: %d", pic->len);
    ESP_LOGI(TAG, "sending post request");
    post_rest_function( (char *)pic->buf, pic->len, SERVER_URL);
    local_image_post( (char *)pic->buf, pic->len, SERVER_URL);
    camera_free(pic);

    
    const char* resp_str = (const char*) req->user_ctx;
    httpd_resp_send(req, resp_str, HTTPD_RESP_USE_STRLEN);

    /* After sending the HTTP response the old HTTP request
     * headers are lost. Check if HTTP request headers can be read now. */
    if (httpd_req_get_hdr_value_len(req, "Host") == 0) {
        ESP_LOGI(TAG, "Request headers lost");
    }
    return ESP_OK;
}

static const httpd_uri_t hello = {
    .uri       = "/hello",
    .method    = HTTP_GET,
    .handler   = hello_get_handler,
    /* Let's pass response string in user
     * context to demonstrate it's usage */
    .user_ctx  = "Hello World!"
};

/* An HTTP POST handler */
static esp_err_t echo_post_handler(httpd_req_t *req)
{
    camera_init();  
    camera_fb_t *pic = esp_camera_fb_get();
    if (!pic) {
        ESP_LOGE(TAG, "get frame failed");
        return;
    }
    ESP_LOGI(TAG, "pic->len: %d", pic->len);
    ESP_LOGI(TAG, "sending post request");
    post_rest_function( (char *)pic->buf, pic->len, SERVER_URL);
    local_image_post( (char *)pic->buf, pic->len, IMAGE_URL);
    camera_free(pic);

    esp_camera_deinit();

    // End response
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static const httpd_uri_t echo = {
    .uri       = "/echo",
    .method    = HTTP_POST,
    .handler   = echo_post_handler,
    .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &echo);
        // httpd_register_uri_handler(server, &ctrl);
        #if CONFIG_EXAMPLE_BASIC_AUTH
        httpd_register_basic_auth(server);
        #endif
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

//....................................................................................................................








void app_main(){
    // ESP_ERROR_CHECK(lcd_init(&lcd_config));
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();

    ///////////////////////////////////////////////////////////
    static httpd_handle_t server = NULL;

    server = start_webserver();


}
