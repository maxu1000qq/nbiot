#ifndef __nbtimer_H
#define __nbtimer_H

#include "stm32f4xx.h"

#ifdef __cplusplus
 extern "C" {
#endif

typedef void (*NB_TimerCB)(void *, void *);

typedef struct nb_tmr_stru nb_tmr_s;
struct nb_tmr_stru
{
  char *NamePtr; /* Name to give the timer */
  NB_TimerCB CallbackPtr; /* Function to call when timer expires */
  void *CallbackPtrArg; /* Argument to pass to function when timer expires */
  uint32_t Period; /* Period to repeat timer */
  uint32_t Remain; /* Remain timer */
  uint8_t Opt; /* Options */
  uint8_t Act; // action status
  nb_tmr_s *NextPtr;
  nb_tmr_s *PrevPtr;
};

typedef nb_tmr_s *NBTmrHandle;
void NBTmr_Init(void);
uint8_t NBTmr_Start(NBTmrHandle *pTmr, char *p_name, uint32_t period, uint8_t opt, NB_TimerCB p_callback, void *p_callback_arg);
uint8_t NBTmr_Stop(nb_tmr_s *pTmr);
void NBTmrDump(void);

#ifdef __cplusplus
}
#endif
#endif

