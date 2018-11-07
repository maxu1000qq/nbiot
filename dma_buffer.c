#include <stdio.h>
#include <string.h>
#include "os.h"
#include "bsp/usart/Bsp_debug_usart.h"
#include "nbiot/nbiot.h"
#include "dma_buffer.h"

#define WAIT_DATA_TIMEOUT 500 // 5s

enum DMA_PAGES {
  PAGE1,
  PAGE2,
  PAGE3,
  PAGE4,
  PAGE_CNT
};

enum DMA_Buffer_Users {
  UserNBRx,
  UserNetRx,
  UserDbgRx,
  DmaBufUser_Cnt
};

typedef struct DmaBuffer_NB_Struct
{
  uint8_t buf[DMA_BUF_MAX_LEN]; // »º´æÇø
  uint16_t dataLen;
  uint32_t sTick;
  uint16_t count;
} DmaBuffer_NB_t;

typedef struct DmaBufferPage_Struct
{
  DmaBuffer_NB_t dmaBuffer[PAGE_CNT];
  uint8_t readPage;
  uint8_t writePage;
} DmaBufferPage_t;

DmaBufferPage_t gDmaBuffer[DmaBufUser_Cnt];
OS_SEM gNBDmaBufSem;
OS_SEM gNetDmaBufSem;
OS_SEM gDbgDmaBufSem;

uint8_t getWritePage(enum DMA_Buffer_Users user)
{
  return gDmaBuffer[user].writePage;
}

void ClrDmaBuffer(enum DMA_Buffer_Users user, uint8_t page)
{
  if (user >= DmaBufUser_Cnt || page >= PAGE_CNT)
  {
    printf("ClrDmaBuffer: user=%u, page=%u error!\r\n", user, page);
    return;
  }

  gDmaBuffer[user].dmaBuffer[page].count = 0;
  gDmaBuffer[user].dmaBuffer[page].dataLen = 0;
  gDmaBuffer[user].dmaBuffer[page].sTick = 0;
}

void nextWritePage(enum DMA_Buffer_Users user)
{
  uint8_t page = 0;

  gDmaBuffer[user].writePage++;
  if (gDmaBuffer[user].writePage >= PAGE_CNT)
  {
    gDmaBuffer[user].writePage = 0;
  }

  page = gDmaBuffer[user].writePage;
  if (gDmaBuffer[user].dmaBuffer[page].dataLen > 0)
  {
    printf("nextWritePage user=%u page=%u will cover!\r\n", user, page);
    ClrDmaBuffer(user, page);
  }
}

void nextReadPage(enum DMA_Buffer_Users user)
{
  gDmaBuffer[user].readPage++;
  if (gDmaBuffer[user].readPage >= PAGE_CNT)
  {
    gDmaBuffer[user].readPage = 0;
  }
}

void DmaBufferInit(void)
{
  OS_ERR err;

  OSSemCreate(&gNBDmaBufSem, "NBDma Buffer Sem", 0, &err);
  if (OS_ERR_NONE != err)
  {
    printf("DmaBufferInit OSSemCreate gNBDmaBufSem fail, err=%u!\r\n", err);
    return;
  }

  OSSemCreate(&gNetDmaBufSem, "NetDma Buffer Sem", 0, &err);
  if (OS_ERR_NONE != err)
  {
    printf("DmaBufferInit OSSemCreate gNetDmaBufSem fail, err=%u!\r\n", err);
    return;
  }

  OSSemCreate(&gDbgDmaBufSem, "DbgDma Buffer Sem", 0, &err);
  if (OS_ERR_NONE != err)
  {
    printf("DmaBufferInit OSSemCreate gDbgDmaBufSem fail, err=%u!\r\n", err);
    return;
  }

  for (uint8_t i = 0; i < DmaBufUser_Cnt; i++)
  {
    gDmaBuffer[i].readPage = 0;
    gDmaBuffer[i].writePage = 0;
    memset(gDmaBuffer[i].dmaBuffer, 0, sizeof(DmaBuffer_NB_t) * PAGE_CNT);
  }
}

