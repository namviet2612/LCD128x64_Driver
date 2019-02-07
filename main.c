#include "lcd.h"
#include "system_LPC17xx.h"

int main(void)
{
		SystemInit();
		SystemCoreClockUpdate();
	
		/* lcd function */
		GLCD_Init();
		GLCD_Clr();
	  GLCD_PutChar78(10,10,'A');
	
	  return (0U);
}