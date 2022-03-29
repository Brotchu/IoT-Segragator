/* Camera Example

    This example code is in the Public Domain (or CC0 licensed, at your option.)
    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>


#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_camera.h"
#include "lcd.h"
#include "board.h"
#include "esp_http_client.h"

#include "esp_event.h"
#include "esp_log.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "espnow_example.h"

#include "wifi.h"

// #include "lwip/err.h"
// #include "lwip/sys.h"
// #include "esp_http_client.h"
// #include "lwip/sockets.h"
// #include "lwip/netdb.h"
// #include "lwip/dns.h"

// #define SERVER_URL "http://192.168.159.21:10000/"

// #define SERVER_URL "https://xau1p5k525.execute-api.us-east-1.amazonaws.com/test/readings"
#define SERVER_URL "http://ec2-54-175-144-24.compute-1.amazonaws.com/detectimage/request/"
// ec2-34-201-148-90.compute-1.amazonaws.com
// ec2-54-175-144-24.compute-1.amazonaws.com

static const char *TAG = "main";


#define EXAMPLE_ESP_WIFI_SSID      "helmsdeep"
#define EXAMPLE_ESP_WIFI_PASS      "gunpowder"
#define EXAMPLE_ESP_MAXIMUM_RETRY  3

static uint8_t *rgb565;

size_t to_rgb565(const camera_fb_t *src, uint8_t **dst) {
  jpg2rgb565(src->buf, src->len, rgb565, JPG_SCALE_NONE);
  *dst = rgb565;
  return 2 * (src->width * src->height);
}

static lcd_config_t lcd_config = {
    .clk_fre = 40 * 1000 * 1000,
    .pin_clk = LCD_CLK,
    .pin_mosi = LCD_MOSI,
    .pin_dc = LCD_DC,
    .pin_cs = LCD_CS,
    .pin_rst = LCD_RST,
    .pin_bk = LCD_BK,
    .max_buffer_size = 2 * 1024,
    .horizontal = 2, /*!< 2: UP, 3: DOWN */
    .swap_data = 1,
};


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




esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP GET EVENT DATA: %s", (char *)evt->data);
        break;
        //send espnow data
    default:
        break;
    }
    return ESP_OK;
}

static void post_rest_function( char *payload , int len)
{
    esp_http_client_config_t config_post = {
        .url = SERVER_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = client_event_get_handler,
        .auth_type = HTTP_AUTH_TYPE_NONE,
        .transport_type = HTTP_TRANSPORT_OVER_TCP
    };

    esp_http_client_handle_t client = esp_http_client_init(&config_post);

    // char *post_data = "{\"name\" : \"name1\" , \"data\" : \"data1\"}";
    // esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_post_field(client, payload, len);
    esp_http_client_set_header(client, "Content-Type:", "image/jpeg");

    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}

//////ESPNOW
/////////////////////////////////////////////////////////////////////////////////////

static void send_callback(const uint8_t *mac_addr, esp_now_send_status_t status)
{
  if (mac_addr==NULL)
  {
    ESP_LOGE(TAG, "send error");
    return;
  }
  printf("sent?");
}

void espnow_init(void)
{
  esp_now_init();
  esp_now_register_send_cb(send_callback);
  char *test_data = "hello";
  char *peer_address = "44:17:93:5E:2F:6C";
  esp_now_send((uint8_t *)peer_address, (uint8_t *)test_data, (sizeof(test_data)));
  // ESP_LOGI(esp_now_send( (uint8_t *)peer_address, (uint8_t *) test_data, size_t(sizeof(test_data))));
}


void espnow_send()
{
  char *test_data = "hello";
  char *peer_address = "44:17:93:5E:2F:6C";
  esp_now_send((uint8_t *)peer_address, (uint8_t *)test_data, (sizeof(test_data)));
}


/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////




void app_main(){
    ESP_ERROR_CHECK(lcd_init(&lcd_config));
    ESP_ERROR_CHECK(nvs_flash_init());

    wifi_init();

    ///FIXME:
    // espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
    // espnow_config.qsize.data      = 64;
    // ESP_ERROR_CHECK( esp_now_init(espnow_config) );

    ///////////////////////////////////////////////////////////
    //init camera
    camera_init();  
    espnow_init();


    // camera_fb_t *pic = camera_get();
    // camera_fb_t *pic;
    while (true) {
        vTaskDelay(5000 / portTICK_RATE_MS);
        camera_fb_t *pic = esp_camera_fb_get();
        if (!pic) {
            ESP_LOGE(TAG, "get frame failed");
            return;
        }

        ESP_LOGI(TAG, "pic->len: %d", pic->len);
        ESP_LOGI(TAG, "sending post request");
        post_rest_function( (char *)pic->buf, pic->len);
        // printf("sending espnow\n");
        // espnow_send();

        // uint8_t *buf;
        // size_t len = to_rgb565(pic, &buf);

        // lcd_set_index(0, 0, pic->width - 1, pic->height - 1);
        // lcd_write_data(buf, len);

        camera_free(pic);
    }
    
    //show on lcd

}

