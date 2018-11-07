#ifndef __nbiot_H
#define __nbiot_H
#ifdef __cplusplus
 extern "C" {
#endif

#define NB_BUF_MAX_LEN 1024

// ±íÊ¾µ±Ç°Òª¶ÔnbÄ£×é½øÐÐµÄ²Ù×÷
typedef enum
{
  PROCESS_NONE,
  PROCESS_INIT,
  PROCESS_MODULE_INFO,
  PROCESS_SIGN,
  PROCESS_NET_REG,
  PROCESS_UDP_CR, // UDP create
  PROCESS_UDP_CL, // UDP close
  PROCESS_UDP_ST, // UDP send
  PROCESS_UDP_RE, // UDP receive
  PROCESS_COAP_CR, // Coap create
  PROCESS_COAP_ST, // Coap send
  PROCESS_COAP_RE, // Coap receive
  PROCESS_DBG
} Function_State;

// NBÄ£¿é²Ù×÷×Ó×´Ì¬¶¨Òå
// state = PROCESS_INIT
typedef enum
{
  SUB_NONE,
  SUB_SYNC,
  SUB_CMEE,
  SUB_CGSN,
  SUB_CFUN,
  SUB_CIMI,
  SUB_CGATT,
  //SUB_CEREG,
  //SUB_CSCON,
  SUB_CGATT_QUERY,
  SUB_NNMI,
  SUB_NSMI,
  SUB_INIT_END
} Init_SUB_State;

// state = PROCESS_MODULE_INFO
typedef enum
{
  SUB_CGMI,
  SUB_CGMM,
  SUB_CGMR,
  SUB_NBAND,
  SUB_QUERY_INFO_END
} Query_INFO_SUB_State;

// app event
typedef enum
{
  EVENT_INIT_NB,
  EVENT_COAP_CR,
  EVENT_COAP_ST,
  EVENT_COAP_RE,
  EVENT_UDP_CR,
  EVENT_UDP_ST,
  EVENT_UDP_RE,
  EVENT_APP_UNKNOW
} App_Callback_Event;

typedef void (*NB_ReceCB)(App_Callback_Event, uint8_t *, uint16_t, uint8_t);

void NBIoT_Init(NB_ReceCB eventCallback);
uint8_t NB_QueryNetWorkReg(void);
const char* NB_getImsi(void);
uint8_t NB_CreateCoap(void);
uint8_t NB_SendCoapData(uint8_t *pData, uint16_t len);
uint8_t NB_CreateUdp(void);
uint8_t NB_SendUdpData(uint8_t *pData, uint16_t len);
void NB_DbgATCmdSend(uint8_t *pCmd, uint16_t len);

#ifdef __cplusplus
}
#endif
#endif

