#ifndef MAIN_WEBSOCKET_TASK_H_
#define MAIN_WEBSOCKET_TASK_H_

#include "lwip/api.h"

err_t WS_write_data(char* p_data, size_t length);

void ws_server(void *pvParameters);


#endif /* MAIN_WEBSOCKET_TASK_H_ */
