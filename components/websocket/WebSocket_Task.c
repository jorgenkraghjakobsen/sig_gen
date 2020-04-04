/**
 * \file   WebSocket_Task.c
 * \author   Thomas Barth
 * \date    16.01.2017
 * \version   0.1
 *
 * \brief A very basic and hacky WebSocket-Server
 *
 * \warning This software is a PROTOTYPE version and is not designed or intended for use in production, especially not for safety-critical applications!
 * The user represents and warrants that it will NOT use or redistribute the Software for such purposes.
 * This prototype is for research purposes only. This software is provided "AS IS," without a warranty of any kind.
 */

#include "WebSocket_Task.h"

#include "freertos/FreeRTOS.h"
#include "esp_heap_caps.h" 
#include "esp32/sha.h"
#include "mbedtls/base64.h"
#include "esp_system.h"
#include "mbedtls/sha1.h"

//#include "MerusAudio.h"
//#include "esp32_dsp.h"
#include "protocol.h"
extern xQueueHandle prot_queue;
#include <string.h>


#define WS_PORT            8000      //TCP Port for the Server
#define WS_CLIENT_KEY_L    24      //Length of the Client Key
#define SHA1_RES_L         20      //SHA1 result
#define WS_MASK_L          0x4      //Length of MASK field in WebSocket HEader
#define WS_STD_LEN         125      //Maximum Length of standard length frames
#define WS_SPRINTF_ARG_L   4      //Length of sprintf argument for string (%.*s)

//Opcode according to RFC 6455
typedef enum{
   WS_OP_CON=0x0,               //Continuation Frame
   WS_OP_TXT=0x1,               //Text Frame
   WS_OP_BIN=0x2,               //Binary Frame
   WS_OP_CLS=0x8,               //Connection Close Frame
   WS_OP_PIN=0x9,               //Ping Frame
   WS_OP_PON=0xa               //Pong Frame
}WS_OPCODES;

typedef struct{
   uint8_t      opcode:WS_MASK_L;
   uint8_t      reserved:3;
   uint8_t      FIN:1;
   uint8_t      payload_length:7;
   uint8_t      mask:1;
}WS_frame_header_t ;

//stores open WebSocket connections
static struct netconn* WS_conn=NULL;

const char WS_sec_WS_keys[]="Sec-WebSocket-Key:";
const char WS_sec_conKey[]="258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
const char WS_srv_hs[] ="HTTP/1.1 101 Switching Protocols \r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %.*s\r\n\r\n";

err_t WS_write_data(char* p_data, size_t length){

   //check if we have an open connection
   if(WS_conn==NULL)
      return ERR_CONN;

   //currently only frames with a payload length <WS_STD_LEN are supported
   if(length>WS_STD_LEN)
      return ERR_VAL;

   //netconn_write result buffer
   err_t   result;

   //prepare header
   WS_frame_header_t hdr;
   hdr.FIN=0x1;
   hdr.payload_length=length;
   hdr.mask=0;
   hdr.reserved=0;
   hdr.opcode=WS_OP_BIN;   // was TXT

   //send header
   result=netconn_write(WS_conn, &hdr, sizeof(WS_frame_header_t), NETCONN_COPY);

   //check if header had been send
   if(result!=ERR_OK)
      return result;

   //send payload
   return netconn_write(WS_conn, p_data, length, NETCONN_COPY);
}

