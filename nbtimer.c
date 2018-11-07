#include <string.h>
#include "includes.h"
#include "tdtech.h"
#include "nbtimer.h"

#define NBTMR_TASK_STK_SIZE 128
OS_TCB NBTMRTaskProcTCB;
CPU_STK NBTMRTaskProcStk[NBTMR_TASK_STK_SIZE];

const uint32_t NBTMR_MAX_PERIOD = 5000000; // ms
const uint8_t NBTMR_MAX_COUNT = 50;

struct nb_timer_manager_stru
{
  nb_tmr_s *HeadPtr;
  nb_tmr_s *TailPtr;
  uint8_t cnt;
} gNbTmrMgr;

OS_SEM gNBTmrSem;

void task_Tmr(void *pArg);
nb_tmr_s *NBTmrdel(nb_tmr_s *pNbTmr);

void NBTmr_Init(void)
{
  OS_ERR err;

  gNbTmrMgr.HeadPtr = NULL;
  gNbTmrMgr.TailPtr = NULL;
  gNbTmrMgr.cnt = 0;

  OSSemCreate(&gNBTmrSem, "NBTmr Sem", 1, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBTmr_Init OSSemCreate gNBTmrSem fail, err=%u!\r\n", err);
    return;
  }

  OSTaskCreate(&NBTMRTaskProcTCB, "NBTimerProcTask", task_Tmr, NULL, NB_TIMER_TASK_PROC_PRIO,
             &NBTMRTaskProcStk[0], NBTMR_TASK_STK_SIZE / 10u, NBTMR_TASK_STK_SIZE,
             0u, 0u, 0, (OS_OPT_TASK_STK_CHK | OS_OPT_TASK_STK_CLR),
             &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBTmr_Init OSTaskCreate NBTimerProcTask fail, err=%u!\r\n", err);
    return;
  }
}

// period is ms
uint8_t NBTmr_Start(NBTmrHandle *pTmr, char *p_name, uint32_t period, uint8_t opt, NB_TimerCB p_callback, void *p_callback_arg)
{
  OS_ERR err;
  nb_tmr_s *pNbTailTmr = NULL;
  nb_tmr_s *pNbTmr = NULL; 

  if (NULL == p_callback)
  {
    printf("NBTmr_Start p_callback is NULL!\r\n");
    return TD_FAILURE;
  }

  if (0 == period || period > NBTMR_MAX_PERIOD)
  {
    printf("NBTmr_Start period=%u error!\r\n", period);
    return TD_FAILURE;
  }

  if (gNbTmrMgr.cnt > NBTMR_MAX_COUNT)
  {
    printf("NBTmr_Start timer count=%u overflow!\r\n", gNbTmrMgr.cnt);
    return TD_FAILURE;
  }

  pNbTmr = malloc(sizeof(nb_tmr_s));
  if (NULL == pNbTmr)
  {
    printf("NBTmr_Start malloc fail!\r\n");
    return TD_FAILURE;
  }

  pNbTmr->NamePtr = p_name;
  pNbTmr->CallbackPtr = p_callback;
  pNbTmr->CallbackPtrArg = p_callback_arg;
  pNbTmr->Period = period / 1000; // trans to seconds
  pNbTmr->Remain = pNbTmr->Period;
  pNbTmr->Opt = opt;
  pNbTmr->Act = TD_TRUE;
  pNbTmr->NextPtr = NULL;
  pNbTmr->PrevPtr = NULL;

  OSSemPend(&gNBTmrSem, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBTmr_Start OSSemPend error:%u!\r\n", err);
    free(pNbTmr);
    return TD_FAILURE;
  }

  if (NULL == gNbTmrMgr.HeadPtr)
  {
    gNbTmrMgr.HeadPtr = pNbTmr;
    gNbTmrMgr.TailPtr = pNbTmr;
  }
  else
  {
    pNbTailTmr = gNbTmrMgr.TailPtr;
    pNbTailTmr->NextPtr = pNbTmr;
    pNbTmr->PrevPtr = pNbTailTmr;
    pNbTmr->NextPtr = NULL;
    gNbTmrMgr.TailPtr = pNbTmr;
  }

  gNbTmrMgr.cnt++;
  *pTmr = pNbTmr;
  OSSemPost(&gNBTmrSem, OS_OPT_POST_1, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBTmr_Start OSSemPost error:%u!\r\n", err);
    return TD_FAILURE;
  }

  return TD_SUCCESS;
}

