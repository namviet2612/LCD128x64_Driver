#include "lcd.h"
#include "Driver_USART.h"
#include "system_LPC17xx.h"
#include "timer.h"
#include "adc.h"
#include "math.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

/* Global variables */
/* Format message: 0x55-0xAA-d[0]-d[1]-...[d9]-d[10]-'\r\n' */
/* In total: First two bytes (0x55, 0xAA) + 11 channels * 2 bytes for each channels + Last two bytes (\r\n) = 26 bytes */
#define BUFFER_LENGTH 26U
#define NR_ADC_CHANNELS 6U
#define NR_COUNTERS 5U
#define FIRST_ADC_BUFFER_INDEX 2U
#define FIRST_COUNTER_BUFFER_INDEX (4U + (NR_ADC_CHANNELS - 1) * 2U)
char dummy_data[1];
static bool USART_TX_Complete[4];
//static bool USART_RX_Complete[2];
//static bool USART_RX_Timeout[2];
static char buffer_data[BUFFER_LENGTH];
static uint32_t counterP0[6];
static bool delay_trigger_tim3 = false;
/* USART Driver */
extern ARM_DRIVER_USART Driver_USART2;
extern ARM_DRIVER_USART Driver_USART1;
extern ARM_DRIVER_USART Driver_USART0;

#define USART_TIMEOUT       100000

#define E_OK                true
#define E_NOT_OK            false

#define CONSOLE_LOG(log, line) {\
    GLCD_Print78(line, 0, log);\
    }

#define WDT_WDMOD_WDEN  0
#define WDT_WDMOD_WDRESET  1

#define LPC_GPIOINT_IO0_INTERRUPT_STATUS_P04_MASKED 0x10
#define LPC_GPIOINT_IO0_INTERRUPT_STATUS_P05_MASKED 0x20
#define LPC_GPIOINT_IO0_INTERRUPT_STATUS_P06_MASKED 0x40
#define LPC_GPIOINT_IO0_INTERRUPT_STATUS_P07_MASKED 0x80
#define LPC_GPIOINT_IO0_INTERRUPT_STATUS_P08_MASKED 0x100
#define LPC_GPIOINT_IO0_INTERRUPT_STATUS_P09_MASKED 0x200


/* Function prototypes */
static void LCD_Dummy(void);
static void USART_Init(ARM_DRIVER_USART *USARTdrv, ARM_USART_SignalEvent_t pUSART_Callback);
static void USART_2_Callback(uint32_t event);
static void USART_1_Callback(uint32_t event);
static void USART_0_Callback(uint32_t event);
static bool USART_SendCommand(ARM_DRIVER_USART *USARTdrv, int instance, const char *data, int SendCmdLength);
void Timer0_Notification(void);
void Timer1_Notification(void);
void Timer2_Notification(void);
void Timer3_Notification(void);
void Timer3_Delay_ms (uint16_t ms);
void WDG_Init(uint16_t msTimer);
void WDG_Reload(void);
void WDG_Disable(void);
void GPIO_InterruptInit(void);

void GPIO_InterruptInit(void)
{
    /* Interrupt enable for input capture pins */
    LPC_GPIOINT->IO0IntEnF |= 0x3F0; /* Input capture on falling edge on P0.4 to P0.9 */
    LPC_SC->EXTMODE |= (1 << 3); /* Edge sensitive on EINT3 */
    LPC_SC->EXTPOLAR &= (~(1 << 3)); /* EINT3 is falling-edge sensitive */
    NVIC_ClearPendingIRQ(EINT3_IRQn);  /* avoid extraneous IRQ on restart */
    NVIC_EnableIRQ(EINT3_IRQn);
}

