#ifndef LCD_H
#define LCD_H

#include <math.h>
#include "font.h"
#include "FontSetup.h"
#include "FontTenCT.h"
#include "LPC17xx.h"

#define LPC_GPIO(n) ((LPC_GPIO_TypeDef *)(LPC_GPIO0_BASE + 0x00020 * n))
#define DIR FIODIR
#define SET FIOSET
#define CLR FIOCLR
#define PIN FIOPIN
#define MASK FIOMASK
#define PORT_0 0
#define PORT_1 1
#define PORT_3 3
#define GLCD_DATA_O PORT_1
#define GLCD_DATA_I PORT_1

void GLCD_Delay(void);               //delay 16 chu ky may
void GLCD_OUT_Set(void);                //Set huong Out cho cac chan
void GLCD_IN_Set(void);               //Set huong In cho cac chan
void GLCD_SetSide(unsigned char Side);        //Chon chip KS0108 trai hoac phai
void wait_GLCD(void);                //cho GLCD ranh
void GLCD_SetDISPLAY(uint8_t ON);    //Cho phep hien thu GLCD
void GLCD_SetYADDRESS(uint8_t Col);    //Set dia chi cot Y
void GLCD_SetXADDRESS(uint8_t Line);    //set dia chi page X
void GLCD_StartLine(uint8_t Offset);    //set start line khi cuon GLCD
void GLCD_WriteDATA(uint8_t DATA);    //ghi du lieu vao GLCD
uint8_t GLCD_ReadDATA(void);            //doc du lieu tu GLCD
void GLCD_Init(void);                //khoi dong GLCD
void GLCD_GotoXY(uint8_t Line, uint8_t Col);    //Di chuyen den vi tri co to do page, cot tren man hinh
void GLCD_Clr(void);                            //Xoa toan bo GLCD
void GLCD_Clr_Line(uint8_t Line);
void GLCD_PutChar78(uint8_t Line, uint8_t Col, uint16_t chr);        //In ky tu kich thuoc 7x8
void GLCD_Print78(uint8_t Line, uint8_t Col, char* str);            //In chuoi ky tu 7x8
void GLCD_PutBMP(uint8_t * bmp, uint8_t x, uint8_t y);                                        //in hinh anh len GLCD
#endif
