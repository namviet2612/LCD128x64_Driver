#include "lcd.h"
#include "Driver_USART.h"
#include "system_LPC17xx.h"
#include "timer.h"
#include "math.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

/* Global variables */
char dummy_data[1];
static bool USART_TX_Complete[2];
//static bool USART_RX_Complete[2];
//static bool USART_RX_Timeout[2];
static bool delay_trigger_tim3 = false;
/* USART Driver */
extern ARM_DRIVER_USART Driver_USART2;
extern ARM_DRIVER_USART Driver_USART1;

#define USART_TIMEOUT       100000

#define E_OK                true
#define E_NOT_OK            false

#define CONSOLE_LOG(log, line) {\
    GLCD_Print78(line, 0, log);\
    }

#define WDT_WDMOD_WDEN  0
#define WDT_WDMOD_WDRESET  1

/* Function prototypes */
static void LCD_Dummy(void);
static void USART_Init(ARM_DRIVER_USART *USARTdrv, ARM_USART_SignalEvent_t pUSART_Callback);
static void USART_2_Callback(uint32_t event);
static void USART_1_Callback(uint32_t event);
static bool USART_SendCommand(ARM_DRIVER_USART *USARTdrv, int instance, const char *data, int SendCmdLength);
void Timer0_Notification(void);
void Timer1_Notification(void);
void Timer2_Notification(void);
void Timer3_Notification(void);
void Delay_ms (uint16_t ms);
void WDG_Init(uint16_t msTimer);
void WDG_Reload(void);
void WDG_Disable(void);

void Delay_ms(uint16_t ms)
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
    /* User code */
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

    WDG_Disable();
    /* WDG_Init(10000); */

    /* USART fucntions */
    USART_Init(&Driver_USART2, USART_2_Callback);
    USART_Init(&Driver_USART1, USART_1_Callback);

    /* Timer function */
    TIMER_Init(0,500000);                  /* Configure timer0 to generate 500ms(500000us) delay */
    TIMER_AttachInterrupt(0,Timer0_Notification);  /* myTimerIsr_0 will be called by TIMER0_IRQn */

    /* Timer1 used for setting USART MODBUS RX Timeout */
    /* With BDR=9600 => T= 0.104ms * 10 bits = 1.04ms for each character. */
    TIMER_Init(1,500000);                  /* Configure timer1 to generate 500ms(500000us) delay */
    TIMER_AttachInterrupt(1,Timer1_Notification);  /* myTimerIsr_1 will be called by TIMER1_IRQn */

    /* Timer2 used for set 5s counter */
    TIMER_Init(2,5000000);                  /* Configure timer2 to generate 5s(5000000us) delay */
    TIMER_AttachInterrupt(2,Timer2_Notification);  /* myTimerIsr_2 will be called by TIMER2_IRQn */
    TIMER_Start(2);

    /* Timer3 used for handle SIM800 delay */
    TIMER_Init(3,2000000);                  /* Configure timer3 to generate 2s(2000000us) delay */
    TIMER_AttachInterrupt(3,Timer3_Notification);  /* myTimerIsr_3 will be called by TIMER3_IRQn */

    /* LCD functions */
    GLCD_Init();
    GLCD_Clr();
    Delay_ms(500);
    LCD_Dummy();
    /* GLCD_Print78(0, 0, "Bo dieu khien trung tam");
    GLCD_Print78(1, 0, "Dang khoi tao..."); */

    while (1)
    {
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
                      9600);

    /* Enable Receiver and Transmitter lines */
    USARTdrv->Control(ARM_USART_CONTROL_TX, 1);
    USARTdrv->Control(ARM_USART_CONTROL_RX, 1);
}

static void USART_2_Callback(uint32_t event)
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