static void ws_server_netconn_serve(struct netconn *conn) {

   //Netbuf
   struct netbuf *inbuf;
   inbuf = malloc(sizeof(256));

   //message buffer
   char* buf;
   buf = malloc(128);
   //pointer to buffer (multi purpose)
   char* p_buf;
   buf = malloc(128);
   
   //Pointer to SHA1 input
   char* p_SHA1_Inp;

   //Pointer to SHA1 result
   char* p_SHA1_result;
   p_SHA1_result = malloc(40);

   //multi purpose number buffer
   uint16_t i;

   //will point to payload (send and receive
   char* p_payload;
   p_payload = malloc(256);

   //Frame header pointer
   WS_frame_header_t* p_frame_hdr;

   //allocate memory for SHA1 input
   p_SHA1_Inp=malloc(WS_CLIENT_KEY_L+sizeof(WS_sec_conKey)); //,MALLOC_CAP_8BIT);
   /*if (p_SHA1_Inp == NULL) 
   { printf("MALLoc error \n"); 
     vTaskDelay( pdMS_TO_TICKS(10000) );
     printf("MALLoc error \n"); 
   } */


   //allocate memory for SHA1 result
   /*p_SHA1_result=malloc(SHA1_RES_L); //,MALLOC_CAP_8BIT);
   if (p_SHA1_result == NULL) 
   { printf("MALLoc error \n"); 
     vTaskDelay( pdMS_TO_TICKS(10000) );
     printf("MALLoc error \n"); 
   }*/

   //Check if malloc suceeded
   if((p_SHA1_Inp!=NULL)&&(p_SHA1_result!=NULL)){
      //receive handshake request
      if(netconn_recv(conn, &inbuf)==ERR_OK)   {
         //printf("recv buff\n");
         //read buffer
         netbuf_data(inbuf, (void**) &buf, &i);
         //printf("recv data : %s\n",buf);
         //write static key into SHA1 Input
         for(i=0;i<sizeof(WS_sec_conKey);i++)
            p_SHA1_Inp[i+WS_CLIENT_KEY_L]=WS_sec_conKey[i];

         //find Client Sec-WebSocket-Key:
         p_buf=strstr(buf, WS_sec_WS_keys);

         //check if needle "Sec-WebSocket-Key:" was found
         if(p_buf!=NULL){
             
            //get Client Key
            for(i=0;i<WS_CLIENT_KEY_L;i++)
               p_SHA1_Inp[i]=*(p_buf+sizeof(WS_sec_WS_keys)+i);

            //printf("Key ready for hash : %s\n",p_SHA1_Inp); 
            //printf("i                  : %d\n",i);
            //printf("lenght             : %d\n",strlen(p_SHA1_Inp));
             
            
            mbedtls_sha1_ret((unsigned char*)p_SHA1_Inp,strlen(p_SHA1_Inp),(unsigned char*)p_SHA1_result);
            //esp_sha(SHA1,(unsigned char*)p_SHA1_Inp,strlen(p_SHA1_Inp),(unsigned char*)p_SHA1_result);
            //for (int j = 0; j<20;j++)
            //  printf("%02x ",*(p_SHA1_result+j));
            //printf("\n");
            size_t olen = 0;
            //p_buf = malloc(40);
            //hex to base64
            int res = mbedtls_base64_encode((unsigned char *)p_buf,40,&olen,(unsigned char*)p_SHA1_result, 20 ); 
			   //p_buf = (char*) base64_encode((unsigned char*)p_SHA1_result, SHA1_RES_L,(size_t*) &i);
            //printf("base64 encode done (%d) (%d) : %s\n",res,olen,p_buf);   
            
            //free SHA1 input
            free(p_SHA1_Inp);

            //free SHA1 result
            free(p_SHA1_result);

            //allocate memory for handshake
            p_payload = malloc(sizeof(WS_srv_hs)+i-WS_SPRINTF_ARG_L); 
              
            //check if malloc suceeded
            if(p_payload!=NULL){

               //printf("WS_srv_hs:\n%s\n",WS_srv_hs);
               //(printf("i:%d %s",i,p_buf); 
               //prepare handshake
               printf("Prep haand shake %d \n",olen);
               sprintf(p_payload,WS_srv_hs, olen , p_buf);
               //printf("handshake : %s\n",p_payload); 
               //send handshake
               
               int netres = netconn_write(conn, p_payload, strlen(p_payload),   NETCONN_COPY);
               printf("write netconn done %d\n",netres);
               
               //free base 64 encoded sec key
               //free(p_buf);

               //free handshake memory
               free(p_payload);

               //set pointer to open WebSocket connection
               WS_conn=conn;

               //Wait for new data
               printf("will enter loop  \n");
               
               while(netconn_recv(conn, &inbuf)==ERR_OK){
                  printf("in ws loop...\n");  
                  //read data from inbuf
                  netbuf_data(inbuf, (void**) &buf, &i);

                  //get pointer to header
                  p_frame_hdr=(WS_frame_header_t*)buf;

                  //check if clients wants to close the connection
                  if(p_frame_hdr->opcode==WS_OP_CLS)
                     break;

                  //get payload length
                  if(p_frame_hdr->payload_length<=WS_STD_LEN){

                     //get beginning of mask or payload
                     p_buf=(char*)&buf[sizeof(WS_frame_header_t)];

                     //check if content is masked
                     if(p_frame_hdr->mask){

                        //allocate memory for decoded message
                        p_payload = malloc(p_frame_hdr->payload_length); 
                        //check if malloc succeeded
                        if (p_payload==NULL) { 
                           printf("Malloc error, %d \n", p_frame_hdr->payload_length);
                           vTaskDelay(pdMS_TO_TICKS(10000));  
                           printf("Malloc error, %d \n", p_frame_hdr->payload_length);
                        }   
                      //decode playload
                        for (i = 0; i < p_frame_hdr->payload_length; i++)
                              p_payload[i] = (p_buf+WS_MASK_L)[i] ^ p_buf[i % WS_MASK_L];
                  
                     }
                     else
                        //content is not masked
                        p_payload=p_buf;

                  printf("Do stuff\n");
                  //do stuff
                  if((p_payload!=NULL)&&(p_frame_hdr->opcode==WS_OP_BIN))
                  { printf("WS bin type");
		              uint8_t (*msg)[] = malloc(32);
                    memcpy(msg,p_payload,p_payload[1]);
                    xQueueSendToBack(prot_queue,&msg,portMAX_DELAY);
                    printf("send msg to queue l: %d\n ",p_payload[1]);
                  }
					   if((p_payload!=NULL)&&(p_frame_hdr->opcode==WS_OP_TXT))
					   {  printf("WS txt type ");
                     printf("scom_w(%d,%d)\n",p_payload[0],p_payload[1]);
					      esp_err_t err = 0 ; //ma_write_byte(p_payload[0],p_payload[1]);
					      if (err == ESP_FAIL )
						   { printf("Fail capture \n");
						    
					    	}
					   }
					   //free payload buffer
                  if(p_frame_hdr->mask&&p_payload!=NULL)
                        free(p_payload);

                  }//p_frame_hdr->payload_length<126

              
                                  
                  //free input buffer
                  netbuf_delete(inbuf);
                  printf("Wait for next msg\n");
                  
               }//while(netconn_recv(conn, &inbuf)==ERR_OK)
            }//p_payload!=NULL
         }//check if needle "Sec-WebSocket-Key:" was found
         else 
           printf("No sec key found\n");
      }//receive handshake
   }//p_SHA1_Inp!=NULL&p_SHA1_result!=NULL

   //release pointer to open WebSocket connection
   WS_conn=NULL;

   //delete buffer
   netbuf_delete(inbuf);

   // Close the connection
   netconn_close(conn);

   //Delete connection
   netconn_delete(conn);

}

void ws_server(void *pvParameters){
  struct netconn *conn, *newconn;
  conn = netconn_new(NETCONN_TCP);
  printf("Netcombind\n");
  netconn_bind(conn, IP_ADDR_ANY, WS_PORT);
  netconn_listen(conn);
  while(netconn_accept(conn, &newconn)==ERR_OK)
  {   ws_server_netconn_serve(newconn);
      printf(".");

  }
  netconn_close(conn);
  netconn_delete(conn);
}
