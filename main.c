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
#define RELAY_CLOSE(pin) LPC_GPIO(PORT_0)->CLR |= (1UL << pin)
#define RELAY_OPEN(pin) LPC_GPIO(PORT_0)->SET |= (1UL << pin)

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
    TIMER_Stop(0);
    /* For debugging */
    /* GLCD_Clr();
    CONSOLE_LOG("No data from SIM800A", 3); */
    USART_RX_Timeout[0] = true;
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

    char TestString[] = "Test!";
    char EndOfTestString[1] = {0x1A};
    char PhoneNumber[10] = "0973569162";
    char Make_Call_String[17];
    char Make_Sms_String[23];
    float MaximumAmpe = 5.0;


    SystemInit();
    SystemCoreClockUpdate();

    /* Init and open all relays */
    RELAY_PIN_INIT();
    RELAY_OPEN(RELAY0);
    RELAY_OPEN(RELAY1);

    /* LCD functions */
    GLCD_Init();
    GLCD_Clr();
    /* LCD_Dummy(); */
    GLCD_Print78(0, 0, "Bo dieu khien trung tam");
    GLCD_Print78(1, 0, "Dang khoi tao...");

    /* USART fucntions */
    USART_Init(sim800_USARTdrv, USART_sim800_Callback);
    USART_Init(modBUS_USARTdrv, USART_modbus_Callback);

    /* Timer function */
    /* Timer0 used for setting USART SIM800 RX Timeout */
    /* With BDR=9600 => T= 0.104ms * 10 bits = 1.04ms for each character. */
    TIMER_Init(0,50000);                  /* Configure timer0 to generate 50ms(50000us) delay */
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

    /* Init and test SIM800 coummunication */
    /* rx_length = SendCmdLength + OK_CR_LF; */
    /* rx_length = OK_CR_LF; */
    returnValue = USART_Communication(sim800_USARTdrv, 0, SIM800A_ECHO_OFF, cmd, SIM800A_ECHO_OFF_LENGTH, OK_CR_LF);
    returnValue = USART_SIM800_VerifyReceivedData(cmd);
    RESET_SIM800_PARAMETER();
    Delay_ms(2000);

    returnValue = USART_Communication(sim800_USARTdrv, 0, SIM800A_PING, cmd, SIM800A_PING_LENGTH, OK_CR_LF);
    returnValue = USART_SIM800_VerifyReceivedData(cmd);
    RESET_SIM800_PARAMETER();
    Delay_ms(2000);

    returnValue = USART_Communication(sim800_USARTdrv, 0, SIM800A_SET_BAUD_9600, cmd, SIM800A_SET_BAUD_9600_LENGTH, OK_CR_LF);
    returnValue = USART_SIM800_VerifyReceivedData(cmd);
    RESET_SIM800_PARAMETER();
    Delay_ms(2000);

    returnValue = USART_Communication(sim800_USARTdrv, 0, SIM800A_TEXT_MODE, cmd, SIM800A_TEXT_MODE_LENGTH, OK_CR_LF);
    returnValue = USART_SIM800_VerifyReceivedData(cmd);
    RESET_SIM800_PARAMETER();
    Delay_ms(2000);

    /* strcpy(Make_Call_String, SIM800A_MAKE_CALL);
    strcat(Make_Call_String, PhoneNumber);
    strcat(Make_Call_String, SIM800A_MAKE_CALL_END_OF_LINE);
    returnValue = USART_Communication(sim800_USARTdrv, 0, Make_Call_String, cmd, SIM800A_MAKE_CALL_LENGTH, OK_CR_LF);
    returnValue = USART_SIM800_VerifyReceivedData(cmd);
    RESET_SIM800_PARAMETER();
    Delay_ms(2000); */

    strcpy(Make_Sms_String, SIM800A_MAKE_SMS);
    strcat(Make_Sms_String, PhoneNumber);
    strcat(Make_Sms_String, SIM800A_MAKE_SMS_END_OF_LINE);
    returnValue = USART_Communication(sim800_USARTdrv, 0, Make_Sms_String, cmd, SIM800A_MAKE_SMS_LENGTH, OK_CR_LF);
    returnValue = USART_SIM800_VerifyReceivedData(cmd);
    RESET_SIM800_PARAMETER();
    Delay_ms(2000);

    returnValue = USART_Communication(sim800_USARTdrv, 0, TestString, cmd, 5, OK_CR_LF);
    returnValue = USART_Communication(sim800_USARTdrv, 0, EndOfTestString, cmd, 1, OK_CR_LF);
    RESET_SIM800_PARAMETER();
    Delay_ms(2000);

    GLCD_Clr_Line(1);
    for (int i = 0; i < 3; i++)
    {
        phaseValue[i].id = i;
    }

    while (1)
    {
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
                if (phaseValue[phaseCounter].ConvertedAmpe > MaximumAmpe)
                {
                    /* Close Relay here */
                    RELAY_CLOSE(RELAY0);
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