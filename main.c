#include "lcd.h"
#include "system_LPC17xx.h"

/* Function prototype */
static void LCD_Dummy();

int main(void)
{
		SystemInit();
		SystemCoreClockUpdate();
	
		/* lcd function */
		GLCD_Init();
		GLCD_Clr();
		LCD_Dummy();
	
	  return (0U);
}

static void LCD_Dummy()
{
		int counterLCD = 0;
	  char test_str[24] = "AAAAAAAAAAAAAAAAAAAAAAAA";
		for (counterLCD = 0; counterLCD < 8; counterLCD++)
		{
				GLCD_Print78(counterLCD,0,test_str);
		}
}
	