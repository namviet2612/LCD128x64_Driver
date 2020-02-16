#include "lcd.h"
#include "Driver_USART.h"
#include "system_LPC17xx.h"
#include "timer.h"
#include "math.h"
#include "string.h"
#include "stdlib.h"
#include "stdio.h"

/* Global variables */
char cmd[30];
char modbus_buffer[30];
static bool USART_TX_Complete[2];
static bool USART_RX_Complete[2];
static bool USART_RX_Timeout[2];
static bool MODBUS_Trigger = false;
static uint8_t Display_Blink = 0;
static bool delay_trigger_tim3 = false;
/* USART Driver */
extern ARM_DRIVER_USART Driver_USART2;
extern ARM_DRIVER_USART Driver_USART1;

typedef struct ValueToGet {
    uint8_t id;
    uint32_t RawVoltage;
    float ConvertedVoltage;
    uint32_t RawAmpe;
    float ConvertedAmpe;
    uint32_t RawPower;
    float ConvertedPower;
} ValueToGet;

/* Macro and define */
#define SIM800A_PING        "AT\r\n"
#define SIM800A_PING_LENGTH     4

#define SIM800A_ECHO_OFF    "ATE0\r\n"
#define SIM800A_ECHO_OFF_LENGTH     6

#define SIM800A_ECHO_ON     "ATE1\r\n"
#define SIM800A_ECHO_ON_LENGTH      6

#define SIM800A_SET_BAUD_9600    "AT+IPR=9600\r\n"
#define SIM800A_SET_BAUD_9600_LENGTH      13

#define SIM800A_TEXT_MODE    "AT+CMGF=1\r\n"
#define SIM800A_TEXT_MODE_LENGTH      11

#define SIM800A_MAKE_CALL   "ATD"
#define SIM800A_MAKE_CALL_END_OF_LINE     ";\r\n"
#define SIM800A_MAKE_CALL_LENGTH      16

#define SIM800A_END_CALL   "ATH"
#define SIM800A_END_CALL_LENGTH      3

#define SIM800A_MAKE_SMS    "AT+CMGS=\""
#define SIM800A_MAKE_SMS_END_OF_LINE        "\"\r\n"
#define SIM800A_MAKE_SMS_LENGTH      22

#define OK_CR_LF            6    /* "\r\nOK\r\n" */

#define USART_TIMEOUT       100000

#define E_OK                true
#define E_NOT_OK            false

#define CONSOLE_LOG(log, line) {\
    GLCD_Print78(line, 0, log);\
    }

#define RESET_SIM800_PARAMETER() {\
    USART_TX_Complete[0] = false;\
    USART_RX_Complete[0] = false;\
    USART_RX_Timeout[0] = false;\
    for (int i = 0; i < 30; i++) {\
        cmd[i] = 0x00; }\
    }

#define RESET_MODBUS_PARAMETER() {\
    USART_TX_Complete[1] = false;\
    USART_RX_Complete[1] = false;\
    USART_RX_Timeout[1] = false;\
    for (int i = 0; i < 30; i++) {\
        modbus_buffer[i] = 0x00; }\
    }

#define RELAY0  2   /* P0.2 */
#define RELAY1  3   /* P0.3 */
#define RELAY_PIN_INIT() LPC_GPIO(PORT_0)->DIR |= (1UL << RELAY0) | (1UL << RELAY1)
/* RELAY_CLOSE: dong ro-le (tich cuc muc thap), RELAY_OPEN: ngat ro-le (muc cao) */
#define RELAY_CLOSE(pin) LPC_GPIO(PORT_0)->CLR |= (1UL << pin)
#define RELAY_OPEN(pin) LPC_GPIO(PORT_0)->SET |= (1UL << pin)

#define BUT1    28  /* P0.28 */
#define BUT2    27  /* P0.27 */
#define BUT3    26  /* P3.26 */
#define BUT4    25  /* P3.25 */
#define BUT5    29  /* P0.29 */
#define BUT6    30  /* P0.30 */

#define BUTTON_PIN_INIT() { \
    LPC_GPIO(PORT_0)->DIR &= (~((1UL << BUT1) | (1UL << BUT2) | (1UL << BUT5) | (1UL << BUT6))); \
    LPC_GPIO(PORT_3)->DIR &= (~((1UL << BUT3) | (1UL << BUT4))); \
    }

