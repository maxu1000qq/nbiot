#include <string.h>
#include "includes.h"
#include "tdtech.h"
#include "atcommand.h"
#include "nbiot.h"
#include "Dma_buffer.h"
#include "nbtimer.h"
#include "myjob/nanrui/nanrui.h"

#define REMOTE_SERVER_IP "47.95.197.61" // aliyun
#define REMOTE_SERVER_PORT "6000"
#define LOCAL_UDP_SET "DGRAM,17,10000,1"

#define REMOTE_COAP_INFO "115.29.240.46,5683"

#define NBIOT_RCV_TASK_STK_SIZE 512u
OS_TCB NBIoTRcvTaskTCB;
CPU_STK NBIoTRcvTaskStk[NBIOT_RCV_TASK_STK_SIZE];

#define NBIOT_SND_TASK_STK_SIZE 512u
OS_TCB NBIoTSndTaskTCB;
CPU_STK NBIoTSndTaskStk[NBIOT_SND_TASK_STK_SIZE];

#define NET_TASK_STK_SIZE 512u
OS_TCB NETTaskProcTCB;
CPU_STK NETTaskProcStk[NET_TASK_STK_SIZE];

// NBiot ¹¦ÄÜ×´Ì¬
typedef struct NB_Function_Stru
{
  Function_State state;
  int sub_state;
} nb_function_t;
nb_function_t g_nb_func_state;

#define COMMON_CMD_LENGTH 100

// 1cmd ÊôÐÔ
typedef enum NB_Cmd_Property_enum
{
	CMD_TEST, // ÃüÁîTEST²Ù×÷
	CMD_READ, // ÃüÁîREAD²Ù×÷
	CMD_SET, // ÃüÁîSET²Ù×÷
	CMD_EXCUTE // ÃüÁîEXCUTE²Ù×÷
} cmd_property_e;

// ATÖ¸Áî¶¯×÷ÐÐÎª
typedef enum
{
  ACTION_OK_EXIT_ERROR_NEXT, // ³É¹¦Ö´ÐÐºóÍË³ö, ´íÎó½«¼ÌÐøÖ´ÐÐÏÂÒ»ÌõÖ¸Áî
  ACTION_OK_NEXT_ERROR_TRY // ³É¹¦Ö´ÐÐºó¼ÌÐøÖ´ÐÐÏÂÒ»ÌõÖ¸Áî, ³ö´íºó½øÐÐ³¢ÊÔ
                           // Èç¹û´ïµ½×î´ó³¢ÊÔ´ÎÊýºó, ÈÔÃ»ÓÐ³É¹¦, ½«ÍË³ö
} cmd_action_e;

// ATÖ¸Áî½á¹¹ÀàÐÍ
typedef struct NB_Cmd_Info_Stru
{
  char *pCMD; //  ATÖ¸Áî
  uint16_t cmdLen; //  ATÖ¸Áî×Ö·û´®³¤¶È
  uint8_t cmd_try; // ³ö´í³¢ÊÔ´ÎÊý
  uint8_t haveTried; // ÒÑ¾­³ö´Î³¢ÊÔµÄ´ÎÊý
  cmd_action_e cmd_action; // ATÖ¸ÁîÐÐÎª
  uint32_t max_timeout; // ×î´ó³¬Ê±Ê±¼ä
  nb_function_t func;
} cmd_info_t;

cmd_info_t *gpExecAtCmd = NULL; // current execute at command
cmd_info_t *gpBlkAtCmd = NULL; // cancel at command

typedef struct
{
  uint8_t buf[NB_BUF_MAX_LEN];
  uint16_t len; // ÓÐÐ§Êý¾Ý³¤¶È
} nb_data_buf_s;

nb_data_buf_s gNBRecvUartBuf; // receive from uart buffer
nb_data_buf_s gNBRecvNetBuf; // receive from net buffer
uint8_t gNBSendNetBuf[NB_BUF_MAX_LEN]; // receive from net buffer

OS_Q gNBSndQue;

// ¶¨Òå´æ´¢NBÄ£×éµÄ×´Ì¬ÐÅÏ¢
struct nb_module_info
{
  uint8_t connection_status;
  uint8_t register_status;
  uint8_t IMSI[16];
  uint8_t IMEI[16];
} g_nb_module_info;

char gNBUdpSocket[5] = {0};

typedef enum
{
  CMD_SEND_TIMER_OUT,
  CMD_BLK_TIMER_OUT
} time_out_event;

//NBTmrHandle gNBCmdTmr;

NB_ReceCB gAppEventCallback = NULL;
time_out_event gTmrEvt[2] = {CMD_SEND_TIMER_OUT, CMD_BLK_TIMER_OUT};

uint8_t gIsNBInit = TD_FALSE;

OS_SEM gNBCmdExecSem;
OS_TCB TestTaskProcTCB;
NBTmrHandle gpNBCmdTmr;

void proc_finish(void);
void reset_rece_buf(void);
void NBTmr_callback(void *p_tmr, void *p_arg);
void TestTmr_callback(void *p_tmr, void *p_arg);
void startTimer(time_out_event event, uint32_t periodms);
void stopTimer(void);
cmd_info_t *cmd_param_init(const char *AT, char *argument, cmd_property_e property);
uint8_t cmd_generate(cmd_info_t *pCmdInfo, const char *AT, char *argument, cmd_property_e property);
void cmd_send(cmd_info_t *pCmdInfo);
void cmd_fail(cmd_info_t *pCmdInfo);
void cmd_free(cmd_info_t *pCmdInfo);
void doRecvUdpData(char *buf, uint16_t len);
void doRecvCoapData(char *buf, uint16_t len);
void task_NBIoT_Snd(void *pvParameters);
void task_NBIoT_Rcv(void *pvParameters);
void task_NET_Rcv(void *pvParameters);
void task_Test(void *pvParameters);
void NB_SendCoapRecv(void);
void NB_SendUdpRecv(uint8_t udp_id, uint16_t udp_len);
void NB_CancelCurProc(void);
uint8_t NB_ASCIIToByte(char *pSrcChar, uint8_t *pDstChar, uint16_t len);
void NB_InitNextCmd(void);
void NB_InitProcess(char *buf, uint16_t len);
uint8_t NB_ProcNotify(char *buf, uint16_t len);
void NB_Process(uint8_t *buf, uint16_t len);

App_Callback_Event getAppEvent(uint8_t nbState)
{
  App_Callback_Event event = EVENT_APP_UNKNOW;

  switch (nbState)
  {
    case PROCESS_NONE:
      printf("getAppEvent nbstate is none, should not run here!\r\n");
      break;
    case PROCESS_INIT:
      event = EVENT_INIT_NB;
      break;
    case PROCESS_MODULE_INFO:
      printf("getAppEvent nbstate is module info, not support now!\r\n");
      break;
    case PROCESS_SIGN:
      printf("getAppEvent nbstate is sign, not support now!\r\n");
      break;
    case PROCESS_NET_REG:
      printf("getAppEvent nbstate is net reg, not support now!\r\n");
      break;
    case PROCESS_UDP_CR: // UDP create
      event = EVENT_UDP_CR;
      break;
    case PROCESS_UDP_CL: // UDP close
      printf("getAppEvent nbstate is udp close, not support now!\r\n");
      break;
    case PROCESS_UDP_ST: // UDP send
      event = EVENT_UDP_ST;
      break;
    case PROCESS_UDP_RE: // UDP receive
      event = EVENT_UDP_RE;
      break;
    case PROCESS_COAP_CR: // Coap create
      event = EVENT_COAP_CR;
      break;
    case PROCESS_COAP_ST: // Coap send
      event = EVENT_COAP_ST;
      break;
    case PROCESS_COAP_RE: // Coap receive
      event = EVENT_COAP_RE;
      break;
    default:
      printf("getAppEvent nbstate=%u unknow!\r\n", nbState);
      break;
  }
	return event;
}

