/**
  ******************************************************************************
  * @file    main.c
  * @author  fire
  * @version V1.0
  * @date    2015-xx-xx
  * @brief   DHCP例程 ucos版本
  ******************************************************************************
  * @attention
  *
  * 实验平台:秉火  STM32 F429 开发板 
  * 论坛    :http://www.firebbs.cn
  * 淘宝    :https://fire-stm32.taobao.com
  *
  ******************************************************************************
  */
#include <String.h>
#include "includes.h"
#include "nbiot/tdtech.h"
#include "lwipopts/netconf.h"
#include "nbiot/nbiot.h"
#include "nbiot/Dma_buffer.h"
#include "nbiot/nbtimer.h"

/*
*********************************************************************************************************
*                                            LOCAL DEFINES
*********************************************************************************************************
*/
#define APP_CFG_TASK_START_STK_SIZE 512u

/* --------------- APPLICATION GLOBALS ---------------- */
static  OS_TCB                        AppTaskStartTCB;
static  CPU_STK                       AppTaskStartStk[APP_CFG_TASK_START_STK_SIZE];

#ifdef USE_DHCP
#define APP_CFG_TASK_DHCP_PRIO        3
#define APP_CFG_TASK_DHCP_STK_SIZE    512
static  OS_TCB                        AppTaskDHCPTCB;
static  CPU_STK                       AppTaskDHCPStk[APP_CFG_TASK_DHCP_STK_SIZE];
#endif

/*
*********************************************************************************************************
*                                         FUNCTION PROTOTYPES
*********************************************************************************************************
*/

uint8_t gAppWork = TD_FALSE;

#define TD_TASK_START_STK_SIZE 512u
static OS_TCB TDTaskStartTCB;
static CPU_STK TDTaskStartStk[TD_TASK_START_STK_SIZE];

#define APP_TASK_QUE_STK_SIZE 512u
static OS_TCB AppTaskQueTCB;
static CPU_STK AppTaskQueStk[APP_TASK_QUE_STK_SIZE];

static void AppTaskStart (void *p_arg);
static void TDTaskStart(void *p_arg);
static void AppTaskQue(void *p_arg);
void AppNBCallback(App_Callback_Event event, uint8_t *pData, uint16_t len, uint8_t result);

extern struct netif gnetif;

OS_Q gAppMsgQue;
const uint8_t MSG_FROM_NB = 1;
const uint8_t APP_OK = 0;
const uint8_t APP_ERR = 1;

// msgtype
typedef enum
{
  MSG_INIT_NB,
  MSG_COAP_CR,
  MSG_COAP_ST,
  MSG_COAP_RE,
  MSG_UDP_CR,
  MSG_UDP_ST,
  MSG_UDP_RE
} msg_type;

typedef struct
{
  uint8_t src; // from
  uint8_t msgtype;
  uint8_t result;
  uint8_t *pData; // 指令参数
  uint16_t len;
} app_msg_t;

#define APP_DBG_BUF_LEN 100
static uint8_t gAppDbgBuffer[APP_DBG_BUF_LEN];

void AppProcMsgFromNB(app_msg_t *pAppMsg);
void AppProcMsg(app_msg_t *pAppMsg);

/*
*********************************************************************************************************
*                                                main()
*
* Description : This is the standard entry point for C code.  It is assumed that your code will call
*               main() once you have performed all necessary initialization.
*
* Arguments   : none
*
* Returns     : none
*
*********************************************************************************************************
*/

int main(void)
{
	OS_ERR err;
           
  /* 初始化调试串口，一般为串口1 */
  Debug_USART_Config();

  printf("\r\n====TD-Tech Demo====\r\n");

	Mem_Init(); /* Initialize Memory Managment Module */
	Math_Init(); /* Initialize Mathematical Module */

#if (CPU_CFG_NAME_EN == DEF_ENABLED)
  CPU_NameSet((CPU_CHAR *)"STM32F429II", (CPU_ERR  *)&err);
#endif

	BSP_IntDisAll(); /* Disable all Interrupts. */

	OSInit(&err); /* Init uC/OS-III. */
	App_OS_SetAllHooks();

	OSTaskCreate(&AppTaskStartTCB, "App Task", AppTaskStart, 0u, APP_CFG_TASK_START_PRIO,
								&AppTaskStartStk[0u],	APP_CFG_TASK_START_STK_SIZE / 10u,
								APP_CFG_TASK_START_STK_SIZE, 0u, 0u, 0u, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
							  &err);
	OSStart(&err); /* Start multitasking (i.e. give control to uC/OS-III). */
}

