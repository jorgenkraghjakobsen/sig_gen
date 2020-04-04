/**
 * \file    rtprx.c   
 * \author  Jorgen Kragh Jakobsen
 * \date    19.10.2019
 * \version   0.1
 *
 * \brief RTP audio stream reciever 
 *
 * \warning This software is a PROTOTYPE version and is not designed or intended for use in production, especially not for safety-critical applications!
 * The user represents and warrants that it will NOT use or redistribute the Software for such purposes.
 * This prototype is for research purposes only. This software is provided "AS IS," without a warranty of any kind.
 */
#include "rtprx.h"

#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "tcpip_adapter.h"

#include "lwip/api.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>


void rtp_rx_task(void *pvParameters){
  static struct netconn *conn;
  static struct netbuf *buf; 
  static uint32_t pkg = 0 ;
  err_t err, recv_err;
  
  conn = netconn_new(NETCONN_UDP);
  if (conn!=NULL)
  { printf("Net RTP RX\n");
    netconn_bind(conn, IP_ADDR_ANY, 1350);
    netconn_listen(conn);
    while (1) { 
      recv_err = netconn_recv(conn,&buf);
      pkg++;
      if ((pkg%100)==0) 
      { printf("%d >",pkg) ;
      }
      
    }
  }
  netconn_close(conn);
  netconn_delete(conn);
}
