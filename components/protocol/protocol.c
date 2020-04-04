// MA DIF protocol handler

// Read incomming messeage from queue and do the work 

// Protocol description 
/* 
  0 IF functions 
    2 select audio source 
  1 Device communication 
  2 GPIO manipolation 
  3 Audio generator 
  4 Telemetri subsystem 
  5 Test function 
*/ 

#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_log.h"
#include "stdint.h"
#include "protocol.h"
#include "MerusAudio.h"
#include "signalgenerator.h"

static const char* TAG = "PROTOHANDLER"; 

extern xQueueHandle prot_queue;

void protocolHandlerTask(void *pvParameter)
{ //ESP_LOGI(TAG,"Protocol handler started");
  printf("prot handler started\n");
  while (1)
  { uint8_t *msg; 
    xQueueReceive(prot_queue, &msg, portMAX_DELAY); 
    //ESP_LOGI(TAG,"Recieved message %d length %d",(msg,msg+1);
    printf("Recieved message %d length %d :",*msg,*(msg+1)); 
    switch (*msg) {
      case 0 :                                     // Interface function
               if (*(msg+2) == 1) 
               { //ESP_LOGI(TAG,"Change Source to : %d\n",(msg+3));
                 printf("Change Source to : %d\n",*(msg+3));
               } 
               break;

      case 1 :                                     // Device communication  // Fuse and otp decoding
               switch (*(msg+2)) { 
                 case 0 :                    //   Byte read 
                    { uint8_t value  = ma_read_byte( *(msg+3), *(msg+4), (*(msg+5)<<8)+*(msg+6) ) ;   
                      printf("Value read : %d\n", value);
                      // return value to sender 
                      break; 
                    }
                 case 1 :                    //   Byte write    
                    ma_write_byte(*(msg+3),*(msg+4),(*(msg+5)<<8)+*(msg+6),*(msg+7) ); 
                    printf("i2c_write : \n");
                    break;
                 case 3 : 
                    {  uint8_t l = (*(msg+1)-7);       
                       uint8_t *writebuf;
                       printf("i2c write block: %d\n",l);
                       writebuf = malloc(l);
                       for (int8_t i = 0; i<l ; i++ )
                         writebuf[i] = *(msg+7+i); 
                       ma_write(*(msg+3),*(msg+4),(*(msg+5)<<8)+*(msg+6),writebuf,l);   
                       break; 
                    }
                  
                 default : break;   
               }
               break; 
      
      
      case 2 : //GPIO 
               break; 
      case 3 : // Signal Generator 
               switch (*(msg+2)) {
                   case 0: // Stop generator
                           signalgenerator_stop(); 
                           break; 
                   case 1: // Start generator 
                           signalgenerator_start();            
                           break;
                   case 2: // Change mode 
                           signalgenerator_mode(*(msg+3));        // cont, sweep, burst
                           break;
                   case 3: signalgenerator_freq(*(msg+3),*(msg+4),*(msg+5));   // freq int >> 4         
                   default : break; 
               } 
               break; 

      case 4 : break; 
      case 5 : //Test function; 
               break;
      default : break;

    } 
    free(msg);
  } 
}