/*
*********************************************************************************************************
*                                          STARTUP TASK
*
* Description : This is an example of a startup task.  As mentioned in the book's text, you MUST
*               initialize the ticker only once multitasking has started.
*
* Arguments   : p_arg   is the argument passed to 'AppTaskStart()' by 'OSTaskCreate()'.
*
* Returns     : none
*
* Notes       : 1) The first line of code is used to prevent a compiler warning because 'p_arg' is not
*                  used.  The compiler should not generate any code for this statement.
*********************************************************************************************************
*/

static void AppTaskStart(void *p_arg)
{
  OS_ERR err;
  (void)p_arg;

  printf("====RUNNING AppTaskStart.\r\n");

  BSP_Init(); /* Initialize BSP functions */
  CPU_Init(); /* Initialize the uC/CPU services */

#if OS_CFG_STAT_TASK_EN > 0u
  OSStatTaskCPUUsageInit(&err); /* Compute CPU capacity with no task running */
#endif

#ifdef CPU_CFG_INT_DIS_MEAS_EN
  CPU_IntDisMeasMaxCurReset();
#endif

	OSTaskCreate(&TDTaskStartTCB, "TD Task", TDTaskStart, 0u, TD_TASK_START_PRIO,
								&TDTaskStartStk[0u],	TD_TASK_START_STK_SIZE / 10u,
								TD_TASK_START_STK_SIZE, 0u, 0u, 0u, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
							  &err);

  while (DEF_TRUE) {
    LED3_TOGGLE;
    OSTimeDlyHMSM( 0u, 0u, 1u, 0u, OS_OPT_TIME_HMSM_STRICT, &err);
  }
}

