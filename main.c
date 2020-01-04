#include "lcd.h"
#include "Driver_USART.h"
#include "system_LPC17xx.h"

/* Global variables */
/* USART Driver */
extern ARM_DRIVER_USART Driver_USART2;

/* Function prototypes */
static void LCD_Dummy();
static void myUART_Thread();

int main(void)
{
    SystemInit();
    SystemCoreClockUpdate();

    /* LCD functions */
    GLCD_Init();
    GLCD_Clr();
    /* LCD_Dummy(); */

    /* USART fucntions */
    myUART_Thread();

    return (0U);
}

static void LCD_Dummy()
{
    int counterLCD = 0;
    char test_str[24] = "AAAAAAAAAAAAAAAAAAAAAAAA";
    for (counterLCD = 0; counterLCD < 8; counterLCD++)
    {
        GLCD_Print78(counterLCD, 0, test_str);
    }
}


static void myUART_Thread(void)
{
    static ARM_DRIVER_USART *USARTdrv = &Driver_USART2;;
    char cmd[2];

    /*Initialize the USART driver */
    USARTdrv->Initialize(NULL);
    /*Power up the USART peripheral */
    USARTdrv->PowerControl(ARM_POWER_FULL);
    /*Configure the USART to 4800 Bits/sec */
    USARTdrv->Control(ARM_USART_MODE_ASYNCHRONOUS |
                      ARM_USART_DATA_BITS_8 |
                      ARM_USART_PARITY_NONE |
                      ARM_USART_STOP_BITS_1 |
                      ARM_USART_FLOW_CONTROL_NONE, 9600);
     
    /* Enable Receiver and Transmitter lines */
    USARTdrv->Control(ARM_USART_CONTROL_TX, 1);
    USARTdrv->Control(ARM_USART_CONTROL_RX, 1);
 
    USARTdrv->Send("AT", 2);
     
    while (1)
    {
        USARTdrv->Receive(&cmd, 2);          /* Get byte from UART */
        if (cmd[0] == 'O')                       /* CR, send greeting  */
        {
            USARTdrv->Send("AT", 2);
        }
    }
}