/* FreeRTOS event group to signal when we are connected*/
// static EventGroupHandle_t s_wifi_event_group;

// /* The event group allows multiple bits for each event, but we only care about two events:
//  * - we are connected to the AP with an IP
//  * - we failed to connect after the maximum amount of retries */
// #define WIFI_CONNECTED_BIT BIT0
// #define WIFI_FAIL_BIT      BIT1

// // static const char *TAG = "wifi station";

// static int s_retry_num = 0;

// static void event_handler(void* arg, esp_event_base_t event_base,
//                                 int32_t event_id, void* event_data)
// {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         esp_wifi_connect();
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
//             esp_wifi_connect();
//             s_retry_num++;
//             ESP_LOGI(TAG, "retry to connect to the AP");
//         } else {
//             xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
//         }
//         ESP_LOGI(TAG,"connect to the AP fail");
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
//         ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
//         s_retry_num = 0;
//         xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
//     }
// }

// void wifi_init_sta(void)
// {
//     s_wifi_event_group = xEventGroupCreate();

//     ESP_ERROR_CHECK(esp_netif_init());

//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();

//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     esp_event_handler_instance_t instance_any_id;
//     esp_event_handler_instance_t instance_got_ip;
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
//                                                         ESP_EVENT_ANY_ID,
//                                                         &event_handler,
//                                                         NULL,
//                                                         &instance_any_id));
//     ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
//                                                         IP_EVENT_STA_GOT_IP,
//                                                         &event_handler,
//                                                         NULL,
//                                                         &instance_got_ip));

//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = EXAMPLE_ESP_WIFI_SSID,
//             .password = EXAMPLE_ESP_WIFI_PASS,
//             /* Setting a password implies station will connect to all security modes including WEP/WPA.
//              * However these modes are deprecated and not advisable to be used. Incase your Access point
//              * doesn't support WPA2, these mode can be enabled by commenting below line */
// 	     .threshold.authmode = WIFI_AUTH_WPA2_PSK,

//             .pmf_cfg = {
//                 .capable = true,
//                 .required = false
//             },
//         },
//     };
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
//     ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
//     ESP_ERROR_CHECK(esp_wifi_start() );

//     ESP_LOGI(TAG, "wifi_init_sta finished.");

//     /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
//      * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
//     EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
//             WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
//             pdFALSE,
//             pdFALSE,
//             portMAX_DELAY);

//     /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
//      * happened. */
//     if (bits & WIFI_CONNECTED_BIT) {
//         ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
//                  EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
//     } else if (bits & WIFI_FAIL_BIT) {
//         ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
//                  EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
//     } else {
//         ESP_LOGE(TAG, "UNEXPECTED EVENT");
//     }

//     /* The event will not be processed after unregister */
//     ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
//     ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
//     vEventGroupDelete(s_wifi_event_group);
// }

// esp_err_t client_event_get_handler(esp_http_client_event_handle_t evt)
// {
//     switch (evt->event_id)
//     {
//     case HTTP_EVENT_ON_DATA:
//         printf("HTTP GET EVENT DATA: %s", (char *)evt->data);
//         break;
    
//     default:
//         break;
//     }
//     return ESP_OK;
// }

// static void post_rest_function(char *post_data)
// {
//     esp_http_client_config_t config_post = {
//         .url = "http://192.168.159.21:10000",
//         .method = HTTP_METHOD_POST,
//         .event_handler = client_event_get_handler
//     };

//     esp_http_client_handle_t client = esp_http_client_init(&config_post);

//     // char *post_data = "{\"name\" : \"name1\" , \"data\" : \"data1\"}";
//     char *upload_data = sprintf("{\"name\" : \"image\", \"image\" : \"%s\"}", post_data);
//     esp_http_client_set_post_field(client, post_data, strlen(post_data));
//     esp_http_client_set_header(client, "Content-Type", "application/json");

//     esp_http_client_perform(client);
//     esp_http_client_cleanup(client);
// }


// void app_main()
// {
//     esp_err_t ret = nvs_flash_init();
//     if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//       ESP_ERROR_CHECK(nvs_flash_erase());
//       ret = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK(ret);