static void TDTaskStart(void *p_arg)
{
  uint16_t len = 0;
  OS_ERR err;

  printf("====RUNNING TDTaskStart.\r\n");

  DmaBufferInit();
  NBTmr_Init();

#ifdef USE_DHCP
   /* Start DHCPClient */
  OSTaskCreate(&AppTaskDHCPTCB, "DHCP", LwIP_DHCP_task, &gnetif, APP_CFG_TASK_DHCP_PRIO,
               &AppTaskDHCPStk[0], APP_CFG_TASK_DHCP_STK_SIZE / 10u,
               APP_CFG_TASK_DHCP_STK_SIZE, 0u, 0u, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
               &err);
#endif //#ifdef USE_DHCP

  // Queue create
  OSQCreate((OS_Q*)&gAppMsgQue,(char *)"AppMsgQue",(OS_MSG_QTY)10, &err);
  if (OS_ERR_NONE != err)
  {
    printf("TDTaskStart OSQCreate AppMsgQue fail, err=%u!\r\n", err);
    return;
  }

  // que task
  OSTaskCreate((OS_TCB *)&AppTaskQueTCB,
              (CPU_CHAR *)"App Task Queue",
              (OS_TASK_PTR)AppTaskQue,
              (void *)0,
              (OS_PRIO)APP_TASK_QUE_PRIO,
              (CPU_STK *)&AppTaskQueStk[0],
              (CPU_STK_SIZE)APP_TASK_QUE_STK_SIZE / 10,
              (CPU_STK_SIZE)APP_TASK_QUE_STK_SIZE,
              (OS_MSG_QTY)5u,
              (OS_TICK) 0u,
              (void *)0,
              (OS_OPT)(OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
              (OS_ERR *)&err);
  if (err != OS_ERR_NONE)
  {
    printf("TDTaskStart OSTaskCreate App Task Queue fail, err=%u!\r\n", err);
    return;
  }

  NBIoT_Init(AppNBCallback);

  /* Initilaize the LwIP stack */
  LwIP_Init();

  memset(gAppDbgBuffer, 0, APP_DBG_BUF_LEN);
  while (DEF_TRUE)
  {
    if (DMA_OK == DbgDmaBufferReadReady())
    {
      while ((len = DbgDmaBufferRead(gAppDbgBuffer)) > 0)
      {
        if (len > 0)
        {
          /*for (uint16_t i = 0; i < len; i++)
          {
            printf("%c", gAppDbgBuffer[i]);
          }*/
          printf("\r\n");
          NB_DbgATCmdSend(gAppDbgBuffer, len);
        }
        memset(gAppDbgBuffer, 0, APP_DBG_BUF_LEN);
      }
    }
  }
}

/*
*********************************************************************************************************
*                                          AppTaskObj0()
*
* Description : Test uC/OS-III objects.
*
* Argument(s) : p_arg is the argument passed to 'AppTaskObj0' by 'OSTaskCreate()'.
*
* Return(s)   : none
*
* Caller(s)   : This is a task
*
* Note(s)     : none.
*********************************************************************************************************
*/
static void AppTaskQue(void *p_arg)
{
  OS_ERR err;
  (void)p_arg;

  printf("====RUNNING AppTaskQue.\r\n");

  while (DEF_TRUE)
  {
    //LED1_TOGGLE;
    //OSTimeDly(1000, OS_OPT_TIME_DLY, &err);
    app_msg_t *pAppMsg = NULL;
    pAppMsg = OSQPend((OS_Q*)&gAppMsgQue, (OS_TICK)0, (OS_OPT)OS_OPT_PEND_BLOCKING, (OS_MSG_SIZE *)(sizeof(app_msg_t)), (CPU_TS *)0, (OS_ERR *)&err);
    if (err == OS_ERR_NONE && pAppMsg != NULL)
    {
      printf("AppTaskQue recive msg!\r\n");
      AppProcMsg(pAppMsg);
    }
    else
    {
      printf("AppTaskQue gAppMsgQue receive fail, err=%u!\r\n", err);
    }
  }
}

void AppNBCallback(App_Callback_Event event, uint8_t *pData, uint16_t len, uint8_t result)
{
  app_msg_t *pMsg = NULL;
  OS_ERR err;

  pMsg = (app_msg_t *)malloc(sizeof(app_msg_t));
  if (NULL != pMsg)
  {
    pMsg->src = MSG_FROM_NB;
    pMsg->pData = NULL;
    pMsg->len = 0;
  }
  else
  {
    printf("AppNBCallback malloc fail, event=%u, result=%u!\r\n", event, result);
    return;
  }

  switch (event)
  {
    case EVENT_INIT_NB:
      pMsg->msgtype = MSG_INIT_NB;
      if (TD_SUCCESS == result)
      {
        //printf("AppNBCallback NB Init success.\r\n");
        pMsg->result = APP_OK;

      }
      else
      {
        //printf("AppNBCallback NB Init fail.\r\n");
        pMsg->result = APP_ERR;
      }
      break;

    case EVENT_COAP_CR:
      pMsg->msgtype = MSG_COAP_CR;
      if (TD_SUCCESS == result)
      {
        printf("AppNBCallback create coap success.\r\n");
        pMsg->result = APP_OK;

      }
      else
      {
        printf("AppNBCallback create coap fail.\r\n");
        pMsg->result = APP_ERR;
      }
      break;

    case EVENT_COAP_ST:
      pMsg->msgtype = MSG_COAP_ST;
      if (TD_SUCCESS == result)
      {
        printf("AppNBCallback send coap success.\r\n");
        pMsg->result = APP_OK;

      }
      else
      {
        printf("AppNBCallback send coap fail.\r\n");
        pMsg->result = APP_ERR;
      }
      break;

    case EVENT_COAP_RE:
      pMsg->msgtype = MSG_COAP_RE;
      if (TD_SUCCESS == result)
      {
        printf("AppNBCallback receive coap success, len=%u.\r\n", len);
        pMsg->result = APP_OK;
        pMsg->pData = malloc(len);
        if (NULL != pMsg->pData)
        {
          memcpy(pMsg->pData, pData, len);
          pMsg->len = len;
        }
        else
        {
          printf("AppNBCallback receive coap alloc databuf fail!\r\n");
          free(pMsg);
          pMsg = NULL;
        }
      }
      else
      {
        printf("AppNBCallback receive coap fail.\r\n");
        free(pMsg);
        pMsg = NULL;
      }
      break;

    case EVENT_UDP_CR:
      pMsg->msgtype = MSG_UDP_CR;
      if (TD_SUCCESS == result)
      {
        //printf("AppNBCallback create udp success.\r\n");
        pMsg->result = APP_OK;

      }
      else
      {
        //printf("AppNBCallback create udp fail.\r\n");
        pMsg->result = APP_ERR;
      }
      break;

    case EVENT_UDP_ST:
      pMsg->msgtype = MSG_UDP_ST;
      if (TD_SUCCESS == result)
      {
        printf("AppNBCallback send udp success.\r\n");
        pMsg->result = APP_OK;

      }
      else
      {
        printf("AppNBCallback send udp fail.\r\n");
        pMsg->result = APP_ERR;
      }
      break;

    case EVENT_UDP_RE:
      pMsg->msgtype = MSG_UDP_RE;
      if (TD_SUCCESS == result)
      {
        printf("AppNBCallback receive udp success, len=%u.\r\n", len);
        pMsg->result = APP_OK;
        pMsg->pData = malloc(len);
        if (NULL != pMsg->pData)
        {
          memcpy(pMsg->pData, pData, len);
          pMsg->len = len;
        }
        else
        {
          printf("AppNBCallback receive udp alloc databuf fail!\r\n");
          free(pMsg);
          pMsg = NULL;
        }
      }
      else
      {
        printf("AppNBCallback receive udp fail.\r\n");
        free(pMsg);
        pMsg = NULL;
      }
      break;

    default:
      printf("AppNBCallback unknow event=%u.\r\n", event);
      if (NULL != pMsg)
      {
        free(pMsg);
        pMsg = NULL;
      }
      break;
  }

  if (NULL != pMsg)
  {
    OSQPost((OS_Q *)&gAppMsgQue, (void *)pMsg, (OS_MSG_SIZE)(sizeof(app_msg_t)), OS_OPT_POST_FIFO, &err);
    if (err != OS_ERR_NONE)
    {
      printf("AppNBCallback OSQPost fail, event=%u, err=%u.\r\n", event, err);
      if (NULL != pMsg->pData)
      {
        free(pMsg->pData);
        pMsg->pData = NULL;
      }
      free(pMsg);
      pMsg = NULL;
    }
  }
}

void AppProcMsgFromNB(app_msg_t *pAppMsg)
{
  uint16_t i;

  switch (pAppMsg->msgtype)
  {
    case MSG_INIT_NB:
      printf("AppProcMsgFromNB MSG_INIT_NB %s", (pAppMsg->result == APP_OK) ? "SUCCESS.\r\n" : "FAILURE!\r\n");
      if (APP_OK == pAppMsg->result)
      {
        if (TD_FAILURE == NB_CreateUdp())
        {
          printf("AppProcMsgFromNB NB_CreateUdp fail!\r\n");
        }
      }
      break;
    case MSG_COAP_CR:
      printf("AppProcMsgFromNB MSG_COAP_CR.\r\n");
      break;
    case MSG_COAP_ST:
      printf("AppProcMsgFromNB MSG_COAP_ST.\r\n");
      break;
    case MSG_UDP_CR:
      if (APP_OK == pAppMsg->result)
      {
        gAppWork = TD_TRUE;
      }
      printf("AppProcMsgFromNB MSG_UDP_CR %u.\r\n", gAppWork);
      break;
    case MSG_UDP_ST:
      printf("AppProcMsgFromNB MSG_UDP_ST.\r\n");
      break;

    case MSG_COAP_RE:
      if (APP_OK == pAppMsg->result)
      {
        printf("AppProcMsgFromNB receive coap data len=%u\r\n", pAppMsg->len);
        for (i = 0; i < pAppMsg->len; i++)
        {
          printf("0x%02x ", pAppMsg->pData[i]);
        }
        printf("\r\n");
        if (NULL != pAppMsg->pData)
        {
          free(pAppMsg->pData);
        }
      }
      break;

    case MSG_UDP_RE:
      if (APP_OK == pAppMsg->result)
      {
        printf("AppProcMsgFromNB receive udp data len=%u\r\n", pAppMsg->len);
        for (i = 0; i < pAppMsg->len; i++)
        {
          printf("0x%02x ", pAppMsg->pData[i]);
        }
        printf("\r\n");
        if (NULL != pAppMsg->pData)
        {
          free(pAppMsg->pData);
        }
      }
      break;

    default:
      break;
  }
  free(pAppMsg);
  pAppMsg = NULL;
}

void AppProcMsg(app_msg_t *pAppMsg)
{
  if (MSG_FROM_NB == pAppMsg->src)
  {
    AppProcMsgFromNB(pAppMsg);
  }
  else
  {
    free(pAppMsg);
    pAppMsg = NULL;
  }
}

