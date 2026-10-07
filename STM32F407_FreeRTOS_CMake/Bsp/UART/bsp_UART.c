#include "bsp_UART.h"
#include "board_config.h"
#include "FreeRTOS.h"
#include "task.h"
#define DEBUG_TX_SIZE 1024U
#define DEBUG_RX_SIZE 256U
static uint8_t tx[DEBUG_TX_SIZE], rx[DEBUG_RX_SIZE];
static volatile uint16_t txHead,txTail,txCount,rxHead,rxTail,rxCount;
volatile uint32_t g_debug_tx_dropped, g_debug_rx_dropped;
void BSP_DebugInit(void)
{
    GPIO_InitTypeDef gpio={0};
    __HAL_RCC_USART1_CLK_ENABLE();
    gpio.Pin=GPIO_PIN_9|GPIO_PIN_10; gpio.Mode=GPIO_MODE_AF_PP; gpio.Pull=GPIO_PULLUP;
    gpio.Speed=GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate=GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA,&gpio);
    USART1->CR1=0; USART1->CR2=0; USART1->CR3=0;
    USART1->BRR=(HAL_RCC_GetPCLK2Freq()+57600U)/115200U;
    USART1->CR1=USART_CR1_UE|USART_CR1_TE|USART_CR1_RE;
#if COATING_DEBUG_INJECTION
    USART1->CR1|=USART_CR1_RXNEIE;
#endif
    HAL_NVIC_SetPriority(USART1_IRQn,BOARD_RTOS_ISR_PRIORITY,0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}
bool BSP_DebugWrite(const char *data, size_t length)
{
    bool accepted=false;
    taskENTER_CRITICAL();
    if (length<=DEBUG_TX_SIZE-txCount) {
        size_t i;
        for (i=0;i<length;++i) { tx[txHead]=(uint8_t)data[i]; txHead=(txHead+1U)%DEBUG_TX_SIZE; }
        txCount+=(uint16_t)length; USART1->CR1|=USART_CR1_TXEIE; accepted=true;
    } else if (g_debug_tx_dropped!=UINT32_MAX) ++g_debug_tx_dropped;
    taskEXIT_CRITICAL(); return accepted;
}
bool BSP_DebugRead(uint8_t *byte)
{
    bool result=false;
    taskENTER_CRITICAL();
    if (rxCount) { *byte=rx[rxTail]; rxTail=(rxTail+1U)%DEBUG_RX_SIZE; --rxCount; result=true; }
    taskEXIT_CRITICAL(); return result;
}
void BSP_DebugIRQ(void)
{
    uint32_t flags=USART1->SR;
    if ((flags & USART_SR_TXE) && (USART1->CR1 & USART_CR1_TXEIE)) {
        if (txCount) { USART1->DR=tx[txTail]; txTail=(txTail+1U)%DEBUG_TX_SIZE; --txCount; }
        else USART1->CR1 &= ~USART_CR1_TXEIE;
    }
    if (flags & (USART_SR_RXNE|USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) {
        uint8_t b=(uint8_t)USART1->DR;
#if COATING_DEBUG_INJECTION
        if ((flags & USART_SR_RXNE) && !(flags & (USART_SR_ORE|USART_SR_FE|USART_SR_NE|USART_SR_PE)) && rxCount<DEBUG_RX_SIZE) {
            rx[rxHead]=b; rxHead=(rxHead+1U)%DEBUG_RX_SIZE; ++rxCount;
        } else if (g_debug_rx_dropped!=UINT32_MAX) ++g_debug_rx_dropped;
#else
        (void)b;
#endif
    }
}
