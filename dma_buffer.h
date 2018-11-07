
#ifndef _DMA_BUFFER_H
#define _DMA_BUFFER_H

#ifdef __cplusplus
extern "C"
{
#endif

#define DMA_OK 0
#define DMA_FAIL 1
#define DMA_BUF_MAX_LEN NB_BUF_MAX_LEN

void DmaBufferInit(void);
// for nb
uint8_t NBDmaBufferReadReady(void);
void NBDmaBufferRecvCallback(uint8_t ucValue);
void NBDmaBufferSend(uint8_t *pData, uint16_t len);
uint16_t NBDmaBufferRead(uint8_t *pBuf);
// for net
uint8_t NetDmaBufferReadReady(void);
void NetDmaBufferRecvCallback(uint8_t *pPacket, uint16_t packetLen);
uint16_t NetDmaBufferRead(uint8_t *pBuf);
// for debug
uint8_t DbgDmaBufferReadReady(void);
void DbgDmaBufferRecvCallback(uint8_t ucValue);
uint16_t DbgDmaBufferRead(uint8_t *pBuf);

#ifdef __cplusplus
}
#endif

#endif