//     ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
//     wifi_init_sta();
//     ESP_LOGI(TAG, "wifi connection complete");
//     lcd_config_t lcd_config = {
//         .clk_fre         = 40 * 1000 * 1000,
//         .pin_clk         = LCD_CLK,
//         .pin_mosi        = LCD_MOSI,
//         .pin_dc          = LCD_DC,
//         .pin_cs          = LCD_CS,
//         .pin_rst         = LCD_RST,
//         .pin_bk          = LCD_BK,
//         .max_buffer_size = 2 * 1024,
//         .horizontal      = 2, /*!< 2: UP, 3: DOWN */
// #ifdef CONFIG_CAMERA_JPEG_MODE
//         .swap_data       = 1,
// #else
//         .swap_data       = 0,
// #endif
//     };

//     lcd_init(&lcd_config);

//     camera_config_t camera_config = {
//         .pin_pwdn = -1,
//         .pin_reset = -1,
//         .pin_xclk = CAM_XCLK,
//         .pin_sscb_sda = CAM_SDA,
//         .pin_sscb_scl = CAM_SCL,

//         .pin_d7 = CAM_D7,
//         .pin_d6 = CAM_D6,
//         .pin_d5 = CAM_D5,
//         .pin_d4 = CAM_D4,
//         .pin_d3 = CAM_D3,
//         .pin_d2 = CAM_D2,
//         .pin_d1 = CAM_D1,
//         .pin_d0 = CAM_D0,
//         .pin_vsync = CAM_VSYNC,
//         .pin_href = CAM_HSYNC,
//         .pin_pclk = CAM_PCLK,

//         //XCLK 20MHz or 10MHz for OV2640 double FPS (Experimental)
//         .xclk_freq_hz = 20000000,
//         .ledc_timer = LEDC_TIMER_0,
//         .ledc_channel = LEDC_CHANNEL_0,
// #ifdef CONFIG_CAMERA_JPEG_MODE
//         .pixel_format = PIXFORMAT_JPEG, //YUV422,GRAYSCALE,RGB565,JPEG
// #else
//         .pixel_format = PIXFORMAT_RGB565,
// #endif
//         .frame_size = FRAMESIZE_QVGA,    //QQVGA-UXGA Do not use sizes above QVGA when not JPEG
//         .jpeg_quality = 12, //0-63 lower number means higher quality
//         .fb_count = 2       //if more than one, i2s runs in continuous mode. Use only with JPEG
//     };
//     //initialize the camera
//     esp_err_t err = esp_camera_init(&camera_config);
//     if (err != ESP_OK) {
//         ESP_LOGE(TAG, "Camera Init Failed");
//         return;
//     }
//     sensor_t *s = esp_camera_sensor_get();
//     s->set_vflip(s, 1);//flip it back
//     //initial sensors are flipped vertically and colors are a bit saturated
//     if (s->id.PID == OV3660_PID) {
//         s->set_brightness(s, 1);//up the blightness just a bit
//         s->set_saturation(s, -2);//lower the saturation
//     }
//     //drop down frame size for higher initial frame rate
//     s->set_framesize(s, FRAMESIZE_QVGA);
//     ESP_LOGI(TAG, "Camera Init done");

// #ifdef CONFIG_CAMERA_JPEG_MODE
//     ESP_LOGI(TAG, "Camera jpeg mode");
//     uint8_t *rgb565 = malloc(240 * 320 * 2);
//     if (NULL == rgb565) {
//         ESP_LOGE(TAG, "can't alloc memory for rgb565 buffer");
//         return;
//     }
// #endif
//     while (1) {
//         camera_fb_t *pic = esp_camera_fb_get();
//         if (pic) {
//             ESP_LOGI(TAG, "picture: %d x %d %dbyte", pic->width, pic->height, pic->len);
//             lcd_set_index(0, 0, pic->width - 1, pic->height - 1);
// #ifdef CONFIG_CAMERA_JPEG_MODE
//             jpg2rgb565(pic->buf, pic->len, rgb565, JPG_SCALE_NONE);
//             lcd_write_data(rgb565, 2 * (pic->width * pic->height));
// #else
//             // lcd_write_data(pic->buf, pic->len);
//             // jpg2rgb565(pic->buf, pic->len, rgb565, JPG_SCALE_NONE);
//             lcd_write_data(pic, 2 * (pic->width * pic->height));
// #endif
//             esp_camera_fb_return(pic);

//             //http post request TODO:
            
//             // char *payload = "{\"name\" : \"image\", \"data\": \"\"}";
//             // post_rest_function((char *)pic);
//             //////////////////
//         } else {
//             ESP_LOGE(TAG, "Get frame failed");
//         }
//     }

// #ifdef CONFIG_CAMERA_JPEG_MODE
//     free(rgb565);
// #endif
// }