uint8_t NBTmr_Stop(nb_tmr_s *pTmr)
{
  if (NULL == pTmr)
  {
    printf("NBTmr_Stop pTmr is NULL!\r\n");
    return TD_FAILURE;
  }

  pTmr->Act = TD_FALSE;
	
	return TD_SUCCESS;
}

// every 1 second
void task_Tmr(void *pArg)
{
  nb_tmr_s *pTmrPtr = NULL;
  OS_ERR err;
  OS_TICK period1s = 1000; // timeTick = periodms / 100; // timer 1 tick is 100ms
  //uint32_t tickOfSecond = period1s / 100; // 1s is 10 tick
  OS_TICK ticks = 0;

  ticks = (OSCfg_TickRate_Hz * ((OS_TICK)period1s + (OS_TICK)500u / OSCfg_TickRate_Hz)) / (OS_TICK)1000u;

  printf("====RUNNING task_Timer %u.\r\n", ticks);

  while(DEF_TRUE)
  {
    OSSemPend(&gNBTmrSem, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
    if (OS_ERR_NONE != err)
    {
      printf("task_Tmr OSSemPend error:%u!\r\n", err);
      continue;
    }

    pTmrPtr = gNbTmrMgr.HeadPtr;
    while(NULL != pTmrPtr)
    {
      if (TD_FALSE == pTmrPtr->Act)
      {
        pTmrPtr = NBTmrdel(pTmrPtr);
        continue;
      }

      pTmrPtr->Remain--;
      if (0 == pTmrPtr->Remain)
      {
        pTmrPtr->Remain = pTmrPtr->Period;
        if (NULL != pTmrPtr->CallbackPtr)
        {
          pTmrPtr->CallbackPtr(pTmrPtr, pTmrPtr->CallbackPtrArg);
        }
        else
        {
          printf("task_Tmr %s callback is NULL!\r\n", pTmrPtr->NamePtr);
        }
      }
      pTmrPtr = pTmrPtr->NextPtr;
    }

    OSSemPost(&gNBTmrSem, OS_OPT_POST_1, &err);
    if (OS_ERR_NONE != err)
    {
      printf("task_Tmr OSSemPost error:%u!\r\n", err);
    }

    OSTimeDly(ticks, OS_OPT_TIME_DLY, &err);
    if (OS_ERR_NONE != err)
    {
      printf("task_Tmr OSTimeDly error:%u!\r\n", err);
    }
  }
}

nb_tmr_s *NBTmrdel(nb_tmr_s *pNbTmr)
{
  nb_tmr_s *pNbPreTmr = NULL, *pNbNextTmr = NULL;

  if (NULL == pNbTmr)
  {
    printf("NBTmrdel *pNbTmr is NULL!\r\n");
    return NULL;
  }

  pNbNextTmr = pNbTmr->NextPtr;
  pNbPreTmr = pNbTmr->PrevPtr;
  if (NULL != pNbNextTmr)
  {
    pNbNextTmr->PrevPtr = pNbPreTmr;
  }

  if (NULL != pNbPreTmr)
  {
    pNbPreTmr->NextPtr = pNbNextTmr;
  }

  if (gNbTmrMgr.HeadPtr == pNbTmr)
  {
    gNbTmrMgr.HeadPtr = pNbNextTmr;
  }
  if (gNbTmrMgr.TailPtr == pNbTmr)
  {
    gNbTmrMgr.TailPtr = pNbPreTmr;
  }

  free(pNbTmr);
  gNbTmrMgr.cnt--;

  return pNbNextTmr;
}

void NBTmrDump(void)
{
  uint8_t tmrCnt = 0;
  nb_tmr_s *pTmrPtr = NULL;

  pTmrPtr = gNbTmrMgr.HeadPtr;
  printf("NBTmrDump count %u.\r\n", gNbTmrMgr.cnt);
  while(NULL != pTmrPtr)
  {
    printf("NBTmrDump(%u) name=%s, period=%u, act=%u.\r\n", tmrCnt, pTmrPtr->NamePtr, pTmrPtr->Period, pTmrPtr->Act);
    pTmrPtr = pTmrPtr->NextPtr;
    tmrCnt++;
  }
}