void EINT3_IRQHandler(void)
{
    uint32_t regIO0IntFallingStatus = LPC_GPIOINT->IO0IntStatF;
    uint32_t regIO0IntFallingEnable = LPC_GPIOINT->IO0IntEnF;

    LPC_SC->EXTINT |= 0x8;  /* Clear Interrupt Flag on EXTINT*/
    LPC_GPIOINT->IO0IntClr |= regIO0IntFallingEnable; /* Clear Interrupt flags on GPIOINT */

    if (regIO0IntFallingStatus & LPC_GPIOINT_IO0_INTERRUPT_STATUS_P04_MASKED)
    {
        /* counterP0[0]++; */
    }
    else if (regIO0IntFallingStatus & LPC_GPIOINT_IO0_INTERRUPT_STATUS_P05_MASKED)
    {
        counterP0[0]++;
    }
    else if (regIO0IntFallingStatus & LPC_GPIOINT_IO0_INTERRUPT_STATUS_P06_MASKED)
    {
        counterP0[1]++;
    }
    else if (regIO0IntFallingStatus & LPC_GPIOINT_IO0_INTERRUPT_STATUS_P07_MASKED)
    {
        counterP0[2]++;
    }
    else if (regIO0IntFallingStatus & LPC_GPIOINT_IO0_INTERRUPT_STATUS_P08_MASKED)
    {
        counterP0[3]++;
    }
    else if (regIO0IntFallingStatus & LPC_GPIOINT_IO0_INTERRUPT_STATUS_P09_MASKED)
    {
        counterP0[4]++;
    }
    else
    {
        /* Spurious flag, do nothing */
    }
}

void Timer3_Delay_ms(uint16_t ms)
{
    uint32_t us;

    us = ms * 1000;
    delay_trigger_tim3 = false;

    TIMER_SetTime(3, us);
    TIMER_Start(3);
    while (delay_trigger_tim3 != true);
    TIMER_Stop(3);
}

void Timer0_Notification(void)
{
    uint8_t i;
    uint16_t u16AdcRawData;
    /* User code */
    /* buffer from #2 to #13 stores the ADC values - two buffers for one ADC value */
    for (i = 0; i < NR_ADC_CHANNELS; i++)
    {
        u16AdcRawData = ADC_GetAdcValue(i);
        buffer_data[FIRST_ADC_BUFFER_INDEX + i * 2] = (u16AdcRawData & 0xFF00) >> 8;
        buffer_data[FIRST_ADC_BUFFER_INDEX + i * 2 + 1] = (u16AdcRawData & 0xFF);
    }
    /* buffer from #14 to #23 stores the counter values - two buffers for one counter value */
    for (i = 0; i < NR_COUNTERS; i++)
    {
        buffer_data[FIRST_COUNTER_BUFFER_INDEX + i * 2] = (counterP0[i] & 0xFF00) >> 8;
        buffer_data[FIRST_COUNTER_BUFFER_INDEX + i * 2 + 1] = (counterP0[i] & 0xFF);
    }

    USART_SendCommand(&Driver_USART0, 0, buffer_data, BUFFER_LENGTH);
    /* Clear counter buffer for water sensor - index #2*/
    counterP0[2] = 0;
}

void Timer1_Notification(void)
{
    /* User code */
}

void Timer2_Notification(void)
{
    /* User code */
    WDG_Reload();
}

void Timer3_Notification(void)
{
    /* User code */
    if (delay_trigger_tim3 == false)
    {
        delay_trigger_tim3 = true;
    }
}

int main(void)
{
    bool returnValue;

    SystemInit();
    SystemCoreClockUpdate();

    /* WDG_Disable(); */
    //WDG_Init(10000);

    /* USART fucntions */
    USART_Init(&Driver_USART0, USART_0_Callback);

    /* LCD functions */
    GLCD_Init();
    GLCD_Clr();
    /* Timer3_Delay_ms(500); */
    /* LCD_Dummy(); */
    GLCD_Print78(0, 0, "Bo dieu khien trung tam");
    GLCD_Print78(1, 0, "Mang cam bien thuy dien");

    ADC_Init();
    GPIO_InterruptInit();

    /* Buffer init */
    buffer_data[0] = 0x55;
    buffer_data[1] = 0xAA;
    buffer_data[BUFFER_LENGTH - 2] = '\r';
    buffer_data[BUFFER_LENGTH - 1] = '\n';

    /* Timer0 generates 500ms period task */
    TIMER_Init(0,500000);                  /* Configure timer0 to generate 500ms(500000us) delay */
    TIMER_AttachInterrupt(0,Timer0_Notification);  /* myTimerIsr_0 will be called by TIMER0_IRQn */
    TIMER_Start(0);

    /* Timer1 used for setting USART MODBUS RX Timeout */
    /* With BDR=9600 => T= 0.104ms * 10 bits = 1.04ms for each character. */
    TIMER_Init(1,500000);                  /* Configure timer1 to generate 500ms(500000us) delay */
    TIMER_AttachInterrupt(1,Timer1_Notification);  /* myTimerIsr_1 will be called by TIMER1_IRQn */

    /* Timer2 used for set 5s counter to WDG feeding */
    TIMER_Init(2,5000000);                  /* Configure timer2 to generate 5s(5000000us) delay */
    TIMER_AttachInterrupt(2,Timer2_Notification);  /* myTimerIsr_2 will be called by TIMER2_IRQn */
    TIMER_Start(2);

    /* Timer3 used for handle SIM800 delay */
    TIMER_Init(3,2000000);                  /* Configure timer3 to generate 2s(2000000us) delay */
    TIMER_AttachInterrupt(3,Timer3_Notification);  /* myTimerIsr_3 will be called by TIMER3_IRQn */

    while (1)
    {
        returnValue = E_OK;
        if (returnValue == E_NOT_OK)
        {
            break;
        }
    }

    return (0U);
}