#define BUTTON_IS_PUSHED(port_num, pin_num) (LPC_GPIO(port_num)->PIN & (1UL << pin_num) ? (0) : (1))

#define MAXIMUM_AMPE    50

#define WDT_WDMOD_WDEN  0
#define WDT_WDMOD_WDRESET  1

/* Function prototypes */
static void LCD_Dummy(void);
static void USART_Init(ARM_DRIVER_USART *USARTdrv, ARM_USART_SignalEvent_t pUSART_Callback);
static void USART_modbus_Callback(uint32_t event);
static void USART_sim800_Callback(uint32_t event);
static bool USART_SendCommand(ARM_DRIVER_USART *USARTdrv, int instance, const char *data, int SendCmdLength);
static bool USART_SIM800_VerifyReceivedData(char *data);
bool USART_Communication(ARM_DRIVER_USART *USARTdrv, int instance, const char *dataIn, char* dataOut, int SendCmdLength, int ReceiveLength);
void Timer0_Notification(void);
void Timer1_Notification(void);
void Timer2_Notification(void);
void Timer3_Notification(void);
void Delay_ms (uint16_t ms);
float IEEE754_Converter(uint32_t input);
void LCD_DisplayData(uint8_t line, ValueToGet phase);
static void GetDisplayData(ValueToGet phase, char* pDisplayBuffer);
static uint8_t getFraction(float value);
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
    /* TIMER_Stop(0); */
    /* For debugging */
    /* GLCD_Clr();
    CONSOLE_LOG("No data from SIM800A", 3); */
    /* USART_RX_Timeout[0] = true; */

    /* Chaging code: TIMER 0 is now for display blinking */
    Display_Blink++;
}

void Timer1_Notification(void)
{
    /* User code */
    TIMER_Stop(1);
    /* For debugging */
    /* GLCD_Clr();
    CONSOLE_LOG("No data from MODBUS", 4); */
    USART_RX_Timeout[1] = true;
}

