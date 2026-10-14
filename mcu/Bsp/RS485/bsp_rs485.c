#include "bsp_rs485.h"
#include "board_config.h"
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "dma_cursor.h"
#include <string.h>

#define DMA_RX_SIZE 512U
#define RX_CLEAR (DMA_HIFCR_CFEIF5|DMA_HIFCR_CDMEIF5|DMA_HIFCR_CTEIF5|DMA_HIFCR_CHTIF5|DMA_HIFCR_CTCIF5)
#define TX_CLEAR (DMA_HIFCR_CFEIF6|DMA_HIFCR_CDMEIF6|DMA_HIFCR_CTEIF6|DMA_HIFCR_CHTIF6|DMA_HIFCR_CTCIF6)
/* DMA 缓冲由链接脚本放入 0x20000000 普通 SRAM；DMA 无法访问 CCM。 */
static uint8_t rxDMA[DMA_RX_SIZE], txDMA[RTU_MAX_ADU];
static RxRing rxRing;
static UART_HandleTypeDef uart2;
static DmaCursor cursor;
static uint32_t lastActivity, lastMonitor, txStarted;
static volatile uint32_t errors;
static volatile bool channelFault, txActive;
static volatile int txResult;
static bool silenceSent;

uint32_t BSP_Micros(void) { return TIM2->CNT; }
static void DE(bool transmit)
{
    GPIO_PinState state=transmit ?
        (BOARD_RS485_DE_INACTIVE==GPIO_PIN_SET ? GPIO_PIN_RESET : GPIO_PIN_SET) :
        BOARD_RS485_DE_INACTIVE;
    HAL_GPIO_WritePin(BOARD_RS485_DE_PORT,BOARD_RS485_DE_PIN,state);
}
static void ReceiveError(uint32_t now)
{
    /* 出错立即停止接收并清记录环，交给通信任务丢帧和重启，禁止沿用可疑数据。 */
    if (errors!=UINT32_MAX) ++errors;
    channelFault=true;
    USART2->CR1 &= ~(USART_CR1_IDLEIE|USART_CR1_PEIE);
    USART2->CR3 &= ~USART_CR3_EIE;
    USART2->CR3 &= ~USART_CR3_DMAR;
    DMA1_Stream5->CR &= ~DMA_SxCR_EN;
    RxRing_Clear(&rxRing);
    (void)RxRing_Push(&rxRing,(RxRecord){now,0,RX_ERROR});
    if (__get_IPSR()) BSP_RS485_WakeFromISR();
}
static void Collect(uint32_t now)
{
    uint16_t head=(uint16_t)((DMA_RX_SIZE-DMA1_Stream5->NDTR)%DMA_RX_SIZE);
    uint16_t count=(uint16_t)((head+DMA_RX_SIZE-cursor.tail)%DMA_RX_SIZE);
    if (txActive || channelFault) { cursor.tail=head; return; }
    if (count) {
        /* 采集间隔过长时无法可靠推断字符间隔，宁可报错也不接受可能粘连的请求。 */
        uint16_t copied;
        if (Elapsed32(now,lastMonitor)>RTU_T15_US) { ReceiveError(now); return; }
        __DMB();
        if (!DmaCursor_Extract(&cursor,rxDMA,DMA_RX_SIZE,head,now,&rxRing,&copied)) {
            ReceiveError(now); return;
        }
        lastActivity=now; silenceSent=false; BSP_RS485_WakeFromISR();
    }
}
static bool StartRX(void)
{
    /* 先关闭 DMA 并有界等待，再重置地址和游标，避免重配期间读到旧缓冲位置。 */
    unsigned retries=128;
    USART2->CR3 &= ~USART_CR3_DMAR;
    DMA1_Stream5->CR &= ~DMA_SxCR_EN;
    while ((DMA1_Stream5->CR & DMA_SxCR_EN) && retries) --retries;
    if (!retries) return false;
    DMA1->HIFCR=RX_CLEAR;
    DMA1_Stream5->PAR=(uint32_t)&USART2->DR;
    DMA1_Stream5->M0AR=(uint32_t)rxDMA;
    DMA1_Stream5->NDTR=DMA_RX_SIZE;
    DMA1_Stream5->FCR=DMA_SxFCR_FEIE;
    DMA1_Stream5->CR=(4U<<DMA_SxCR_CHSEL_Pos)|DMA_SxCR_MINC|DMA_SxCR_CIRC|
        DMA_SxCR_PL_1|DMA_SxCR_HTIE|DMA_SxCR_TCIE|DMA_SxCR_TEIE|DMA_SxCR_DMEIE;
    cursor.tail=0;
    (void)USART2->SR; (void)USART2->DR;
    DMA1_Stream5->CR|=DMA_SxCR_EN;
    USART2->CR3|=USART_CR3_DMAR|USART_CR3_EIE;
    USART2->CR1|=USART_CR1_IDLEIE|USART_CR1_PEIE;
    lastMonitor=BSP_Micros(); lastActivity=lastMonitor; silenceSent=true;
    return true;
}
void BSP_RS485_Init(void)
{
    GPIO_InitTypeDef gpio={0};
    uint32_t timerClock=2U*HAL_RCC_GetPCLK1Freq();
    __HAL_RCC_USART2_CLK_ENABLE(); __HAL_RCC_DMA1_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE(); __HAL_RCC_TIM3_CLK_ENABLE();
    gpio.Pin=GPIO_PIN_2|GPIO_PIN_3; gpio.Mode=GPIO_MODE_AF_PP; gpio.Pull=GPIO_PULLUP;
    gpio.Speed=GPIO_SPEED_FREQ_VERY_HIGH; gpio.Alternate=GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA,&gpio);
    uart2.Instance=USART2; uart2.Init.BaudRate=115200;
    /* STM32 的 9B 字长包含校验位，配偶校验后实际为 8 数据位、偶校验、1 停止位。 */
    uart2.Init.WordLength=UART_WORDLENGTH_9B; uart2.Init.StopBits=UART_STOPBITS_1;
    uart2.Init.Parity=UART_PARITY_EVEN; uart2.Init.Mode=UART_MODE_TX_RX;
    uart2.Init.HwFlowCtl=UART_HWCONTROL_NONE; uart2.Init.OverSampling=UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&uart2)!=HAL_OK) Error_Handler();
    /* TIM2 提供可回绕的 1 MHz 时间戳，TIM3 每 50 us 采集接收进度和静默记录。 */
    TIM2->PSC=timerClock/1000000U-1U; TIM2->ARR=UINT32_MAX;
    TIM2->EGR=TIM_EGR_UG; TIM2->CR1=TIM_CR1_CEN;
    TIM3->PSC=timerClock/1000000U-1U; TIM3->ARR=RX_MONITOR_US-1U;
    TIM3->EGR=TIM_EGR_UG; TIM3->SR=0; TIM3->DIER=TIM_DIER_UIE;
    RxRing_Init(&rxRing); DE(false);
    if (!StartRX()) Error_Handler();
    /* 这些中断会调用 FreeRTOS FromISR 接口，优先级必须满足内核的调用限制。 */
    HAL_NVIC_SetPriority(USART2_IRQn,BOARD_RTOS_ISR_PRIORITY,0);
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn,BOARD_RTOS_ISR_PRIORITY,0);
    HAL_NVIC_SetPriority(DMA1_Stream6_IRQn,BOARD_RTOS_ISR_PRIORITY,0);
    HAL_NVIC_SetPriority(TIM3_IRQn,BOARD_RTOS_ISR_PRIORITY,0);
    HAL_NVIC_EnableIRQ(USART2_IRQn); HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
    HAL_NVIC_EnableIRQ(DMA1_Stream6_IRQn); HAL_NVIC_EnableIRQ(TIM3_IRQn);
    TIM3->CR1=TIM_CR1_CEN;
}
void BSP_RS485_MonitorIRQ(void)
{
    uint32_t now;
    if (!(TIM3->SR & TIM_SR_UIF)) return;
    TIM3->SR=0; now=BSP_Micros(); Collect(now); lastMonitor=now;
    /* 同 RTU 层一起预留在途字符和监视误差，每段静默只发布一次。 */
    if (!txActive && !channelFault && !silenceSent &&
        Elapsed32(now,lastActivity)>=RTU_T35_US+RTU_CHAR_US+RX_MONITOR_US) {
        if (!RxRing_Push(&rxRing,(RxRecord){now,0,RX_SILENCE})) ReceiveError(now);
        else { silenceSent=true; BSP_RS485_WakeFromISR(); }
    }
}
void BSP_RS485_RxDMAIRQ(void)
{
    uint32_t flags=DMA1->HISR; DMA1->HIFCR=RX_CLEAR;
    /* 半满和全满同时挂起意味着服务可能滞后，按接收错误处理，避免环回覆盖。 */
    if ((flags & (DMA_HISR_TEIF5|DMA_HISR_DMEIF5|DMA_HISR_FEIF5)) ||
        (flags & (DMA_HISR_HTIF5|DMA_HISR_TCIF5))==(DMA_HISR_HTIF5|DMA_HISR_TCIF5))
        ReceiveError(BSP_Micros());
    else Collect(BSP_Micros());
}
static void TxFailure(void)
{
    USART2->CR3 &= ~USART_CR3_DMAT; USART2->CR1 &= ~USART_CR1_TCIE;
    DMA1_Stream6->CR &= ~DMA_SxCR_EN;
    txActive=false; txResult=-1; DE(false); ReceiveError(BSP_Micros());
}
void BSP_RS485_TxDMAIRQ(void)
{
    uint32_t flags=DMA1->HISR; DMA1->HIFCR=TX_CLEAR;
    if (flags & (DMA_HISR_TEIF6|DMA_HISR_DMEIF6|DMA_HISR_FEIF6)) TxFailure();
    else if (flags & DMA_HISR_TCIF6) {
        USART2->CR3 &= ~USART_CR3_DMAT;
        USART2->CR1 |= USART_CR1_TCIE; /* DMA 完成后仍保持 DE，等待 USART 发完最后停止位。 */
    }
}
void BSP_RS485_UartIRQ(void)
{
    uint32_t status=USART2->SR;
    if (status & (USART_SR_PE|USART_SR_FE|USART_SR_NE|USART_SR_ORE)) {
        (void)USART2->DR; ReceiveError(BSP_Micros());
    } else if (status & USART_SR_IDLE) {
        Collect(BSP_Micros()); (void)USART2->DR;
    }
    if ((status & USART_SR_TC) && (USART2->CR1 & USART_CR1_TCIE)) {
        /* TC 才是线上的发送完成；此时释放 DE，并跳过发送期间可能收到的本机回显。 */
        USART2->CR1 &= ~USART_CR1_TCIE;
        txActive=false; txResult=1; DE(false);
        (void)USART2->SR; (void)USART2->DR;
        cursor.tail=(uint16_t)((DMA_RX_SIZE-DMA1_Stream5->NDTR)%DMA_RX_SIZE);
        lastActivity=BSP_Micros(); silenceSent=true;
        BSP_RS485_WakeFromISR();
    }
}
bool BSP_RS485_Read(RxRecord *record)
{
    bool result; taskENTER_CRITICAL(); result=RxRing_Pop(&rxRing,record); taskEXIT_CRITICAL();
    return result;
}
uint32_t BSP_RS485_Errors(void)
{
    uint32_t count; taskENTER_CRITICAL(); count=errors; errors=0; taskEXIT_CRITICAL(); return count;
}
bool BSP_RS485_Healthy(void) { return !channelFault; }
void BSP_RS485_Recover(void)
{
    taskENTER_CRITICAL();
    if (channelFault && !txActive && StartRX()) channelFault=false;
    taskEXIT_CRITICAL();
}
bool BSP_RS485_Send(const uint8_t *data, size_t length)
{
    bool result=false;
    if (!length || length>RTU_MAX_ADU) return false;
    taskENTER_CRITICAL();
    if (!txActive && !channelFault && !(DMA1_Stream6->CR & DMA_SxCR_EN)) {
        /* 复制到专用发送缓冲，DMA 期间不依赖调用方数据的生命周期。 */
        memcpy(txDMA,data,length); DMA1->HIFCR=TX_CLEAR;
        DMA1_Stream6->PAR=(uint32_t)&USART2->DR; DMA1_Stream6->M0AR=(uint32_t)txDMA;
        DMA1_Stream6->NDTR=(uint32_t)length; DMA1_Stream6->FCR=DMA_SxFCR_FEIE;
        DMA1_Stream6->CR=(4U<<DMA_SxCR_CHSEL_Pos)|DMA_SxCR_MINC|DMA_SxCR_DIR_0|
            DMA_SxCR_PL_1|DMA_SxCR_TCIE|DMA_SxCR_TEIE|DMA_SxCR_DMEIE;
        txActive=true; txResult=0; txStarted=HAL_GetTick(); DE(true);
        __HAL_UART_CLEAR_FLAG(&uart2,UART_FLAG_TC);
        __DMB(); DMA1_Stream6->CR|=DMA_SxCR_EN; USART2->CR3|=USART_CR3_DMAT;
        result=true;
    }
    taskEXIT_CRITICAL(); return result;
}
int BSP_RS485_TxResult(void)
{
    int result;
    taskENTER_CRITICAL();
    /* 50 ms 内未完成则关闭发送并报告失败，通信任务不会永久等待硬件。 */
    if (txActive && Elapsed32(HAL_GetTick(),txStarted)>=50U) TxFailure();
    result=txResult; taskEXIT_CRITICAL(); return result;
}
