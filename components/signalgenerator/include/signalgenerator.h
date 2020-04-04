#ifndef _SIGNALGENERATOR_H_
#define _SIGNALGENERATOR_H_

void signalgenerator_start(void);
void signalgenerator_stop(void);
void signalgenerator_mode(uint8_t newmode);                       // Cont, sweep, burst 
void signalgenerator_freq(uint8_t f2,uint8_t f1, uint8_t f0);     // (f2,f1,f0)>>4 
void generator_task(void *arg);
void setup_i2s(void);
#endif