void NBIoT_Init(NB_ReceCB eventCallback)
{
  OS_ERR err;

  gAppEventCallback = eventCallback;

  g_nb_func_state.state = PROCESS_NONE;
  g_nb_func_state.sub_state = 0;

  reset_rece_buf();
  memset((void *)&g_nb_module_info, 0, sizeof(g_nb_module_info));

  memset((void *)gNBRecvUartBuf.buf, 0, NB_BUF_MAX_LEN);
  gNBRecvUartBuf.len = 0;
  memset((void *)gNBRecvNetBuf.buf, 0, NB_BUF_MAX_LEN);
  gNBRecvNetBuf.len = 0;

  memset((void *)gNBSendNetBuf, 0, NB_BUF_MAX_LEN);

  OSSemCreate(&gNBCmdExecSem, "NBCmdExec Sem", 1, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBIoT_Init OSSemCreate gNBCmdExecSem fail, err=%u!\r\n", err);
    return;
  }

  // Queue create
  OSQCreate((OS_Q*)&gNBSndQue,(char *)"NBSndQue",(OS_MSG_QTY)10, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBIoT_Init OSQCreate gNBSndQue fail, err=%u!\r\n", err);
    return;
  }

  OSTaskCreate(&NBIoTSndTaskTCB, "NBSndTask", task_NBIoT_Snd, NULL, NBIOT_SND_TASK_PRIO,
             &NBIoTSndTaskStk[0], NBIOT_SND_TASK_STK_SIZE / 10u, NBIOT_SND_TASK_STK_SIZE,
             0u, 0u, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
             &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBIoT_Init OSTaskCreate NBSndTask fail, err=%u!\r\n", err);
    return;
  }

  OSTaskCreate(&NBIoTRcvTaskTCB, "NBRcvTask", task_NBIoT_Rcv, NULL, NBIOT_RCV_TASK_PRIO,
             &NBIoTRcvTaskStk[0], NBIOT_RCV_TASK_STK_SIZE / 10u, NBIOT_RCV_TASK_STK_SIZE,
             0u, 0u, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
						 &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBIoT_Init OSTaskCreate NBRcvTask fail, err=%u!\r\n", err);
    return;
  }

  OSTaskCreate(&NETTaskProcTCB, "NetProcTask", task_NET_Rcv, NULL, NET_TASK_PROC_PRIO,
             &NETTaskProcStk[0], NET_TASK_STK_SIZE / 10u, NET_TASK_STK_SIZE,
             0u, 0u, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
             &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBIoT_Init OSTaskCreate NetProcTask fail, err=%u!\r\n", err);
    return;
  }

  /*OSTaskCreate(&TestTaskProcTCB, "TestProcTask", task_Test, NULL, 29,
             &NETTaskProcStk[0], 128 / 10u, 128,
             0u, 0u, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
             &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBIoT_Init OSTaskCreate NetProcTask fail, err=%u!\r\n", err);
    return;
  }*/
}

void proc_finish(void)
{
  OS_ERR err;

  stopTimer();
  g_nb_func_state.state = PROCESS_NONE;
  g_nb_func_state.sub_state = 0;
  cmd_free(gpExecAtCmd);
  gpExecAtCmd = NULL;
  OSSemPost(&gNBCmdExecSem, OS_OPT_POST_1, &err);
  if (OS_ERR_NONE != err)
  {
    printf("proc_finish OSSemPost %u error!\r\n", err);
  }
}

void reset_rece_buf(void)
{
  memset(gNBRecvUartBuf.buf, 0, NB_BUF_MAX_LEN);
  gNBRecvUartBuf.len = 0;
}

void cmd_timeout_handle()
{
  if (gpExecAtCmd->cmd_action == ACTION_OK_NEXT_ERROR_TRY)
  {
    gpExecAtCmd->haveTried++;
    if (gpExecAtCmd->haveTried < gpExecAtCmd->cmd_try)
    {
      NBDmaBufferSend((uint8_t *)gpExecAtCmd->pCMD, gpExecAtCmd->cmdLen);
    }
    else
    {
      // Í¨ÖªÉÏ²ãÓ¦ÓÃ, ´Ë¶¯×÷Ö´ÐÐÊ§°Ü
      gAppEventCallback(getAppEvent(g_nb_func_state.state), NULL, 0, TD_FAILURE);
      // ¸´Î»×´Ì¬±êÖ¾
      proc_finish();
    }
  }
  else
  {
    // Í¨ÖªÉÏ²ãÓ¦ÓÃ, ´Ë¶¯×÷Ö´ÐÐÊ§°Ü
    gAppEventCallback(getAppEvent(g_nb_func_state.state), NULL, 0, TD_FAILURE);
    // ¸´Î»×´Ì¬±êÖ¾
    proc_finish();
  }
}

void NBTmr_callback(void *p_tmr, void *p_arg)
{
  time_out_event *pEvent = (time_out_event *)p_arg;

  printf("NBTmr_callback evetn=%u\r\n", *pEvent);

  if (CMD_SEND_TIMER_OUT == *pEvent)
  {
    reset_rece_buf();
    cmd_timeout_handle();
  }
  else if (CMD_BLK_TIMER_OUT == *pEvent)
  {
    if (PROCESS_INIT == g_nb_func_state.state)
    {
      NB_InitNextCmd();
      stopTimer();
    }
  }
}

void TestTmr_callback(void *p_tmr, void *p_arg)
{
  time_out_event *pEvent = (time_out_event *)p_arg;
  nb_tmr_s *pTmr = (nb_tmr_s *)p_tmr;
  OS_ERR err;

  printf("TestTmr_callback %s, evetn=%u, tick=%u\r\n", pTmr->NamePtr, *pEvent, OSTimeGet(&err));
  if (OS_ERR_NONE != err)
  {
    printf("TestTmr_callback %u.\r\n", err);
  }

  //printf("TestTmr_callback NBTmr_Stop %u.\r\n", NBTmr_Stop(pTmr));
}

// must > 100ms
void startTimer(time_out_event event, uint32_t periodms)
{
	//printf("startTimer state=%u, event=%u, periodms=%u\r\n", g_nb_func_state.state, event, periodms);
  if (TD_SUCCESS != NBTmr_Start(&gpNBCmdTmr, NULL, periodms, 0, NBTmr_callback, &gTmrEvt[event]))
  {
    printf("startTimer fail!\r\n");
    return;
  }
}

void stopTimer()
{
  if (TD_SUCCESS != NBTmr_Stop(gpNBCmdTmr))
  {
    printf("stopTimer fail!\r\n");
    return;
  }
}

