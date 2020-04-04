//
// Signal generator 
// 
// Component from MADIF 
// Written by Jørgen Kragh Jakobsen, IFX - October 2019
//
//  Support 
//    Sine, triangle, square 
//    Sweep and burst 
//    DC offset  
// 
//  Interface for enable, diable and generator parameters
//

#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/i2s.h"
#include "math.h"
#include "signalgenerator.h"

xQueueHandle i2s_queue;
uint32_t sample_rate = 48000;
TaskHandle_t xTaskSignal; 

uint8_t mode ;  
double freq ;
uint32_t sweepCnt;   
uint gate; 
double angleSpeed ;    
double sampleRate = 48000.0;
   
static bool signalState = 0;
//static bool i2sState = 0;
void signalgenerator_start() { 
  if (signalState == 0 )  
  { //if (i2sState == 0) { 
      setup_i2s();
    //  i2sState = 1; 
    //}
    //else 
    //{ 
     // i2s_start(0);
      //i2s_zero_dma_buffer(0);
    //}

    xTaskCreate(generator_task, "signalgenerator", 6*1024, NULL, 5, &xTaskSignal);
    signalState = 1;
  }
}

void signalgenerator_stop() {
  if (signalState == 1)
  { //i2s_zero_dma_buffer(0);
  
    //i2s_stop(0); 
    i2s_driver_uninstall(0);
    vTaskDelete(xTaskSignal);
    signalState = 0;
  }
}    

void signalgenerator_mode(uint8_t newmode){
  if (mode != newmode ) 
  { if (newmode == 1) 
    { gate = 1;
      sweepCnt = 0;
    }   
    if (newmode == 0)
    { freq = 1000.0;
      angleSpeed  = 2*M_PI*freq/sampleRate;
    }
    mode = newmode;  
  }
}      
void signalgenerator_freq(uint8_t f2, uint8_t f1, uint8_t f0) {
  if (mode == 0)
  { 
    freq = (double)((f2<<12) + (f1<<4) + (f0>>4));
    printf("Freq : %f\n",freq);
    angleSpeed  = 2*M_PI*freq/sampleRate;
  }
}


void generator_task(void *arg)
{   uint32_t bufferRate  = 200;   
    uint32_t lengthSig   = sampleRate/bufferRate; 
    uint32_t byteWritten = 0; 
    uint32_t timer_cnt   = 0;
    uint8_t sample_buf[240*4*2];
    double sigbuf[240]; 
    double amp   = 0.5;
    double phase = 0;
    double sweepSpeed = 1.0125;
    double sweepStartFreq  = 50.0;  
    double sweepStopFreq   = 20000;
    double sweepPeriod     = 4;        // in sec  
    uint32_t i = 0;    
    i2s_event_t i2s_evt;
    sweepCnt = 0;
    freq = 50.0;   
    angleSpeed  = 2*M_PI*freq/sampleRate;
    
    while (1) {
        xQueueReceive(i2s_queue, &i2s_evt, portMAX_DELAY);
        if (i2s_evt.type == I2S_EVENT_TX_DONE) 
        { timer_cnt++;
          switch (mode) {
            case 0 : gate = 1;              // Continues 
                     break;
            case 1 :                  // Sweep                       
                     if (sweepCnt == 0 ) {
                       gate = 1;
                       freq = sweepStartFreq; 
                     } else 
                     { freq = freq * sweepSpeed;
                       angleSpeed = 2*M_PI*freq/sampleRate;
                     }
                     if ( freq > sweepStopFreq )
                     { gate = 0;
                       //printf("sweepCnt : %d",sweepCnt);
                       sweepCnt = ( sweepCnt > sweepPeriod*lengthSig) ? 0 : sweepCnt + 1; 
                     } else 
                     { sweepCnt++; 
                     }
                     break;
            case 2 : gate = 1;             // Burst             
                     break;
          }
          for ( i = 0; i < lengthSig; i++)
          {  sigbuf[i] = amp*sinf((double) (i*angleSpeed) + phase );
          }
          // How many period round the circle did se do and where on the circle did we end
          double phase_in  = (double) (i*angleSpeed) + phase;
          // Save the end location on the circle as new starting point 
          phase            = phase_in - (double) 2.0*M_PI * (int) (phase_in/(2*M_PI)); 

          //if (timer_cnt < 10) 
          //{  printf("out: %d %f %f   \n",i, phase_in, phase); }

          for (int i = 0; i < lengthSig; i++)
          {  int16_t sample_int = gate*sigbuf[i]*0x7FFF;   
             sample_buf[i*8 + 0 ] = 0;
             sample_buf[i*8 + 1 ] = 0;
             sample_buf[i*8 + 2 ] = sample_int & 0x00FF;
             sample_buf[i*8 + 3 ] = (sample_int & 0xFF00)>>8;
             sample_buf[i*8 + 4 ] = 0;
             sample_buf[i*8 + 5 ] = 0;
             sample_buf[i*8 + 6 ] = sample_int & 0x00FF;
             sample_buf[i*8 + 7 ] = (sample_int & 0xFF00)>>8;
          }
          i2s_write(0, sample_buf, lengthSig*4*2, &byteWritten, 5);//portMAX_DELAY );

          //if (timer_cnt%200==0) 
          //{ printf("%d %d \n",byteWritten, i2s_evt.size );
          //}    
        }
        else
        {
           printf("Other interupt on i2s queue \n");
        }
    }
}


void setup_i2s()
{
  i2s_config_t i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,                                  // Only TX
    .sample_rate = sample_rate,
    .bits_per_sample = 32,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
    .communication_format = I2S_COMM_FORMAT_I2S_MSB , I2S_COMM_FORMAT_I2S_MSB,
    .dma_buf_count = 2,
    .dma_buf_len = 240,
   // .intr_alloc_flags = 0,                                                  //Default interrupt priority
    .use_apll = true,
    .fixed_mclk = 0,
    .tx_desc_auto_clear = 0                                                 //Auto clear tx descriptor on underflow
  };
  
  i2s_driver_install(0, &i2s_config, 10, &i2s_queue);
  i2s_zero_dma_buffer(0);

   i2s_pin_config_t pin_config = {
    .bck_io_num = CONFIG_EXAMPLE_I2S_BCK_PIN,
    .ws_io_num = CONFIG_EXAMPLE_I2S_LRCK_PIN,
    .data_out_num = CONFIG_EXAMPLE_I2S_DATA_PIN,
    .data_in_num = -1                                                       //Not used
  };

  i2s_set_pin(0, &pin_config);  

  //i2s_set_clk(0, sample_rate, 32, 2);
}