/* For testing LCD */
static void LCD_Dummy(void)
{
    int counterLCD = 0;
    char test_str[24] = "OKOKOKOKOKOKOKOKOKOKOKOK";
    for (counterLCD = 0; counterLCD < 8; counterLCD++)
    {
        GLCD_Print78(counterLCD, 0, test_str);
    }
}

static void USART_Init(ARM_DRIVER_USART *USARTdrv, ARM_USART_SignalEvent_t pUSART_Callback)
{
    /*Initialize the USART driver */
    USARTdrv->Initialize(pUSART_Callback);
    /*Power up the USART peripheral */
    USARTdrv->PowerControl(ARM_POWER_FULL);
    /*Configure the USART to 9600 Bits/sec */
    USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS |
                          ARM_USART_DATA_BITS_8 |
                          ARM_USART_PARITY_NONE |
                          ARM_USART_STOP_BITS_1 |
                          ARM_USART_FLOW_CONTROL_NONE,
                      115200);

    /* Enable Receiver and Transmitter lines */
    USARTdrv->Control(ARM_USART_CONTROL_TX, 1);
    USARTdrv->Control(ARM_USART_CONTROL_RX, 1);
}

static void USART_2_Callback(uint32_t event)
{
    if (event & ARM_USART_EVENT_SEND_COMPLETE)
    {
        USART_TX_Complete[2] = true;
    }

    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE)
    {
        //User code
    }
}

static void USART_1_Callback(uint32_t event)
{
    if (event & ARM_USART_EVENT_SEND_COMPLETE)
    {
        USART_TX_Complete[1] = true;
    }

    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE)
    {
        //User code
    }
}

static void USART_0_Callback(uint32_t event)
{
    if (event & ARM_USART_EVENT_SEND_COMPLETE)
    {
        USART_TX_Complete[0] = true;
    }

    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE)
    {
        //User code
    }
}

static bool USART_SendCommand(ARM_DRIVER_USART *USARTdrv, int instance, const char *data, int SendCmdLength)
{
    uint32_t tx_timeout = USART_TIMEOUT;

    USARTdrv->Send(data, SendCmdLength);
    while ((USART_TX_Complete[instance] != true) && (tx_timeout > 0))
    {
        tx_timeout--;
    }
    if (tx_timeout == 0)
    {
        return ((bool)E_NOT_OK);
    }

    return ((bool)E_OK);
}

void WDG_Init(uint16_t msTimer)
{
    /* Using IRC 4MHz, pre-scaler by 4 => WDG CLK = 1MHz */
    /* Set wd Counter  */
    LPC_WDT->WDTC = (uint32_t)(msTimer) * 1000;
    LPC_WDT->WDMOD |= 0x03;
    /* Start WDG by writing 0xaa then 0x55 followed */
    LPC_WDT->WDFEED = 0xAA;
    LPC_WDT->WDFEED = 0x55;
}

void WDG_Reload(void)
{
    /* Start WDG by writing 0xaa then 0x55 followed will reload counter */
    LPC_WDT->WDFEED = 0xAA;
    LPC_WDT->WDFEED = 0x55;
}

void WDG_Disable(void)
{
    LPC_WDT->WDMOD = 0x00;
}