cmd_info_t *cmd_param_init(const char *AT, char *argument, cmd_property_e property)
{
  cmd_info_t *pCmdInfo = NULL;

  pCmdInfo = malloc(sizeof(cmd_info_t));
  if (NULL == pCmdInfo)
  {
    printf("cmd_param_init: %s malloc pCmdInfo fail!\r\n", AT);
    return NULL;
  }
  memset(pCmdInfo, 0, sizeof(cmd_info_t));

  if (TD_SUCCESS != cmd_generate(pCmdInfo, AT, argument, property))
  {
    printf("cmd_param_init: %s cmd_generate fail!\r\n", AT);
    free(pCmdInfo);
    return NULL;
  }

  // init cmd struct
  pCmdInfo->cmd_try = CMD_TRY_TIMES;
  pCmdInfo->cmd_action = ACTION_OK_NEXT_ERROR_TRY;
  pCmdInfo->haveTried = 0;
  pCmdInfo->max_timeout = 5000; // original 2000
  return pCmdInfo;
}

uint8_t cmd_generate(cmd_info_t *pCmdInfo, const char *AT, char *argument, cmd_property_e property)
{
  char *pAtCmdStr = NULL;
  int cmdLen = 0;
  if (NULL == pCmdInfo)
  {
    printf("cmd_generate: %s pCmdInfo is NULL!\r\n", AT);
    return TD_FAILURE;
  }

  // 1. malloc
  pAtCmdStr = malloc(COMMON_CMD_LENGTH);
  if (NULL == pAtCmdStr)
  {
    printf("cmd_generate: %s malloc pAtCmdStr fail!\r\n", AT);
    return TD_FAILURE;
  }
  memset(pAtCmdStr, 0, COMMON_CMD_LENGTH);

  if (CMD_TEST == property)
  {
    cmdLen = snprintf(pAtCmdStr, COMMON_CMD_LENGTH, "%s=?\r\n", AT);
  }
  else if (CMD_READ == property)
  {
    cmdLen = snprintf(pAtCmdStr, COMMON_CMD_LENGTH, "%s?\r\n", AT);
  }
  else if (CMD_EXCUTE == property)
  {
    cmdLen = snprintf(pAtCmdStr, COMMON_CMD_LENGTH, "%s\r\n", AT);
  }
  else if (CMD_SET == property)
  {
    cmdLen = snprintf(pAtCmdStr, COMMON_CMD_LENGTH, "%s=%s\r\n", AT, argument);
  }

  if (cmdLen <= 0 || cmdLen > COMMON_CMD_LENGTH)
  {
    printf("cmd_generate: %s snprintf retrurn %d error!\r\n", AT, cmdLen);
    free(pAtCmdStr);
    return TD_FAILURE;
  }

  pCmdInfo->pCMD = pAtCmdStr;
  pCmdInfo->cmdLen = (uint16_t)cmdLen;
  return TD_SUCCESS;
}

void cmd_send(cmd_info_t *pCmdInfo)
{
  if (pCmdInfo == NULL)
  {
    printf("cmd_send cmdHandle is NULL!\r\n");
    return;
  }

  // ¿ªÆô¶¨Ê±Æ÷
  if (pCmdInfo->max_timeout > 0)
  {
    startTimer(CMD_SEND_TIMER_OUT, pCmdInfo->max_timeout);
  }

  if (pCmdInfo->cmdLen > 0)
  {
    NBDmaBufferSend((uint8_t *)pCmdInfo->pCMD, pCmdInfo->cmdLen);
  }
  else
  {
    printf("cmd_send cmd len error:%d!\r\n", pCmdInfo->cmdLen);
    return;
  }
}

void cmd_fail(cmd_info_t *pCmdInfo)
{
  if (pCmdInfo->haveTried < pCmdInfo->cmd_try)
  {
    if (pCmdInfo->max_timeout)
    {
      // do nothing wait timeout
    }
    else
    {
      // send immediately
      cmd_send(pCmdInfo);
    }
  }
  else
  {
    NB_CancelCurProc();
  }
}

void cmd_free(cmd_info_t *pCmdInfo)
{
  if (NULL == pCmdInfo)
  {
    return;
  }

  if (NULL != pCmdInfo->pCMD)
  {
    free(pCmdInfo->pCMD);
  }
  
  free(pCmdInfo);
}

void doRecvUdpData(char *buf, uint16_t len)
{
  char *param[6];
  uint16_t index = 0;
  char *tmp_buf = buf;
  while((param[index] = strtok(tmp_buf, ",")) != NULL)
  {
    index++;
    tmp_buf = NULL;
    if (index >= 6)
    {
      break;
    }
  }

  /*for (uint16_t i = 0; i < index; i++)
  {
    printf("doRecvUdpData[%u]: %s\r\n", i, param[i]);
  }*/

  if (index < 5)
  {
    printf("doRecvUdpData index=%u error!\r\n", index);
    gAppEventCallback(EVENT_UDP_RE, NULL, 0, TD_FAILURE);
    cmd_fail(gpExecAtCmd);
    reset_rece_buf();
    return;
  }

  //printf("doRecvUdpData(0) len:%u:%s\r\n", strlen(param[3]), param[3]);
  uint16_t dataLen = strtoul(param[3], 0, 10);
  printf("doRecvUdpData(1) len:%u\r\n", dataLen);
  if (dataLen >= NB_BUF_MAX_LEN)
  {
    printf("doRecvUdpData len=%u too long!\r\n", dataLen);
    gAppEventCallback(EVENT_UDP_RE, NULL, 0, TD_FAILURE);
    cmd_fail(gpExecAtCmd);
    reset_rece_buf();
    return;
  }

  tmp_buf = param[4];
  index = NB_ASCIIToByte(tmp_buf, gNBSendNetBuf, dataLen * 2);
  //gAppEventCallback(EVENT_UDP_RE, pAppData, dataLen, NB_SUCCESS);
  nanrui_output(gNBSendNetBuf, dataLen);

  //OS_ERR err;
  //printf("receive timestamp:%u\r\n", OSTimeGet(&err));

  proc_finish();
  // OK no care
}

void doRecvCoapData(char *buf, uint16_t len)
{
  uint16_t keyIndex = 0;
  uint8_t msgLen = 0;
  char *pCh = buf;
  uint8_t *pAppData = NULL;

  for (uint16_t j = 0; j < len; j++)
  {
    if (pCh[j] == ',')
    {
      keyIndex = j;
      break;
    }
  }

  if (keyIndex > 0)
  {
    uint8_t tempValue[2] = {0, 0};
    if (0 == NB_ASCIIToByte(pCh, tempValue, keyIndex))
    {
      msgLen = tempValue[0];
      if (msgLen > 0 && msgLen < NB_BUF_MAX_LEN)
      {
        printf("doReceCoapData: msglen=%d.\r\n", msgLen);
        pAppData = malloc(msgLen);
        if (NULL != pAppData)
        {
          uint16_t offset = keyIndex + 1;
          if (0 != NB_ASCIIToByte(&pCh[offset], pAppData, msgLen * 2))
          {
            printf("doRecvCoapData: offset=%d error!\r\n", offset);
            cmd_fail(gpExecAtCmd);
            gAppEventCallback(EVENT_COAP_RE, NULL, 0, TD_FAILURE);
            printf("doReceCoapData FAIL.\r\n");
            return;
          }
          printf("doReceCoapData OK.\r\n");
          gAppEventCallback(EVENT_COAP_RE, pAppData, msgLen, TD_SUCCESS);
        }
        else
        {
          printf("doReceCoapData: malloc fail!\r\n");
          gAppEventCallback(EVENT_COAP_RE, NULL, 0, TD_FAILURE);
        }
      }
      else
      {
        printf("doReceCoapData: msgLen=%u error!\r\n", msgLen);
        gAppEventCallback(EVENT_COAP_RE, NULL, 0, TD_FAILURE);
      }
    }
  }
  else
  {
    printf("doReceCoapData: can not find, discard!\r\n");
    gAppEventCallback(EVENT_COAP_RE, NULL, 0, TD_FAILURE);
  }

  proc_finish();
  return;
}

