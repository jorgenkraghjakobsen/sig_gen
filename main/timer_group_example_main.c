/* Timer group-hardware timer example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include "esp_types.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "driver/periph_ctrl.h"
#include "MerusAudio.h"
#include "protocol.h"
#include "signalgenerator.h"
#include "wifi_manager.h"
#include "WebSocket_Task.h"
#include "mdns.h"
#include "rtprx.h"

static const char TAG[] = "main";

xQueueHandle prot_queue;
xQueueHandle i2s_queue;

void monitoring_task(void *pvParameter)
{
	for(;;){
		ESP_LOGI(TAG, "free heap: %d",esp_get_free_heap_size());
		vTaskDelay( pdMS_TO_TICKS(10000) );
	}
}
void cb_connection_ok(void *pvParameter){
	ESP_LOGI(TAG, "I have a connection!");
}



void app_main()
{  
    setup_ma120();
    ma120_setup_audio(0x20);
    
    wifi_manager_start();

	  /* register a callback as an example to how you can integrate your code with the wifi manager */
	  wifi_manager_set_callback(EVENT_STA_GOT_IP, &cb_connection_ok);
    
    vTaskDelay( pdMS_TO_TICKS(5000) );
  	ESP_LOGI(TAG, "Setup ws server");
	  xTaskCreatePinnedToCore(&ws_server, "ws_server", 2*2048, NULL, 20, NULL,0);

    prot_queue = xQueueCreate(10, sizeof(uint8_t *) );
    xTaskCreate(protocolHandlerTask, "prot_handler_task", 2*1024, NULL, 5, NULL);
    
    // start rtp rx 
    vTaskDelay(4000/portTICK_PERIOD_MS);
    xTaskCreate(rtp_rx_task, "rtp_rx_task", 2*1024, NULL, 5, NULL);
    
    uint32_t cnt = 0; 
    while (1)
    {  ma120_read_error(0x20); 
       vTaskDelay(2000/portTICK_PERIOD_MS);
       if (cnt%10==11)
       { signalgenerator_start();
         printf("Signalgenerator start\n");
       }
       if (cnt%10==11) 
       { uint8_t msg[] = { 0x01,0x07,0x01,0x20,2,0x00,0x03,0x30}; 
         uint8_t (*mymsg)[] = malloc(10);
         memcpy(mymsg,msg,8);
         xQueueSendToBack(prot_queue,&mymsg,portMAX_DELAY);
       }  
       if (cnt%10==11) 
       { uint8_t msg[] = { 0x01,0x07,0x01,0x20,2,0x00,0x04,0x30}; 
         uint8_t (*mymsg)[] = malloc(10);
         memcpy(mymsg,msg,8);
         xQueueSendToBack(prot_queue,&mymsg,portMAX_DELAY);
       }  
       if (cnt%10==11) 
       { uint8_t msg[] = { 0x01,0x07,0x00,0x20,2,0x00,0x03}; 
         uint8_t (*mymsg)[] = malloc(10);
         memcpy(mymsg,msg,7);
         xQueueSendToBack(prot_queue,&mymsg,portMAX_DELAY);
       }  
       if (cnt%10==11) 
       { signalgenerator_stop();
         printf("Signalgenerator stop\n");
       } 
       cnt++;
    }
}