uint16_t DmaBufferRead(enum DMA_Buffer_Users user, uint8_t *pBuf)
{
  OS_ERR err;
  uint8_t readPage;
  uint16_t dataLen = 0;
  DmaBuffer_NB_t *pDmaBuffer = NULL;

  if (user >= DmaBufUser_Cnt || NULL == pBuf)
  {
    printf("DmaBufferRead: user=%u or pBuf error, read none!\r\n", user);
    return 0;
  }

  // read ready page
  readPage = gDmaBuffer[user].readPage;
  if (gDmaBuffer[user].dmaBuffer[readPage].dataLen > 0) // ready to read
  {
    pDmaBuffer = &gDmaBuffer[user].dmaBuffer[readPage];
    memcpy(pBuf, pDmaBuffer->buf, pDmaBuffer->dataLen);
    dataLen = pDmaBuffer->dataLen;
    ClrDmaBuffer(user, readPage);
    nextReadPage(user);
  }

  // no check
  if (UserDbgRx == user)
  {
    return dataLen;
  }

  // check write page
  if (gDmaBuffer[user].writePage < PAGE_CNT && 0 == pDmaBuffer->dataLen && pDmaBuffer->count > 0)
  {
    uint32_t curTick = OSTimeGet(&err);
    pDmaBuffer = &gDmaBuffer[user].dmaBuffer[gDmaBuffer[user].writePage];
    uint32_t durTick = (curTick >= pDmaBuffer->sTick) ? curTick - pDmaBuffer->sTick : curTick + UINT32_MAX - pDmaBuffer->sTick;
    if (durTick > WAIT_DATA_TIMEOUT)
    {
      printf("DmaBufferRead chk: user=%u, writePage=%u, durTick=%u over time!\r\n", user, gDmaBuffer[user].writePage, durTick);
      printf("DmaBufferRead chk: count=%u, curTick=%u, sTick=%u.\r\n", pDmaBuffer->count, curTick, pDmaBuffer->sTick);
      ClrDmaBuffer(user, gDmaBuffer[user].writePage);
    }
  }

  return dataLen;
}

// for NB
uint8_t NBDmaBufferReadReady(void)
{
  OS_ERR err;

  OSSemPend(&gNBDmaBufSem, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NBDmaBufferReadReady OSSemPend fail, err=%u!\r\n", err);
    return DMA_OK;
  }

  return DMA_OK;
}

void NBDmaBufferRecvCallback(uint8_t ucValue)
{
  OS_ERR err;
  uint16_t pos = 0;
  uint8_t writePage = getWritePage(UserNBRx);

  //printf("0x%02x\r\n", ucValue);
  DmaBuffer_NB_t *pDmaBuf = &gDmaBuffer[UserNBRx].dmaBuffer[writePage];
  if (0 == pDmaBuf->count) // new frame, should begin with 0x0d0a
  {
    if (0x0d != ucValue)
    {
      printf("NBDmaBufferRecvCallback: ucValue=0x%x is not new frame flag!\r\n", ucValue);
      return;
    }

    pDmaBuf->sTick = OSTimeGet(&err);
    pDmaBuf->count++;
  }
  else if (1 == pDmaBuf->count)
  {
    if (0x0a != ucValue)
    {
      printf("NBDmaBufferRecvCallback: ucValue=0x%x is not new frame flag!\r\n", ucValue);
      ClrDmaBuffer(UserNBRx, writePage);
      return;
    }

    pDmaBuf->sTick = OSTimeGet(&err);
    pDmaBuf->count++;
  }
  else
  {
    if (pDmaBuf->count >= DMA_BUF_MAX_LEN)
    {
      printf("NBDmaBufferRecvCallback: count=%u too long, discard this frame!\r\n", pDmaBuf->count);
      ClrDmaBuffer(UserNBRx, writePage);
      return;
    }
    pos = pDmaBuf->count - 2;
    pDmaBuf->buf[pos] = ucValue;
    pDmaBuf->sTick = OSTimeGet(&err);
    pDmaBuf->count++;
    if (pos > 0) // judge tail
    {
      if (0x0d == pDmaBuf->buf[pos - 1] && 0x0a == pDmaBuf->buf[pos])
      {
        pDmaBuf->dataLen = pDmaBuf->count - 4; // head and tail cut
        if (pDmaBuf->dataLen > 0)
        {
          nextWritePage(UserNBRx);
          OSSemPost(&gNBDmaBufSem, OS_OPT_POST_1, &err);
          if (OS_ERR_NONE != err)
          {
            printf("NBDmaBufferRecvCallback OSSemPost fail, err=%u!\r\n", err);
          }
        }
        else
        {
          printf("NBDmaBufferRecvCallback: count=%u no data, discard this frame!\r\n", pDmaBuf->count);
          ClrDmaBuffer(UserNBRx, writePage);
        }
      }
    }
  }
}