void task_NBIoT_Snd(void *pvParameters)
{
  OS_ERR err;
  (void)pvParameters;

  printf("====RUNNING task_NBIoT_Snd.\r\n");

  while (DEF_TRUE)
  {
    OSSemPend(&gNBCmdExecSem, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    if (OS_ERR_NONE != err)
    {
      printf("task_NBIoT_Snd OSSemPend error:%u!\r\n", err);
      continue;
    }

    cmd_info_t *pAtCmd = OSQPend((OS_Q*)&gNBSndQue, (OS_TICK)0, OS_OPT_PEND_BLOCKING, (OS_MSG_SIZE *)(sizeof(cmd_info_t)), (CPU_TS *)0, &err);
    if (err == OS_ERR_NONE && NULL != pAtCmd)
    {
      if (NULL != gpExecAtCmd)
      {
        printf("task_NBIoT_Snd nbstate=%u, gpExecAtCmd=%s will discard!\r\n", g_nb_func_state.state, gpExecAtCmd->pCMD);
        stopTimer();
        g_nb_func_state.state = PROCESS_NONE;
        g_nb_func_state.sub_state = 0;
        cmd_free(gpExecAtCmd);
        gpExecAtCmd = NULL;
      }
      gpExecAtCmd = pAtCmd;
      g_nb_func_state.state = gpExecAtCmd->func.state;
      g_nb_func_state.sub_state = gpExecAtCmd->func.sub_state;
      cmd_send(gpExecAtCmd);
      if (PROCESS_DBG == gpExecAtCmd->func.state)
      {
        proc_finish();
      }
    }
    else
    {
      printf("task_NBIoT_Snd gNBSndQue receive fail, err=%u!\r\n", err);
      cmd_free(pAtCmd);
    }
  }
}

void task_NBIoT_Rcv(void *pvParameters)
{
	OS_ERR err;
  cmd_info_t *pCmdInfo = NULL;

  printf("====RUNNING task_NBIoT_Rcv.\r\n");
  OSTimeDlyHMSM(0u, 0u, 2u, 0u, OS_OPT_TIME_HMSM_STRICT, &err);

  // 1. send AT wait asyn and go switch case
  pCmdInfo = cmd_param_init(AT_SYNC, NULL, CMD_EXCUTE);
  if (NULL == pCmdInfo)
  {
    printf("task_NBIoT_Rcv %s malloc fail!\r\n", AT_SYNC);
    return;
  }
  // ¸ü¸ÄNBiot²Ù×÷½ø³Ì£¬½øÈëInit×´Ì¬
  pCmdInfo->max_timeout = 1000;
  pCmdInfo->func.state = PROCESS_INIT;
  pCmdInfo->func.sub_state = SUB_SYNC;
  OSQPost(&gNBSndQue, (void *)pCmdInfo, (OS_MSG_SIZE)(sizeof(cmd_info_t)), OS_OPT_POST_FIFO, &err);
  if (OS_ERR_NONE != err)
  {
    printf("task_NBIoT_Rcv: OSQPost fail %u!\r\n", err);
    cmd_free(pCmdInfo);
    return;
  }

  while(DEF_TRUE)
  {
    if (DMA_OK == NBDmaBufferReadReady())
    {
      while ((gNBRecvUartBuf.len = NBDmaBufferRead(gNBRecvUartBuf.buf)) > 0)
      {
        //printf("task_NBIoT_Rcv len=%u:\r\n", gNBRecvUartBuf.len);
        for (uint16_t i = 0; i < gNBRecvUartBuf.len; i++)
        {
          printf("%c", (char *)gNBRecvUartBuf.buf[i]);
        }
        printf("\r\n");

        if (gNBRecvUartBuf.len > 0)
        {
          NB_Process(gNBRecvUartBuf.buf, gNBRecvUartBuf.len);
          reset_rece_buf();
        }
      }
    }
  }
}

void task_NET_Rcv(void *pvParameters)
{
  uint8_t re = TD_FAILURE;
  OS_ERR err;

  printf("====RUNNING task_NET_Rcv.\r\n");
  OSTimeDlyHMSM(0u, 0u, 5u, 0u, OS_OPT_TIME_HMSM_STRICT, &err);

  while(DEF_TRUE)
  {
    if (DMA_OK == NetDmaBufferReadReady())
    {
      while ((gNBRecvNetBuf.len = NetDmaBufferRead(gNBRecvNetBuf.buf)) > 0)
      {
        /*printf("NetDmaBufferRead len=%u ", gNBRecvNetBuf.len);
        for (uint16_t i = 0; i < gNBRecvNetBuf.len; i++)
        {
          printf("0x%02x ", (char *)gNBRecvNetBuf.buf[i]);
        }
        printf("\r\n");*/

        if (gNBRecvNetBuf.len > 0 && gNBRecvNetBuf.len < NB_BUF_MAX_LEN)
        {
          re = NB_SendUdpData(gNBRecvNetBuf.buf, gNBRecvNetBuf.len);
          if (re != TD_SUCCESS)
          {
            printf("NB_SendUdpData fail, discard len=%u packet!\r\n", gNBRecvNetBuf.len);
          }
        }
        else
        {
          printf("NB_SendUdpData fail, len=%u error discard packet!\r\n", gNBRecvNetBuf.len);
        }
        gNBRecvNetBuf.len = 0;
      }
    }
  }
}

void task_Test(void *pvParameters)
{
  OS_ERR err;
  //NBTmrHandle Tmr1, Tmr2, Tmr3, Tmr4;

  printf("====RUNNING task_Test.\r\n");
  OSTimeDlyHMSM(0u, 0u, 5u, 0u, OS_OPT_TIME_HMSM_STRICT, &err);

  /*NBTmr_Start(&Tmr1, "Test Tmr1", 10000, 0, TestTmr_callback, &gTmrEvt[0]);
  NBTmr_Start(&Tmr2, "Test Tmr2", 20000, 0, TestTmr_callback, &gTmrEvt[1]);
  NBTmr_Start(&Tmr3, "Test Tmr3", 25000, 0, TestTmr_callback, &gTmrEvt[0]);
  NBTmr_Start(&Tmr4, "Test Tmr4", 32000, 0, TestTmr_callback, &gTmrEvt[1]);*/

  while(DEF_TRUE)
  {
    OSTimeDlyHMSM(0u, 0u, 40u, 0u, OS_OPT_TIME_HMSM_STRICT, &err);
    NBTmrDump();
  }
}

uint8_t NB_QueryNetWorkReg(void)
{
  OS_ERR err;
  cmd_info_t *pCmdInfo = NULL;

  // 1. check
  if (TD_TRUE != gIsNBInit)
  {
    printf("NB_QueryNetWorkReg: NB should init first!\r\n");
    return TD_FAILURE;
  }

  err = OSQGetState(&gNBSndQue);
  if (OS_ERR_NONE != err)
  {
    printf("NB_QueryNetWorkReg: queue err=%u error!\r\n", err);
    return TD_FAILURE;
  }

  // 2. create cmd
  pCmdInfo = cmd_param_init(AT_CEREG, "1", CMD_SET);
  if (NULL == pCmdInfo)
  {
    printf("NB_QueryNetWorkReg: cmd_param_init fail!\r\n");
    return TD_FAILURE;
  }
  pCmdInfo->cmd_try = 1;
  pCmdInfo->func.state = PROCESS_NET_REG;

  // 3. send to queue
  OSQPost(&gNBSndQue, (void *)pCmdInfo, (OS_MSG_SIZE)(sizeof(cmd_info_t)), OS_OPT_POST_FIFO, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NB_QueryNetWorkReg: OSQPost fail %u!\r\n", err);
    cmd_free(pCmdInfo);
    return TD_FAILURE;
  }

  return TD_SUCCESS;
}

const char* NB_getImsi(void)
{
  return (const char*)g_nb_module_info.IMSI;
}

uint8_t NB_CreateCoap(void)
{
  OS_ERR err;
  cmd_info_t *pCmdInfo = NULL;

  // 1. check
  if (TD_TRUE != gIsNBInit)
  {
    printf("NB_CreateCoap: NB should init first!\r\n");
    return TD_FAILURE;
  }

  err = OSQGetState(&gNBSndQue);
  if (OS_ERR_NONE != err)
  {
    printf("NB_CreateCoap: queue err=%u error!\r\n", err);
    return TD_FAILURE;
  }

  // 2. create cmd
  pCmdInfo = cmd_param_init(AT_NCDP, REMOTE_COAP_INFO, CMD_SET);
  if (NULL == pCmdInfo)
  {
    printf("NB_CreateCoap: cmd_param_init fail!\r\n");
    return TD_FAILURE;
  }
  pCmdInfo->func.state = PROCESS_COAP_CR;

  // 3. send to queue
  OSQPost(&gNBSndQue, (void *)pCmdInfo, (OS_MSG_SIZE)(sizeof(cmd_info_t)), OS_OPT_POST_FIFO, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NB_CreateCoap: OSQPost fail %u!\r\n", err);
    cmd_free(pCmdInfo);
    return TD_FAILURE;
  }

  return TD_SUCCESS;
}

uint8_t NB_SendCoapData(uint8_t *pData, uint16_t len)
{
  OS_ERR err;
  char *pAtCmdStr = NULL;
  cmd_info_t *pCmdInfo = NULL;
  uint16_t strLen = 0;

  // 1. check
  if (TD_TRUE != gIsNBInit)
  {
    printf("NB_SendCoapData: NB should init first!\r\n");
    return TD_FAILURE;
  }

  if (len > NB_BUF_MAX_LEN)
  {
    printf("NB_SendCoapData: datalen=%u too long!\r\n", len);
    return TD_FAILURE;
  }

  err = OSQGetState(&gNBSndQue);
  if (OS_ERR_NONE != err)
  {
    printf("NB_SendCoapData: queue err=%u error!\r\n", err);
    return TD_FAILURE;
  }

  // 2. malloc
  strLen = len * 2 + 100;
  pAtCmdStr = malloc(strLen);
  if (NULL == pAtCmdStr)
  {
    printf("NB_SendCoapData: malloc pAtCmdStr fail!\r\n");
    return TD_FAILURE;
  }
  memset(pAtCmdStr, 0, strLen);

  pCmdInfo = malloc(sizeof(cmd_info_t));
  if (NULL == pCmdInfo)
  {
    printf("NB_SendCoapData: malloc pCmdInfo fail!\r\n");
    free(pAtCmdStr);
    return TD_FAILURE;
  }
  memset(pCmdInfo, 0, sizeof(cmd_info_t));

  // 3. snprintf
  uint16_t strPos = snprintf(pAtCmdStr, strLen, "%s=%d,", AT_NMGS, len);
  uint16_t tempPos = 0;
  for (uint16_t i = 0 ; i < len ; i++)
  {
    tempPos += sprintf(&pAtCmdStr[strPos + (i << 1)], "%02X", pData[i]);
  }
  tempPos += sprintf(&pAtCmdStr[strPos + tempPos], "\r\n");

  // 4. init cmd struct
  pCmdInfo->cmd_try = CMD_TRY_TIMES;
  pCmdInfo->cmd_action = ACTION_OK_NEXT_ERROR_TRY;
  pCmdInfo->haveTried = 0;
  pCmdInfo->max_timeout = 10000;
  pCmdInfo->func.state = PROCESS_COAP_ST;
  pCmdInfo->pCMD = pAtCmdStr;
  pCmdInfo->cmdLen = strPos + tempPos;

  // 5. send to queue
  OSQPost(&gNBSndQue, (void *)pCmdInfo, (OS_MSG_SIZE)(sizeof(cmd_info_t)), OS_OPT_POST_FIFO, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NB_SendCoapData: OSQPost fail %u!\r\n", err);
    cmd_free(pCmdInfo);
    return TD_FAILURE;
  }

  return TD_SUCCESS;
}

void NB_SendCoapRecv(void)
{
  OS_ERR err;
  cmd_info_t *pCmdInfo = NULL;

  // 1. check
  if (TD_TRUE != gIsNBInit)
  {
    printf("NB_SendCoapRecv: NB should init first!\r\n");
    return;
  }

  err = OSQGetState(&gNBSndQue);
  if (OS_ERR_NONE != err)
  {
    printf("NB_SendCoapRecv: queue err=%u error!\r\n", err);
    return;
  }

  // 2. create cmd
  pCmdInfo = cmd_param_init(AT_NMGR, NULL, CMD_EXCUTE);
  if (NULL == pCmdInfo)
  {
    printf("NB_SendCoapRecv: cmd_param_init fail!\r\n");
    return;
  }
  pCmdInfo->func.state = PROCESS_COAP_RE;

  // 3. send to queue
  OSQPost(&gNBSndQue, (void *)pCmdInfo, (OS_MSG_SIZE)(sizeof(cmd_info_t)), OS_OPT_POST_FIFO, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NB_SendCoapRecv: OSQPost fail %u!\r\n", err);
    cmd_free(pCmdInfo);
    return;
  }

  return;
}

uint8_t NB_CreateUdp(void)
{
  OS_ERR err;
  cmd_info_t *pCmdInfo = NULL;

  // 1. check
  if (TD_TRUE != gIsNBInit)
  {
    printf("NB_CreateUdp: NB should init first!\r\n");
    return TD_FAILURE;
  }

  err = OSQGetState(&gNBSndQue);
  if (OS_ERR_NONE != err)
  {
    printf("NB_CreateUdp: queue err=%u error!\r\n", err);
    return TD_FAILURE;
  }

  // 2. create cmd
  pCmdInfo = cmd_param_init(AT_NSOCR, LOCAL_UDP_SET, CMD_SET);
  if (NULL == pCmdInfo)
  {
    printf("NB_CreateUdp: cmd_param_init fail!\r\n");
    return TD_FAILURE;
  }
  pCmdInfo->func.state = PROCESS_UDP_CR;

  // 3. send to queue
  OSQPost(&gNBSndQue, (void *)pCmdInfo, (OS_MSG_SIZE)(sizeof(cmd_info_t)), OS_OPT_POST_FIFO, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NB_CreateUdp: OSQPost fail %u!\r\n", err);
    cmd_free(pCmdInfo);
    return TD_FAILURE;
  }

  return TD_SUCCESS;
}

uint8_t NB_SendUdpData(uint8_t *pData, uint16_t len)
{
  OS_ERR err;
  char *pAtCmdStr = NULL;
  cmd_info_t *pCmdInfo = NULL;
  uint16_t strLen = 0;

  // 1. check
  if (TD_TRUE != gIsNBInit)
  {
    printf("NB_SendUdpData: NB should init first!\r\n");
    return TD_FAILURE;
  }

  if (len > NB_BUF_MAX_LEN)
  {
    printf("NB_SendUdpData: datalen=%u too long!\r\n", len);
    return TD_FAILURE;
  }

  if (gNBUdpSocket[0] < '0' || gNBUdpSocket[0] > '6' )
  {
    printf("NB_SendUdpData: udp id=%c error!\r\n", gNBUdpSocket[0]);
    return TD_FAILURE;
  }

  err = OSQGetState(&gNBSndQue);
  if (OS_ERR_NONE != err)
  {
    printf("NB_SendUdpData: queue err=%u error!\r\n", err);
    return TD_FAILURE;
  }

  // 2. malloc
  strLen = len * 2 + 100;
  pAtCmdStr = malloc(strLen);
  if (NULL == pAtCmdStr)
  {
    printf("NB_SendUdpData: malloc pAtCmdStr fail!\r\n");
    return TD_FAILURE;
  }
  memset(pAtCmdStr, 0, strLen);

  pCmdInfo = malloc(sizeof(cmd_info_t));
  if (NULL == pCmdInfo)
  {
    printf("NB_SendUdpData: malloc pCmdInfo fail!\r\n");
    free(pAtCmdStr);
    return TD_FAILURE;
  }
  memset(pCmdInfo, 0, sizeof(cmd_info_t));

  // 3. snprintf
  uint16_t strPos = snprintf(pAtCmdStr, strLen, "%s=%c,%s,%s,%d,", AT_NSOST, gNBUdpSocket[0],
                              REMOTE_SERVER_IP, REMOTE_SERVER_PORT, len);
  uint16_t tempPos = 0;
  for (uint16_t i = 0 ; i < len ; i++)
  {
    tempPos += sprintf(&pAtCmdStr[strPos + (i << 1)], "%02X", pData[i]);
  }
  tempPos += sprintf(&pAtCmdStr[strPos + tempPos], "\r\n");

  // 4. init cmd struct
  pCmdInfo->cmd_try = CMD_TRY_TIMES;
  pCmdInfo->cmd_action = ACTION_OK_NEXT_ERROR_TRY;
  pCmdInfo->haveTried = 0;
  pCmdInfo->max_timeout = 1000;
  pCmdInfo->func.state = PROCESS_UDP_ST;
  pCmdInfo->pCMD = pAtCmdStr;
  pCmdInfo->cmdLen = strPos + tempPos;

  // 5. send to queue
  OSQPost(&gNBSndQue, (void *)pCmdInfo, (OS_MSG_SIZE)(sizeof(cmd_info_t)), OS_OPT_POST_FIFO, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NB_SendUdpData: OSQPost fail %u!\r\n", err);
    cmd_free(pCmdInfo);
    return TD_FAILURE;
  }

  //printf("send timestamp:%u\r\n", OSTimeGet(&err));

  return TD_SUCCESS;
}

void NB_SendUdpRecv(uint8_t udp_id, uint16_t udp_len)
{
  OS_ERR err;
  cmd_info_t *pCmdInfo = NULL;
  int strLen = 0;
  char param[10] = {0};

  // 1. check
  if (TD_TRUE != gIsNBInit)
  {
    printf("NB_SendUdpRecv: NB should init first!\r\n");
    return;
  }

  if (udp_len > NB_BUF_MAX_LEN)
  {
    printf("NB_SendUdpRecv: datalen=%u too long!\r\n", udp_len);
    return;
  }

  if (udp_id > 6) // 0-6
  {
    printf("NB_SendUdpRecv: udp id=%d error!\r\n", udp_id);
    return;
  }

  err = OSQGetState(&gNBSndQue);
  if (OS_ERR_NONE != err)
  {
    printf("NB_SendUdpRecv: queue err=%u error!\r\n", err);
    return;
  }

  // 2. snprintf cmd para
  strLen = snprintf(param, 10, "%d,%d", udp_id, udp_len);
  if (strLen <= 0 || strLen > 10)
  {
    printf("NB_SendUdpRecv snprintf return %d error!\r\n", strLen);
    return;
  }

  // 3. create cmd
  pCmdInfo = cmd_param_init(AT_NSORF, param, CMD_SET);
  if (NULL == pCmdInfo)
  {
    printf("NB_SendUdpRecv: cmd_param_init fail!\r\n");
    return;
  }
  pCmdInfo->func.state = PROCESS_UDP_RE;

  // 4. send to queue
  OSQPost(&gNBSndQue, (void *)pCmdInfo, (OS_MSG_SIZE)(sizeof(cmd_info_t)), OS_OPT_POST_LIFO, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NB_SendUdpRecv: OSQPost fail %u!\r\n", err);
    cmd_free(pCmdInfo);
    return;
  }
}

void NB_DbgATCmdSend(uint8_t *pCmd, uint16_t len)
{
  OS_ERR err;
  char *pAtCmdStr = NULL;
  cmd_info_t *pCmdInfo = NULL;

  // 1. check
  if (len > NB_BUF_MAX_LEN)
  {
    printf("NB_DbgATCmdSend: datalen=%u too long!\r\n", len);
    return;
  }

  err = OSQGetState(&gNBSndQue);
  if (OS_ERR_NONE != err)
  {
    printf("NB_DbgATCmdSend: queue err=%u error!\r\n", err);
    return;
  }

  pCmdInfo = malloc(sizeof(cmd_info_t));
  if (NULL == pCmdInfo)
  {
    printf("NB_DbgATCmdSend: %s malloc pCmdInfo fail!\r\n", pCmd);
    return;
  }
  memset(pCmdInfo, 0, sizeof(cmd_info_t));

  pAtCmdStr = malloc(len);
  if (NULL == pAtCmdStr)
  {
    printf("NB_DbgATCmdSend: %s malloc pAtCmdStr fail!\r\n", pCmd);
    return;
  }
  memset(pAtCmdStr, 0, len);

  // init cmd struct
  memcpy(pAtCmdStr, pCmd, len);
  pCmdInfo->cmd_try = 1;
  pCmdInfo->cmd_action = ACTION_OK_NEXT_ERROR_TRY;
  pCmdInfo->haveTried = 0;
  pCmdInfo->max_timeout = 0;
  pCmdInfo->func.state = PROCESS_DBG;
  pCmdInfo->pCMD = pAtCmdStr;
  pCmdInfo->cmdLen = len;

  // send to queue
  OSQPost(&gNBSndQue, (void *)pCmdInfo, (OS_MSG_SIZE)(sizeof(cmd_info_t)), OS_OPT_POST_FIFO, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NB_DbgATCmdSend: OSQPost fail %u!\r\n", err);
    cmd_free(pCmdInfo);
    return;
  }
}

void NB_CancelCurProc(void)
{
  printf("NB_CancelCurProc g_nb_func_state=%d cancel.\r\n", g_nb_func_state.state);
  if (NULL != gpExecAtCmd && NULL != gpExecAtCmd->pCMD)
  {
    printf("NB_CancelCurProc cmd=%s cancel.\r\n", gpExecAtCmd->pCMD);
  }
  proc_finish();
}

uint8_t NB_ASCIIToByte(char *pSrcChar, uint8_t *pDstChar, uint16_t len)
{
  uint8_t tmp = 0;
  uint8_t tmp1 = 0;

  for (uint16_t i = 0; i < len; i++)
  {
    if (pSrcChar[i] >= '0' && pSrcChar[i] <= '9')
    {
      tmp = pSrcChar[i] - '0';
    }
    else if (pSrcChar[i] >= 'A' && pSrcChar[i] <= 'F')
    {
      tmp = pSrcChar[i] - 'A' + 10; 
    }
    else if (pSrcChar[i] >= 'a' && pSrcChar[i] <= 'f')
    {
      tmp = pSrcChar[i] - 'a' + 10;
    }
    else
    {
      printf("NB_ASCIIToByte: [%d]:0x%02x error!\r\n", i, pSrcChar[i]);
      return 1;
    }

    if ((i%2) == 0)
    {
      tmp1 = tmp;
      pDstChar[i] = tmp1;
    }
    else
    {
      tmp1 = (tmp1 << 4) | tmp;
      pDstChar[i >> 1] = tmp1;
    }
  }

  return 0;
}

void NB_InitNextCmd(void)
{
  cmd_info_t *pCmdInfo = NULL;
  OS_ERR err;

  if (g_nb_func_state.sub_state == SUB_INIT_END)
  {
    printf("NB_InitNextCmd: not next cmd.\r\n");
    return;
  }

  switch(g_nb_func_state.sub_state)
  {
    case SUB_CMEE:
      pCmdInfo = cmd_param_init(AT_CMEE, "1", CMD_SET);
      if (NULL == pCmdInfo)
      {
        printf("NB_InitNextCmd %s cmd_param_init fail!\r\n", AT_CMEE);
        proc_finish();
      }
      else
      {
        pCmdInfo->func.state = PROCESS_INIT;
        pCmdInfo->func.sub_state = SUB_CMEE;
      }
      break;
  
    case SUB_CFUN:
      pCmdInfo = cmd_param_init(AT_CFUN, "1", CMD_SET);
      if (NULL == pCmdInfo)
      {
        printf("NB_InitNextCmd %s cmd_param_init fail!\r\n", AT_CFUN);
        proc_finish();
      }
      else
      {
        pCmdInfo->max_timeout = 10000; //10S 
        pCmdInfo->func.state = PROCESS_INIT;
        pCmdInfo->func.sub_state = SUB_CFUN;
      }
      break;
  
    case SUB_CIMI:
      pCmdInfo = cmd_param_init(AT_CIMI, NULL, CMD_EXCUTE);
      if (NULL == pCmdInfo)
      {
        printf("NB_InitNextCmd %s cmd_param_init fail!\r\n", AT_CIMI);
        proc_finish();
      }
      else
      {
        pCmdInfo->func.state = PROCESS_INIT;
        pCmdInfo->func.sub_state = SUB_CIMI;
      }
      break;
  
    case SUB_CGSN:
      pCmdInfo = cmd_param_init(AT_CGSN, "1", CMD_SET);
      if (NULL == pCmdInfo)
      {
        printf("NB_InitNextCmd %s cmd_param_init fail!\r\n", AT_CGSN);
        proc_finish();
      }
      else
      {
        pCmdInfo->func.state = PROCESS_INIT;
        pCmdInfo->func.sub_state = SUB_CGSN;
      }
      break;
  
    case SUB_CGATT:
      pCmdInfo = cmd_param_init(AT_CGATT, "1", CMD_SET);
      if (NULL == pCmdInfo)
      {
        printf("NB_InitNextCmd %s set cmd_param_init fail!\r\n", AT_CGATT);
        proc_finish();
      }
      else
      {
        pCmdInfo->max_timeout = 3000;
        pCmdInfo->func.state = PROCESS_INIT;
        pCmdInfo->func.sub_state = SUB_CGATT;
      }
      break;
  
    case SUB_CGATT_QUERY:
      pCmdInfo = cmd_param_init(AT_CGATT, NULL, CMD_READ);
      if (NULL == pCmdInfo)
      {
        printf("NB_InitNextCmd %s read cmd_param_init fail!\r\n", AT_CGATT);
        proc_finish();
      }
      else
      {
        pCmdInfo->max_timeout = 3000;
        pCmdInfo->func.state = PROCESS_INIT;
        pCmdInfo->func.sub_state = SUB_CGATT_QUERY;
      }
      break;
  
    case SUB_NSMI:
      pCmdInfo = cmd_param_init(AT_NSMI, "0", CMD_SET); // turn off send indication
      if (NULL == pCmdInfo)
      {
        printf("NB_InitNextCmd %s set cmd_param_init fail!\r\n", AT_NSMI);
        proc_finish();
      }
      else
      {
        pCmdInfo->func.state = PROCESS_INIT;
        pCmdInfo->func.sub_state = SUB_NSMI;
      }
      break;
  
    case SUB_NNMI:
      pCmdInfo = cmd_param_init(AT_NNMI, "1", CMD_SET);
      if (NULL == pCmdInfo)
      {
        printf("NB_InitNextCmd %s set cmd_param_init fail!\r\n", AT_NSMI);
        proc_finish();
      }
      else
      {
        pCmdInfo->func.state = PROCESS_INIT;
        pCmdInfo->func.sub_state = SUB_NNMI;
      }
      break;
  
    default:
      printf("NB_InitNextCmd:sub_state=%u, unknow and reset.\r\n", g_nb_func_state.sub_state);
      proc_finish();
      break;
  }

  if (NULL != pCmdInfo)
  {
    OSQPost(&gNBSndQue, (void *)pCmdInfo, (OS_MSG_SIZE)(sizeof(cmd_info_t)), OS_OPT_POST_FIFO, &err);
    if (OS_ERR_NONE != err)
    {
      printf("NB_InitNextCmd: OSQPost fail %u!\r\n", err);
      cmd_free(pCmdInfo);
      proc_finish();
    }
    else
    {
      //printf("NB_InitNextCmd: OSQPost success.\r\n");
    }
  }
}

void NB_InitProcess(char *buf, uint16_t len)
{
  OS_ERR err;
  uint8_t isPass = TD_FAILURE;
  uint8_t isFinish = 0;

  switch (g_nb_func_state.sub_state)
  {
    case SUB_CIMI:
      if (strstr(buf, "460"))
      {
        isFinish = 0;
        isPass = TD_SUCCESS;
        memcpy(g_nb_module_info.IMSI, buf, 15);
        g_nb_module_info.IMSI[15] = 0;
        printf("NB_InitProcess SUB_CIMI imsi:%s\r\n", g_nb_module_info.IMSI);
      }
      else if (strstr(buf, "OK"))
      {
        isFinish = 1;
        isPass = TD_SUCCESS;
        printf("NB_InitProcess SUB_CIMI OK.\r\n");
      }
      else
      {
        isFinish = 0;
        isPass = TD_FAILURE;
        printf("NB_InitProcess SUB_CIMI FAIL.\r\n");
      }
      break;
    case SUB_CGSN:
      if (strstr(buf, "+CGSN:"))
      {
        isFinish = 0;
        isPass = TD_SUCCESS;
        memcpy(g_nb_module_info.IMEI, buf, 15);
        g_nb_module_info.IMEI[15] = 0;
        printf("NB_InitProcess SUB_CGSN imei:%s\r\n", g_nb_module_info.IMEI);
      }
      else if (strstr(buf, "OK"))
      {
        isFinish = 1;
        isPass = TD_SUCCESS;
        printf("NB_InitProcess SUB_CGSN OK.\r\n");
      }
      else
      {
        isFinish = 0;
        isPass = TD_FAILURE;
        printf("NB_InitProcess SUB_CGSN FAIL.\r\n");
      }
      break;
    case SUB_CGATT:
      if (strstr(buf, "CGATT:1"))
      {
        isFinish = 0;
        isPass = TD_SUCCESS;
        printf("NB_InitProcess SUB_CGATT is 1.\r\n");
      }
      else if (strstr(buf, "OK"))
      {
        isFinish = 1;
        isPass = TD_SUCCESS;
        printf("NB_InitProcess SUB_CGATT OK.\r\n");
      }
      else
      {
        isFinish = 0;
        isPass = TD_FAILURE;
        printf("NB_InitProcess SUB_CGATT FAIL.\r\n");
      }
      break;
    default:
      if (strstr(buf, "OK"))
      {
        isFinish = 1;
        isPass = TD_SUCCESS;
      }
      else if (strstr(buf, "ERROR"))
      {
        isFinish = 0;
        isPass = TD_FAILURE;
      }
      break;
  }

  //printf("NB_InitProcess: isFinish=%u, isPass=%u, substate=%u, %s\r\n", isFinish, isPass, g_nb_func_state.sub_state, buf);

  if (isPass != TD_SUCCESS)
  {
    cmd_fail(gpExecAtCmd);
  }
  else
  {
    if (isFinish) // do next
    {
      OSSemPost(&gNBCmdExecSem, OS_OPT_POST_1, &err);
      if (OS_ERR_NONE != err)
      {
        printf("NB_InitProc OSSemPost %u error!\r\n", err);
      }

      stopTimer();
      cmd_free(gpExecAtCmd);
      gpExecAtCmd = NULL;
      g_nb_func_state.sub_state++;
      if (g_nb_func_state.sub_state == SUB_INIT_END)
      {
        printf("NB_InitProc end.\r\n");
        gAppEventCallback(EVENT_INIT_NB, NULL, 0, TD_SUCCESS);
        g_nb_func_state.state = PROCESS_NONE;
        g_nb_func_state.sub_state = 0;
        gIsNBInit = TD_TRUE;
        return;
      }

      // after 3s execute next init at cmd
      //startTimer(CMD_BLK_TIMER_OUT, 3000);
      OSTimeDlyHMSM(0u, 0u, 1u, 0u, OS_OPT_TIME_HMSM_STRICT, &err);
      NB_InitNextCmd();
    }
    else
    {
      // ´ËÖ¸Áî»¹Î´Ö´ÐÐÍê
      // do nothing
    }
  }
}

uint8_t NB_ProcNotify(char *buf, uint16_t len)
{
  uint8_t isNotify = TD_FALSE;
  char *pCh = NULL;

  if ((pCh = strstr(buf, "+NSONMI")) != NULL)
  {
    // ÊÕµ½·þÎñÆ÷¶Ë·¢À´µÄUDPÊý¾Ý
    uint8_t udp_id = 0;
    uint16_t udp_len = 0;
    char *pSocke = strchr(pCh, ':');
    if (pSocke)
    {
      pSocke++;
      udp_id = strtoul(pSocke, 0, 10);
    }
    char *pMsgLen = strchr(pSocke, ',');
    if (pMsgLen)
    {
      pMsgLen++;
      udp_len = strtoul(pMsgLen, 0, 10);
    }

    // begin receive process
    NB_SendUdpRecv(udp_id, udp_len);
    isNotify = TD_TRUE;
  }
  else if ((pCh = strstr(buf, "+NNMI")) != NULL)
  {
    if (len > 7)
    {
      char *pChTemp = strchr(pCh, ':');
      if (pChTemp)
      {
        pChTemp++;
        doRecvCoapData(pChTemp, len - 6);
      }
    }
    else
    {
      NB_SendCoapRecv();
    }
    isNotify = TD_TRUE;
  }
  else if ((pCh = strstr(buf, "+NSMI")) != NULL) // not need
  {
    // do nothing
    isNotify = TD_TRUE;
  }

  return isNotify;
}

void NB_Process(uint8_t *buf, uint16_t len)
{
  char *pChar = (char *)buf;
	char *pChTemp = NULL;

  if (NB_ProcNotify(pChar, len)) // notify
  {
    // has do in NB_ProcNotify
  }
  else // response
  {
    switch (g_nb_func_state.state)
    {
      case PROCESS_UDP_ST: // UDP send response
        if (strstr(pChar, "OK"))
        {
          gAppEventCallback(EVENT_UDP_ST, NULL, 0, TD_SUCCESS);
          proc_finish();
        }
        else // if (strstr(buf, "ERROR"))
        {
          cmd_fail(gpExecAtCmd);
        }
        break;

      case PROCESS_UDP_RE:
        doRecvUdpData(pChar, len);
        break;

      case PROCESS_COAP_ST:
        if (strstr(pChar, "OK"))
        {
          gAppEventCallback(EVENT_COAP_ST, NULL, 0, TD_SUCCESS);
          proc_finish();
        }
        else // if (strstr(buf, "ERROR"))
        {
          cmd_fail(gpExecAtCmd);
        }
        break;

      case PROCESS_COAP_RE:
        doRecvCoapData(pChar, len);
        break;
        
      case PROCESS_INIT:
        NB_InitProcess(pChar, len);
        break;

      case PROCESS_UDP_CR:
        if (0 == gNBUdpSocket[0])
        {
          if (len >= 2)
          {
            printf("NB_Process PROCESS_UDP_CR socket:%s error!\r\n", pChar);
            cmd_fail(gpExecAtCmd);
            return;
          }
          memcpy(gNBUdpSocket, buf, len);
        }
        else if (strstr(pChar, "OK"))
        {
          gAppEventCallback(EVENT_UDP_CR, NULL, 0, TD_SUCCESS);
          proc_finish();
        }
        else
        {
          cmd_fail(gpExecAtCmd);
          printf("NB_Process PROCESS_UDP_CR fail!\r\n");
        }
        break;

      case PROCESS_COAP_CR:
        if (strstr(pChar, "OK"))
        {
          gAppEventCallback(EVENT_COAP_CR, NULL, 0, TD_SUCCESS);
          proc_finish();
        }
        else
        {
          pChar = strchr(pChar, ':');
          if (pChar)
          {
            pChar++;
          }
          gAppEventCallback(EVENT_COAP_CR, (uint8_t *)pChar, strlen(pChar), TD_FAILURE);
          cmd_fail(gpExecAtCmd);
        }
        break;

      case PROCESS_SIGN:
        break;

      case PROCESS_NET_REG:
        if (strstr(pChar, "+CEREG"))
        {
          pChTemp = strchr(pChar, ':');
          if (pChTemp)
          {
            pChTemp++;
            g_nb_module_info.register_status = (*pChTemp - 0x30);
          }
          proc_finish();
        }
        else
        {
          cmd_fail(gpExecAtCmd);
          printf("NB_Process PROCESS_NET_REG receive error.\r\n");
        }
        break;

      case PROCESS_MODULE_INFO:
        break;

      case PROCESS_UDP_CL:
        break;

      default:
        //printf("NB_Process: unknow state=%u\r\n", g_nb_func_state.state);
        break;
    }
  }
}

