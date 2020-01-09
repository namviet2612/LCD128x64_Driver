#include "lcd.h"
#include "Driver_USART.h"
#include "system_LPC17xx.h"
#include "timer.h"

/* Global variables */
char cmd[20];
static bool USART_TX_Complete = false;
static bool USART_RX_Complete = false;
static bool USART_RX_Timeout = false;
/* USART Driver */
extern ARM_DRIVER_USART Driver_USART2;

/* Macro and define */
#define SIM800A_PING        "AT\r\n"
#define SIM800A_PING_LENGTH     4

#define SIM800A_ECHO_OFF    "ATE[0]\r\n"
#define SIM800A_ECHO_OFF_LENGTH     8

#define SIM800A_ECHO_ON     "ATE[1]\r\n"
#define SIM800A_ECHO_ON_LENGTH      8

#define SIM800A_SET_BAUD_9600    "AT+IPR=[9600]\r\n"
#define SIM800A_SET_BAUD_9600_LENGTH      16

#define OK_CR_LF            5    /* "\nOK\r\n" */

#define USART_TIMEOUT       100000

#define E_OK                true
#define E_NOT_OK            false

#define CONSOLE_LOG(log, line) {\
    GLCD_Print78(line, 0, log);\
    }

#define RESET_ALL_PARAMETER() {\
    USART_TX_Complete = false;\
    USART_RX_Complete = false;\
    USART_RX_Timeout = false;\
    for (int i = 0; i < 20; i++) {\
        cmd[i] = 0x00; }\
    }

/* Function prototypes */
static void LCD_Dummy(void);
static void USART_Init(ARM_DRIVER_USART *USARTdrv);
static void USART_Callback(uint32_t event);
bool USART_SIM800_SendCommand(ARM_DRIVER_USART *USARTdrv, char *data, int length);
void USART_SIM800_VerifyReceivedData(char *data, int SendCmdLength);
void Timer0_Notification(void);

void Timer0_Notification(void)
{
    /* User code */
    TIMER_Stop(0);
    GLCD_Clr();
    CONSOLE_LOG("Receive Data from SIM800A not success", 0);
    USART_RX_Timeout = true;
}

int main(void)
{
    bool returnValue;
    static ARM_DRIVER_USART *currentUSARTdrv = &Driver_USART2;

    SystemInit();
    SystemCoreClockUpdate();

    /* LCD functions */
    GLCD_Init();
    GLCD_Clr();
    LCD_Dummy();

    /* USART fucntions */
    USART_Init(currentUSARTdrv);

    /* Timer function */
    /* Timer0 used for setting USART RX Timeout */
    /* With BDR=9600 => T= 0.104ms * 10 bits for each symbol. */
    TIMER_Init(0,50000);                  /* Configure timer0 to generate 50ms(50000us) delay */
    TIMER_AttachInterrupt(0,Timer0_Notification);  /* myTimerIsr_0 will be called by TIMER0_IRQn */

    returnValue = USART_SIM800_SendCommand(currentUSARTdrv, SIM800A_ECHO_OFF, SIM800A_ECHO_OFF_LENGTH);

    if (returnValue == E_OK)
    {  
        USART_SIM800_VerifyReceivedData(cmd, SIM800A_ECHO_OFF_LENGTH);
        RESET_ALL_PARAMETER();
    }

    return (0U);
}

static void LCD_Dummy(void)
{
    int counterLCD = 0;
    char test_str[24] = "OKOKOKOKOKOKOKOKOKOKOKOK";
    for (counterLCD = 0; counterLCD < 8; counterLCD++)
    {
        GLCD_Print78(counterLCD, 0, test_str);
    }
}

static void USART_Init(ARM_DRIVER_USART *USARTdrv)
{
    /*Initialize the USART driver */
    USARTdrv->Initialize(USART_Callback);
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

static void USART_Callback(uint32_t event)
{
    if (event & ARM_USART_EVENT_SEND_COMPLETE)
    {
        USART_TX_Complete = true;
    }

    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE)
    {
        TIMER_Stop(0);
        USART_RX_Complete = true;
    }
}

bool USART_SIM800_SendCommand(ARM_DRIVER_USART *USARTdrv, char *data, int SendCmdLength)
{
    uint32_t tx_timeout = USART_TIMEOUT;
    uint8_t rx_length;

    USARTdrv->Send(data, SendCmdLength);
    while ((USART_TX_Complete != true) && (tx_timeout > 0))
    {
        tx_timeout--;
    }
    if (tx_timeout == 0)
    {
        GLCD_Clr();
        CONSOLE_LOG("Send Command not success", 0);
        CONSOLE_LOG(data, 1);
        return ((bool)E_NOT_OK);
    }

    rx_length = SendCmdLength + OK_CR_LF;
    USARTdrv->Receive(&cmd, rx_length);
    /* Start timer0 for trigger timeout if occurs */
    TIMER_Start(0);

    return ((bool)E_OK);
}

void USART_SIM800_VerifyReceivedData(char *data, int SendCmdLength)
{
    while (USART_RX_Complete != true)
    {
        if (USART_RX_Timeout == true)
        {
            break;
        }
    }
    /* Received data should be a copied of sent command, append "\nOK\r\n" */
    /* Example: Send cmd "AT\r\n" (4 characters) */
    /*          Receive cmd "AT\r\n\nOK\r\n" (9 characters) */
    if (USART_RX_Complete == true)
    {
        if (data[SendCmdLength+1] != 'O')
        {
            GLCD_Clr();
            CONSOLE_LOG("Error Data from SIM800A", 0);
            CONSOLE_LOG(cmd, 1);
        }
        else
        {
            GLCD_Clr();
            CONSOLE_LOG("SIM800A was responsed OK!", 0);
            CONSOLE_LOG(cmd, 1);
        }
    }
}