void Timer2_Notification(void)
{
    /* User code */
    /* Trigger reading data from MODBUS */
    if (MODBUS_Trigger == false)
    {
        MODBUS_Trigger = true;
    }
    /* Timer2 also used to reset WDG counter */
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
    static ARM_DRIVER_USART *sim800_USARTdrv = &Driver_USART2;
    static ARM_DRIVER_USART *modBUS_USARTdrv = &Driver_USART1;

    struct ValueToGet phaseValue[3];

    const char VoltageReadCommand[3][8] =  {{0x01, 0x04, 0x00, 0x00, 0x00, 0x02, 0x71, 0xCB},\
                                            {0x01, 0x04, 0x00, 0x02, 0x00, 0x02, 0xD0, 0x0B},\
                                            {0x01, 0x04, 0x00, 0x04, 0x00, 0x02, 0x30, 0x0A}};

    const char AmpeReadCommand[3][8]=   {{0x01, 0x04, 0x00, 0x06, 0x00, 0x02, 0x91, 0xCA},\
                                         {0x01, 0x04, 0x00, 0x08, 0x00, 0x02, 0xF0, 0x09},\
                                         {0x01, 0x04, 0x00, 0x0A, 0x00, 0x02, 0x51, 0xC9}};

    const char PowerReadCommand[3][8] = {{0x01, 0x04, 0x00, 0x0C, 0x00, 0x02, 0xB1, 0xC8},\
                                         {0x01, 0x04, 0x00, 0x0E, 0x00, 0x02, 0x10, 0x08},\
                                         {0x01, 0x04, 0x00, 0x10, 0x00, 0x02, 0x70, 0x0E}};

    char TestString[] = "TB:Thiet bi qua tai!";
    char EndOfTestString[1] = {0x1A};
    char PhoneNumber[10] = "0973569162";
    char Make_Call_String[17];
    char Make_Sms_String[23];
    uint8_t MaximumAmpe = 5;
    char RawMaximumAmpe[5];

    bool PhoneNumberSetting = false;
    bool AmpeMaxSetting = false;
    uint8_t ModeSetting = 0;
    uint8_t currentCursor;
    uint8_t currentDigit;

    bool OverloadBit = false;

    SystemInit();
    SystemCoreClockUpdate();

    /* Init and close all relays */
    RELAY_PIN_INIT();
    RELAY_CLOSE(RELAY0);
    RELAY_CLOSE(RELAY1);

    /* Init button */
    BUTTON_PIN_INIT();

    //WDG_Disable();
    WDG_Init(10000);

    /* USART fucntions */
    USART_Init(sim800_USARTdrv, USART_sim800_Callback);
    USART_Init(modBUS_USARTdrv, USART_modbus_Callback);

    /* Timer function */
    /* Timer0 used for display blinking implementation */
    TIMER_Init(0,500000);                  /* Configure timer0 to generate 500ms(500000us) delay */
    TIMER_AttachInterrupt(0,Timer0_Notification);  /* myTimerIsr_0 will be called by TIMER0_IRQn */

    /* Timer1 used for setting USART MODBUS RX Timeout */
    /* With BDR=9600 => T= 0.104ms * 10 bits = 1.04ms for each character. */
    TIMER_Init(1,50000);                  /* Configure timer1 to generate 50ms(50000us) delay */
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
    /* LCD_Dummy(); */
    GLCD_Print78(0, 0, "Bo dieu khien trung tam");
    GLCD_Print78(1, 0, "Dang khoi tao...");

    /* Init and test SIM800 coummunication */
    /* rx_length = SendCmdLength + OK_CR_LF; */
    /* rx_length = OK_CR_LF; */
    /* returnValue = USART_Communication(sim800_USARTdrv, 0, SIM800A_ECHO_OFF, cmd, SIM800A_ECHO_OFF_LENGTH, OK_CR_LF); */
    returnValue = USART_SendCommand(sim800_USARTdrv, 0, SIM800A_ECHO_OFF, SIM800A_ECHO_OFF_LENGTH);
    /* returnValue = USART_SIM800_VerifyReceivedData(cmd); */
    RESET_SIM800_PARAMETER();
    Delay_ms(2000);

    /* returnValue = USART_Communication(sim800_USARTdrv, 0, SIM800A_PING, cmd, SIM800A_PING_LENGTH, OK_CR_LF); */
    /* returnValue = USART_SIM800_VerifyReceivedData(cmd); */
    /* RESET_SIM800_PARAMETER();
    Delay_ms(2000); */

    /* returnValue = USART_Communication(sim800_USARTdrv, 0, SIM800A_SET_BAUD_9600, cmd, SIM800A_SET_BAUD_9600_LENGTH, OK_CR_LF); */
    returnValue = USART_SendCommand(sim800_USARTdrv, 0, SIM800A_SET_BAUD_9600, SIM800A_SET_BAUD_9600_LENGTH);
    /* returnValue = USART_SIM800_VerifyReceivedData(cmd); */
    RESET_SIM800_PARAMETER();
    Delay_ms(2000);

    /* returnValue = USART_Communication(sim800_USARTdrv, 0, SIM800A_TEXT_MODE, cmd, SIM800A_TEXT_MODE_LENGTH, OK_CR_LF); */
    returnValue = USART_SendCommand(sim800_USARTdrv, 0, SIM800A_TEXT_MODE, SIM800A_TEXT_MODE_LENGTH);
    /* returnValue = USART_SIM800_VerifyReceivedData(cmd); */
    RESET_SIM800_PARAMETER();
    Delay_ms(2000);

    /* strcpy(Make_Call_String, SIM800A_MAKE_CALL);
    strcat(Make_Call_String, PhoneNumber);
    strcat(Make_Call_String, SIM800A_MAKE_CALL_END_OF_LINE);
    returnValue = USART_Communication(sim800_USARTdrv, 0, Make_Call_String, cmd, SIM800A_MAKE_CALL_LENGTH, OK_CR_LF);
    returnValue = USART_SIM800_VerifyReceivedData(cmd);
    RESET_SIM800_PARAMETER();
    Delay_ms(2000); */

    GLCD_Clr_Line(1);
    sprintf(RawMaximumAmpe, "%d", MaximumAmpe);

    for (int i = 0; i < 3; i++)
    {
        phaseValue[i].id = i;
    }

    while (1)
    {
        if (BUTTON_IS_PUSHED(PORT_0, BUT1))
        {
            while(BUTTON_IS_PUSHED(PORT_0, BUT1));
            GLCD_Print78(5, 0, "SDT:");
            GLCD_Print78(5, 40, PhoneNumber);
            GLCD_Print78(6, 0, "Amax:");
            GLCD_Print78(6, 48, RawMaximumAmpe);
            GLCD_Print78(6, 64, "A");
            currentCursor = 40;
            TIMER_Start(0);
            ModeSetting++;
            if (ModeSetting == 1)
            {
                PhoneNumberSetting = true;
                AmpeMaxSetting = false;
            }
            else if (ModeSetting == 2)
            {
                PhoneNumberSetting = false;
                AmpeMaxSetting = true;
            }
            else
            {
                ModeSetting = 0;
            }
        }

        if (BUTTON_IS_PUSHED(PORT_3, BUT3))
        {
            while(BUTTON_IS_PUSHED(PORT_3, BUT3));
            if ((PhoneNumberSetting == true) || (AmpeMaxSetting == true))
            {
                TIMER_Stop(0);
                Display_Blink = 0;
                currentCursor = 40;     /* Reset to first digit position */
                currentDigit = 0;
                GLCD_Print78(5, 40, PhoneNumber);
                GLCD_Print78(6, 48, RawMaximumAmpe);
                GLCD_Print78(7, 0, "Da luu!");
                PhoneNumberSetting = false;
                AmpeMaxSetting = false;
                ModeSetting = 0;
                Delay_ms(1000);
                GLCD_Clr_Line(5);
                GLCD_Clr_Line(6);
                GLCD_Clr_Line(7);
            }
            /* Re-setting relay here */
            if (OverloadBit == true)
            {
                OverloadBit = false;
                GLCD_Clr_Line(4);
                RELAY_CLOSE(RELAY0);
            }
        }

        if (BUTTON_IS_PUSHED(PORT_0, BUT2))
        {
            while(BUTTON_IS_PUSHED(PORT_0, BUT2));
            if (PhoneNumberSetting == true)
            {
                GLCD_Print78(5, 40, PhoneNumber);
                currentCursor+=8;
                if (currentCursor == 120) /* Reach highest digit position */
                {
                    currentCursor = 40; /* First digit position */
                }
            }
        }

        if (BUTTON_IS_PUSHED(PORT_3, BUT4))
        {
            while(BUTTON_IS_PUSHED(PORT_3, BUT4));
            if (PhoneNumberSetting == true)
            {
                GLCD_Print78(5, 40, PhoneNumber);
                currentCursor-=8;
                if (currentCursor == 32) /* Reach lowest digit position */
                {
                    currentCursor = 112; /* Last digit position */
                }
            }
        }

        if (BUTTON_IS_PUSHED(PORT_0, BUT5))
        {
            while(BUTTON_IS_PUSHED(PORT_0, BUT5));
            if (PhoneNumberSetting == true)
            {
                GLCD_Print78(5, 40, PhoneNumber);
                PhoneNumber[currentDigit]++;
                if (PhoneNumber[currentDigit] == 0x3A)   /* Reach above range of 0-9 */
                {
                    PhoneNumber[currentDigit] = '0';
                }
            }
            if (AmpeMaxSetting == true)
            {
                GLCD_Print78(6, 48, RawMaximumAmpe);
                MaximumAmpe++;
                if (MaximumAmpe == MAXIMUM_AMPE+1)
                {
                    MaximumAmpe = 1;
                }
                sprintf(RawMaximumAmpe, "%d", MaximumAmpe);
                GLCD_Print78(6, 48, RawMaximumAmpe);
            }
        }

        if (BUTTON_IS_PUSHED(PORT_0, BUT6))
        {
            while(BUTTON_IS_PUSHED(PORT_0, BUT6));
            if (PhoneNumberSetting == true)
            {
                GLCD_Print78(5, 40, PhoneNumber);
                PhoneNumber[currentDigit]--;
                if (PhoneNumber[currentDigit] == 0x2F)   /* Reach below range of 0-9 */
                {
                    PhoneNumber[currentDigit] = '9';
                }
            }
            if (AmpeMaxSetting == true)
            {
                GLCD_Print78(6, 48, RawMaximumAmpe);
                MaximumAmpe--;
                if (MaximumAmpe == 0)
                {
                    MaximumAmpe = MAXIMUM_AMPE;
                }
                sprintf(RawMaximumAmpe, "%d", MaximumAmpe);
                GLCD_Print78(6, 48, RawMaximumAmpe);
            }
        }

        if (PhoneNumberSetting == true)
        {
            /* Re-appending ampe setting */
            GLCD_Print78(6, 48, RawMaximumAmpe);
            currentDigit = (currentCursor - 40)/8;
            if (Display_Blink%2 != 0)
            {
                GLCD_Print78(5, currentCursor, " ");
            }
            else
            {
                GLCD_Print78(5, currentCursor, &PhoneNumber[currentDigit]);
            }
        }

        if (AmpeMaxSetting == true)
        {
            /* Re-appending phone number setting */
            GLCD_Print78(5, 40, PhoneNumber);
            if (Display_Blink%2 != 0)
            {
                GLCD_Print78(6, 48, "  ");
            }
            else
            {
                GLCD_Print78(6, 48, RawMaximumAmpe);
            }
        }

        if (MODBUS_Trigger == true)
        {
            /* Start MODBUS reading data */
            for (int phaseCounter = 0; phaseCounter < 3; phaseCounter++)
            {
                returnValue = USART_Communication(modBUS_USARTdrv, 1, &VoltageReadCommand[phaseCounter][0], modbus_buffer, 8, 9);
                phaseValue[phaseCounter].RawVoltage = (modbus_buffer[3] << 24) | (modbus_buffer[4] << 16) | (modbus_buffer[5] << 8) | modbus_buffer[6];
                phaseValue[phaseCounter].ConvertedVoltage = IEEE754_Converter(phaseValue[phaseCounter].RawVoltage);
                RESET_MODBUS_PARAMETER();

                returnValue = USART_Communication(modBUS_USARTdrv, 1, &AmpeReadCommand[phaseCounter][0], modbus_buffer, 8, 9);
                phaseValue[phaseCounter].RawAmpe = (modbus_buffer[3] << 24) | (modbus_buffer[4] << 16) | (modbus_buffer[5] << 8) | modbus_buffer[6];
                phaseValue[phaseCounter].ConvertedAmpe = IEEE754_Converter(phaseValue[phaseCounter].RawAmpe);
                if (phaseValue[phaseCounter].ConvertedAmpe > (float)MaximumAmpe)
                {
                    OverloadBit = true;
                    /* Open Relay here */
                    /* Ngat Ro-le */
                    RELAY_OPEN(RELAY0);
                    /* Send SMS here */
                    strcpy(Make_Sms_String, SIM800A_MAKE_SMS);
                    strcat(Make_Sms_String, PhoneNumber);
                    strcat(Make_Sms_String, SIM800A_MAKE_SMS_END_OF_LINE);
                    returnValue = USART_SendCommand(sim800_USARTdrv, 0, Make_Sms_String, SIM800A_MAKE_SMS_LENGTH);
                    RESET_SIM800_PARAMETER();
                    Delay_ms(2000);

                    returnValue = USART_SendCommand(sim800_USARTdrv, 0, TestString, 21);
                    Delay_ms(1000);
                    returnValue = USART_SendCommand(sim800_USARTdrv, 0, EndOfTestString, 1);
                    Delay_ms(1000);
                    RESET_SIM800_PARAMETER();
                    GLCD_Print78(4, 0, "TB Qua tai!!!");
                }
                else
                {
                    /* code */
                    /* OverloadBit = false; */
                    /* GLCD_Clr_Line(4); */
                }
                
                RESET_MODBUS_PARAMETER();

                /* returnValue = USART_Communication(modBUS_USARTdrv, 1, &PowerReadCommand[phaseCounter][0], modbus_buffer, 8, 9);
                phaseValue[phaseCounter].RawPower = (modbus_buffer[3] << 24) | (modbus_buffer[4] << 16) | (modbus_buffer[5] << 8) | modbus_buffer[6];
                phaseValue[phaseCounter].ConvertedPower = IEEE754_Converter(phaseValue[phaseCounter].RawPower);
                RESET_MODBUS_PARAMETER(); */

                LCD_DisplayData(phaseCounter + 1, phaseValue[phaseCounter]);
            }

            MODBUS_Trigger = false;
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

static void USART_sim800_Callback(uint32_t event)
{
    if (event & ARM_USART_EVENT_SEND_COMPLETE)
    {
        USART_TX_Complete[0] = true;
    }

    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE)
    {
        TIMER_Stop(0);
        USART_RX_Complete[0] = true;
    }
}

static void USART_modbus_Callback(uint32_t event)
{
    if (event & ARM_USART_EVENT_SEND_COMPLETE)
    {
        USART_TX_Complete[1] = true;
    }

    if (event & ARM_USART_EVENT_RECEIVE_COMPLETE)
    {
        TIMER_Stop(1);
        USART_RX_Complete[1] = true;
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
        /* For debugging */
        /* GLCD_Clr();
        CONSOLE_LOG("Send Command not success", 0);
        CONSOLE_LOG(data, 1); */
        return ((bool)E_NOT_OK);
    }

    return ((bool)E_OK);
}

static bool USART_SIM800_VerifyReceivedData(char *data)
{
    /* Received data should be a copied of sent command, append "\nOK\r\n" */
    /* Example: Send cmd "AT\r\n" (4 characters) */
    /*          Receive cmd "AT\r\n\nOK\r\n" (9 characters) */

    /* Update: Receive cmd is now always as "\r\nOK\r\n" if succesfully */
    if (USART_RX_Complete[0] == true)
    {
        if (data[2] != 'O')
        {
            /* For debugging */
            /* GLCD_Clr();
            CONSOLE_LOG("Error Data from SIM800A", 0);
            CONSOLE_LOG(data, 1); */
            return (E_NOT_OK);
        }
        else
        {
            /* For debugging */
            /* GLCD_Clr();
            CONSOLE_LOG("SIM800A was responsed OK!", 0);
            CONSOLE_LOG(data, 1); */
        }
    }
    else
    {
        return (E_NOT_OK);
    }
    
    return (E_OK);
}

bool USART_Communication(ARM_DRIVER_USART *USARTdrv, int instance, const char *dataIn, char* dataOut, int SendCmdLength, int ReceiveLength)
{
    bool returnValue = E_NOT_OK;

    returnValue = USART_SendCommand(USARTdrv, instance, dataIn, SendCmdLength);

    if (returnValue == E_OK)
    {
        returnValue = E_NOT_OK;

        USARTdrv->Receive(dataOut, ReceiveLength);

        /* Start timer0 for trigger timeout if occurs */
        TIMER_Start(instance);

        while (USART_RX_Complete[instance] != true)
        {
            if (USART_RX_Timeout[instance] == true)
            {
                returnValue = E_NOT_OK;
                break;
            }
        }
    }
    return returnValue;
}

float IEEE754_Converter(uint32_t raw)
{
    /* uint8_t sign; */
    uint32_t mantissa;
    int32_t exp;
    float result;

    mantissa = (raw & 0x7FFFFF) | 0x800000;
    exp = ((raw >> 23) & 0xFF) - 127 - 23;
    result = mantissa * pow(2.0, exp);

    return result;
}

void LCD_DisplayData(uint8_t line, ValueToGet phase)
{
    char DisplayBuffer[20];

    GetDisplayData(phase, DisplayBuffer);
    GLCD_Print78(line, 0, DisplayBuffer);
}

static uint8_t getFraction(float value)
{
    uint16_t tmpInt1 = (uint16_t)value;
    float tmpFrac = value - tmpInt1;
    uint8_t tmpInt2 = trunc(tmpFrac * 10);

    return tmpInt2;
}

static void GetDisplayData(ValueToGet phase, char* pDisplayBuffer)
{
    sprintf(pDisplayBuffer, "O%d: %03dV %02d.%01dA", \
    phase.id, \
    (uint16_t)phase.ConvertedVoltage, \
    (uint16_t)phase.ConvertedAmpe, \
    getFraction(phase.ConvertedAmpe));
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