void NBDmaBufferSend(uint8_t *pData, uint16_t len)
{
  USART2_SendData(pData, len);
}

uint16_t NBDmaBufferRead(uint8_t *pBuf)
{
  return DmaBufferRead(UserNBRx, pBuf);
}

// for Net
uint8_t NetDmaBufferReadReady(void)
{
  OS_ERR err;

  OSSemPend(&gNetDmaBufSem, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NetDmaBufferReadReady OSSemPend fail, err=%u!\r\n", err);
    return DMA_OK;
  }

  return DMA_OK;
}

void NetDmaBufferRecvCallback(uint8_t *pPacket, uint16_t packetLen)
{
  OS_ERR err;
  uint8_t writePage = getWritePage(UserNetRx);

  DmaBuffer_NB_t *pDmaBuf = &gDmaBuffer[UserNetRx].dmaBuffer[writePage];
  if (0 != pDmaBuf->dataLen) // new frame
  {
    printf("NetDmaBufferRecvCallback: page=%u is not new!\r\n", writePage);
    return;
  }

  pDmaBuf->sTick = OSTimeGet(&err);
  memcpy(pDmaBuf->buf, pPacket, packetLen);
  pDmaBuf->count = packetLen;
  pDmaBuf->dataLen = packetLen;

  nextWritePage(UserNetRx);
  OSSemPost(&gNetDmaBufSem, OS_OPT_POST_1, &err);
  if (OS_ERR_NONE != err)
  {
    printf("NetDmaBufferRecvCallback OSSemPost fail, err=%u!\r\n", err);
  }

  return;
}

uint16_t NetDmaBufferRead(uint8_t *pBuf)
{
  return DmaBufferRead(UserNetRx, pBuf);
}

// for Dbg
uint8_t DbgDmaBufferReadReady(void)
{
  OS_ERR err;

  OSSemPend(&gDbgDmaBufSem, 0, OS_OPT_PEND_BLOCKING, NULL, &err);
  if (OS_ERR_NONE != err)
  {
    printf("DbgDmaBufferReadReady OSSemPend fail, err=%u!\r\n", err);
    return DMA_OK;
  }

  return DMA_OK;
}

void DbgDmaBufferRecvCallback(uint8_t ucValue)
{
  OS_ERR err;
  uint8_t writePage = getWritePage(UserDbgRx);

  //printf("%c", ucValue);

  DmaBuffer_NB_t *pDmaBuf = &gDmaBuffer[UserDbgRx].dmaBuffer[writePage];
  if (pDmaBuf->count >= DMA_BUF_MAX_LEN)
  {
    printf("DbgDmaBufferRecvCallback: count=%u too long, discard this frame!\r\n", pDmaBuf->count);
    ClrDmaBuffer(UserDbgRx, writePage);
    return;
  }

  pDmaBuf->buf[pDmaBuf->count] = ucValue;
  // pDmaBuf->sTick = OSTimeGet(&err);
  pDmaBuf->count++;
  if (pDmaBuf->count > 1) // judge tail
  {
    if (0x0d == pDmaBuf->buf[pDmaBuf->count - 2] && 0x0a == pDmaBuf->buf[pDmaBuf->count - 1])
    {
      pDmaBuf->dataLen = pDmaBuf->count;
      nextWritePage(UserDbgRx);
      OSSemPost(&gDbgDmaBufSem, OS_OPT_POST_1, &err);
      if (OS_ERR_NONE != err)
      {
        printf("DbgDmaBufferRecvCallback OSSemPost fail, err=%u!\r\n", err);
      }
    }
  }
}

uint16_t DbgDmaBufferRead(uint8_t *pBuf)
{
  return DmaBufferRead(UserDbgRx, pBuf);
}

