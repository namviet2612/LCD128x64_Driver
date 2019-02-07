//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
// Author : Ho Thanh Tam                                                             ;
// Email:   thanhtam.h@gmail.com                                                     ;        
// Date :   2009, Oct., 15                                                           ;
// Version: 0.1                                                                      ;
// Title:   thu vien myGLCD cho Graphic LCD                                          ;
// website: www.hocavr.com                                                           ;
// Description: Dieu khien Graphic LCD 128x64 dung KS0108   .                        ;
//                                                                                   ; 
//;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
/*
//----cac ham co trong thu vien--------------------------------------------- 
void GLCD_Delay(void)                //delay 16 chu ky may
void GLCD_OUT_Set(void)                //Set huong Out cho cac chan
void GLCD_IN_Set(void)                //Set huong In cho cac chan
void GLCD_SetSide(char Side)        //Chon chip KS0108 trai hoac phai
void wait_GLCD(void)                //cho GLCD ranh
void GLCD_SetDISPLAY(uint8_t ON)    //Cho phep hien thu GLCD
void GLCD_SetYADDRESS(uint8_t Col)    //Set dia chi cot Y
void GLCD_SetXADDRESS(uint8_t Line)    //set dia chi page X
void GLCD_StartLine(uint8_t Offset)    //set start line khi cuon GLCD
void GLCD_WriteDATA(uint8_t DATA)    //ghi du lieu vao GLCD
uint8_t GLCD_ReadDATA(void)            //doc du lieu tu GLCD
void GLCD_Init(void)                //khoi dong GLCD
void GLCD_GotoXY(uint8_t Line, uint8_t Col)    //Di chuyen den vi tri co to do page, cot tren man hinh
void GLCD_Clr(void)                            //Xoa toan bo GLCD
void GLCD_PutChar78(uint8_t Line, uint8_t Col, uint8_t chr)        //In ky tu kich thuoc 7x8
void GLCD_Print78(uint8_t Line, uint8_t Col, char* str)            //In chuoi ky tu 7x8
void GLCD_PutBMP(char *bmp)                                        //in hinh anh len GLCD
*/

#include "lcd.h"

#define LPC_GPIO(n)             ((LPC_GPIO_TypeDef *)(LPC_GPIO0_BASE + 0x00020*n))
#define DIR  FIODIR
#define SET  FIOSET
#define CLR  FIOCLR
#define PIN  FIOPIN
#define MASK FIOMASK
/* #define _BV(b)    (1<<(b)) */
/* #define bit_is_set(sfr, b)  (sfr & (1<<b)) */
#define bit_is_set(port_num, pin_num) ((LPC_GPIO(port_num)->PIN & (1UL << pin_num)) ? (1) : (0))
/* #define cbi(sfr, b)  (sfr &= ~_BV(b)) */
#define cbi(port_num, pin_num) (LPC_GPIO(port_num)->CLR = (1UL << pin_num))
/* #define sbi(sfr, b) (sfr |=  _BV(b)) */
#define sbi(port_num, pin_num) (LPC_GPIO(port_num)->SET = (1UL << pin_num))

//define the data port (D7:0)
/* #define GLCD_DATA_O      PORTA  */ // Ouput Data bus
/* #define GLCD_DATA_I          PINA  */  // Input Data bus
/* #define GLCD_DATA_DDR      DDRA  */  // Direction
#define GLCD_D7          18
#define GLCD_D6          19
#define GLCD_D5          20
#define GLCD_D4          21
#define GLCD_D3          22
#define GLCD_D2          23
#define GLCD_D1          24
#define GLCD_D0          25
#define GLCD_DATA_MASK      0xFC03FFFF

//define the control port (RS, RW, E, CS1, CS2)
/* #define GLCD_CTRL_O        PORTC  */
/* #define GLCD_CTRL_I     PINC  */
/* #define GLCD_CTRL_DDR     DDRC */
//GLCD control pins
#define GLCD_E             1// Enable line
#define GLCD_RW         0 // read/write selecting line
#define GLCD_RS         29 // Register select: Data/Instruction (DI)
#define GLCD_CS1        28 // left/right selecting line#1
#define GLCD_CS2        27 // left/right selecting line#2


#define PORT_0          0
#define PORT_1          1
#define GLCD_DATA_O          PORT_1
#define GLCD_DATA_I          PORT_1

//define the macro to enable/disable GLCD
#define GLCD_ENABLE        cbi(PORT_0,GLCD_E)
#define GLCD_DISABLE       sbi(PORT_0,GLCD_E)

//Instruction code
#define GLCD_DISPLAY    0x3E //0011111x: display, the last bit is ON or OFF
#define GLCD_YADDRESS    0x40 //01xxxxxx: Set Y (column) address
#define GLCD_XADDRESS    0xB8 //10111xxx: set X (page, line) address
#define GLCD_STARTLINE    0xC0 //11xxxxxx: Display start line or page scroll, xxxxxx is the offset of scroll
/* #define GLCD_BUSY        0x80 */ //7 //bit Busy in Status
#define GLCD_BUSY        GLCD_D7



//*************************supporting functions*****************************
void GLCD_Delay(void){
    uint8_t i;
    //for(i=0; i<16; i++) asm volatile ("nop"::);
    for(i=0; i<2; i++){
        __asm("nop");
    }
}                                                                            
void GLCD_OUT_Set(void){                                                  
    /* GLCD_CTRL_DDR |=                                                      
    (1<<GLCD_E)|(1<<GLCD_RW)|(1<<GLCD_RS)|(1<<GLCD_CS1)|(1<<GLCD_CS2); */    
    LPC_GPIO(PORT_0)->DIR |=  (1UL << GLCD_E)|(1UL << GLCD_RW);
    LPC_GPIO(PORT_1)->DIR |=  (1UL << GLCD_RS)|(1UL << GLCD_CS1)|(1UL << GLCD_CS2);
    cbi(PORT_0, GLCD_E);
    /* GLCD_DATA_DDR=0xFF; */
    LPC_GPIO(PORT_1)->DIR |=  (1UL << GLCD_D7)|(1UL << GLCD_D6)|(1UL << GLCD_D5)| \
                              (1UL << GLCD_D4)|(1UL << GLCD_D3)|(1UL << GLCD_D2)| \
                              (1UL << GLCD_D1)|(1UL << GLCD_D0);
    /* GLCD_DATA_O=0x00; */
    LPC_GPIO(PORT_1)->CLR |=  (1UL << GLCD_D7)|(1UL << GLCD_D6)|(1UL << GLCD_D5)| \
                              (1UL << GLCD_D4)|(1UL << GLCD_D3)|(1UL << GLCD_D2)| \
                              (1UL << GLCD_D1)|(1UL << GLCD_D0);
}
void GLCD_IN_Set(void){
    /* GLCD_CTRL_DDR |=
    (1<<GLCD_E)|(1<<GLCD_RW)|(1<<GLCD_RS)|(1<<GLCD_CS1)|(1<<GLCD_CS2);  */
    LPC_GPIO(PORT_0)->DIR |=  (1UL << GLCD_E)|(1UL << GLCD_RW);
    LPC_GPIO(PORT_1)->DIR |=  (1UL << GLCD_RS)|(1UL << GLCD_CS1)|(1UL << GLCD_CS2);
    cbi(PORT_0, GLCD_E);
    /* GLCD_DATA_DDR=0x00; */
    LPC_GPIO(PORT_1)->DIR &=  ~((1UL << GLCD_D7)|(1UL << GLCD_D6)|(1UL << GLCD_D5)| \
                              (1UL << GLCD_D4)|(1UL << GLCD_D3)|(1UL << GLCD_D2)| \
                              (1UL << GLCD_D1)|(1UL << GLCD_D0));
    /* GLCD_DATA_O=0xFF; */ ////pull up resistors
    LPC_GPIO(PORT_1)->SET |=  (1UL << GLCD_D7)|(1UL << GLCD_D6)|(1UL << GLCD_D5)| \
                              (1UL << GLCD_D4)|(1UL << GLCD_D3)|(1UL << GLCD_D2)| \
                              (1UL << GLCD_D1)|(1UL << GLCD_D0);
}
void GLCD_SetSide(unsigned char Side){    //Left or right controller
    GLCD_OUT_Set(); 
    switch(Side)
    {
    case 0 : 
        cbi(PORT_1, GLCD_CS1);
        cbi(PORT_1, GLCD_CS2);
    break;    

    case 1 :        //0: Left
        sbi(PORT_1, GLCD_CS1);
        cbi(PORT_1, GLCD_CS2);
    break;  
    case 2 : //1:Right
        cbi(PORT_1, GLCD_CS1);
        sbi(PORT_1, GLCD_CS2);
    break;
    }
		GLCD_Delay(); 
}
//*************************END supporting functions**************************

//*************************primary functions of GLCD*************************
//-------------------------For INSTRUCTION---------------------
void wait_GLCD(void){
    GLCD_IN_Set();
    cbi(PORT_1, GLCD_RS); //pull both RS down
    sbi(PORT_0, GLCD_RW); //pull both RW up, (GLCD->AVR)

    GLCD_ENABLE;           //Pull the EN line up
    GLCD_Delay();
    GLCD_DISABLE;
    
    while (bit_is_set(GLCD_DATA_I,GLCD_BUSY)){
        GLCD_ENABLE;           //Pull the EN line up        
        GLCD_Delay();
        GLCD_DISABLE;           //Pull the EN line down                        
    }
}
void GLCD_SetDISPLAY(uint8_t ON){
    wait_GLCD();
    GLCD_OUT_Set();
    cbi(PORT_1, GLCD_RS); //pull both RS, RW down, (AVR->GLCD)
    cbi(PORT_0, GLCD_RW);
    
    /* GLCD_DATA_O=GLCD_DISPLAY+ON; */
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_DISPLAY+ON) & 0x80) >> 7) <<  GLCD_D7;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_DISPLAY+ON) & 0x40) >> 6) <<  GLCD_D6;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_DISPLAY+ON) & 0x20) >> 5) <<  GLCD_D5;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_DISPLAY+ON) & 0x10) >> 4) <<  GLCD_D4;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_DISPLAY+ON) & 0x08) >> 3) <<  GLCD_D3;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_DISPLAY+ON) & 0x04) >> 2) <<  GLCD_D2;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_DISPLAY+ON) & 0x02) >> 1) <<  GLCD_D1;
	  LPC_GPIO(PORT_1)-> SET =  ((GLCD_DISPLAY+ON) & 0x01) <<  GLCD_D0;
    GLCD_ENABLE;           //Pull the EN line up
    GLCD_Delay();
    GLCD_DISABLE;           //Pull the EN line down
}
void GLCD_SetYADDRESS(uint8_t Col){ //set Y address (or column) of GLCD
    wait_GLCD();
    GLCD_OUT_Set();
    cbi(PORT_1, GLCD_RS); //pull both RS, RW down, (AVR->GLCD)
    cbi(PORT_0, GLCD_RW);
    
    /* GLCD_DATA_O=GLCD_YADDRESS+Col; */
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_YADDRESS+Col) & 0x80) >> 7) <<  GLCD_D7;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_YADDRESS+Col) & 0x40) >> 6) <<  GLCD_D6;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_YADDRESS+Col) & 0x20) >> 5) <<  GLCD_D5;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_YADDRESS+Col) & 0x10) >> 4) <<  GLCD_D4;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_YADDRESS+Col) & 0x08) >> 3) <<  GLCD_D3;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_YADDRESS+Col) & 0x04) >> 2) <<  GLCD_D2;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_YADDRESS+Col) & 0x02) >> 1) <<  GLCD_D1;
	  LPC_GPIO(PORT_1)-> SET =  ((GLCD_YADDRESS+Col) & 0x01) <<  GLCD_D0;
    GLCD_ENABLE;           //Pull the EN line up    
    GLCD_Delay();
    GLCD_DISABLE;           //Pull the EN line down  
    
}
void GLCD_SetXADDRESS(uint8_t Line){ //set X address (or line) of GLCD
    wait_GLCD();
    GLCD_OUT_Set();
    cbi(PORT_1, GLCD_RS); //pull both RS, RW down,(AVR->GLCD)
    cbi(PORT_0, GLCD_RW);

    /* GLCD_DATA_O=GLCD_XADDRESS+Line;   */
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_XADDRESS+Line) & 0x80) >> 7) <<  GLCD_D7;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_XADDRESS+Line) & 0x40) >> 6) <<  GLCD_D6;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_XADDRESS+Line) & 0x20) >> 5) <<  GLCD_D5;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_XADDRESS+Line) & 0x10) >> 4) <<  GLCD_D4;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_XADDRESS+Line) & 0x08) >> 3) <<  GLCD_D3;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_XADDRESS+Line) & 0x04) >> 2) <<  GLCD_D2;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_XADDRESS+Line) & 0x02) >> 1) <<  GLCD_D1;
	  LPC_GPIO(PORT_1)-> SET =  ((GLCD_XADDRESS+Line) & 0x01) <<  GLCD_D0;
    GLCD_ENABLE;           //Pull the EN line up
    GLCD_Delay();
    GLCD_DISABLE;           //Pull the EN line down      
}
void GLCD_StartLine(uint8_t Offset){ //Set start line khi cuon GLCD
    wait_GLCD();
    GLCD_OUT_Set();
    cbi(PORT_1, GLCD_RS); //pull both RS, RW down, (AVR->GLCD)
    cbi(PORT_0, GLCD_RW);

    GLCD_SetSide(0); 
    /* GLCD_DATA_O=GLCD_STARTLINE+Offset;   */
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x80) >> 7) <<  GLCD_D7;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x40) >> 6) <<  GLCD_D6;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x20) >> 5) <<  GLCD_D5;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x10) >> 4) <<  GLCD_D4;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x08) >> 3) <<  GLCD_D3;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x04) >> 2) <<  GLCD_D2;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x02) >> 1) <<  GLCD_D1;
	  LPC_GPIO(PORT_1)-> SET =  ((GLCD_STARTLINE+Offset) & 0x01) <<  GLCD_D0;
    GLCD_DISABLE;           //Pull the EN line down
    GLCD_Delay();
    GLCD_ENABLE;           //Pull the EN line up  
        
    GLCD_SetSide(1);    
    /* GLCD_DATA_O=GLCD_STARTLINE+Offset; */
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x80) >> 7) <<  GLCD_D7;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x40) >> 6) <<  GLCD_D6;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x20) >> 5) <<  GLCD_D5;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x10) >> 4) <<  GLCD_D4;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x08) >> 3) <<  GLCD_D3;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x04) >> 2) <<  GLCD_D2;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x02) >> 1) <<  GLCD_D1;
	  LPC_GPIO(PORT_1)-> SET =  ((GLCD_STARTLINE+Offset) & 0x01) <<  GLCD_D0;
    GLCD_DISABLE;           //Pull the EN line down
    GLCD_Delay();
    GLCD_ENABLE;           //Pull the EN line up  
            
    GLCD_SetSide(2);
    /* GLCD_DATA_O=GLCD_STARTLINE+Offset; */
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x80) >> 7) <<  GLCD_D7;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x40) >> 6) <<  GLCD_D6;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x20) >> 5) <<  GLCD_D5;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x10) >> 4) <<  GLCD_D4;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x08) >> 3) <<  GLCD_D3;
	  LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x04) >> 2) <<  GLCD_D2;
		LPC_GPIO(PORT_1)-> SET = (((GLCD_STARTLINE+Offset) & 0x02) >> 1) <<  GLCD_D1;
	  LPC_GPIO(PORT_1)-> SET =  ((GLCD_STARTLINE+Offset) & 0x01) <<  GLCD_D0;
    GLCD_DISABLE;           //Pull the EN line down
    GLCD_Delay();
    GLCD_ENABLE;           //Pull the EN line up  
    

}
//-------------------------END For INSTRUCTION---------------------

//-------------------------For DATA---------------------
//write Data to GLCD: used in "Write Display Data" only
void GLCD_WriteDATA(uint8_t DATA){    
    wait_GLCD();
    GLCD_OUT_Set();
    sbi(PORT_1, GLCD_RS); //pull RS up
    cbi(PORT_0, GLCD_RW); //RW down,  (AVR->GLCD)
    /* GLCD_DATA_O=DATA; */        // Put Data out
	  LPC_GPIO(PORT_1)-> SET = (((DATA) & 0x80) >> 7) <<  GLCD_D7;
	  LPC_GPIO(PORT_1)-> SET = (((DATA) & 0x40) >> 6) <<  GLCD_D6;
		LPC_GPIO(PORT_1)-> SET = (((DATA) & 0x20) >> 5) <<  GLCD_D5;
	  LPC_GPIO(PORT_1)-> SET = (((DATA) & 0x10) >> 4) <<  GLCD_D4;
		LPC_GPIO(PORT_1)-> SET = (((DATA) & 0x08) >> 3) <<  GLCD_D3;
	  LPC_GPIO(PORT_1)-> SET = (((DATA) & 0x04) >> 2) <<  GLCD_D2;
		LPC_GPIO(PORT_1)-> SET = (((DATA) & 0x02) >> 1) <<  GLCD_D1;
	  LPC_GPIO(PORT_1)-> SET =  ((DATA) & 0x01) <<  GLCD_D0;
    GLCD_ENABLE;           //Pull the EN line up    
    GLCD_Delay();
    GLCD_DISABLE;           //Pull the EN line down
    GLCD_Delay();
    
}
//Read DATA from GLCD: used for "Read Display Data" only
uint8_t GLCD_ReadDATA(void){
    uint8_t DATA;
    wait_GLCD();
    GLCD_IN_Set();
    sbi(PORT_1, GLCD_RS); //pull both RS up    
    sbi(PORT_0, GLCD_RW); //pull both RW up, (GLCD->AVR)
    
    GLCD_ENABLE;           //Pull the EN line up
    GLCD_Delay();
    /* DATA=GLCD_DATA_I; */       // get Data    
	  DATA = (DATA & 0x7F) | (bit_is_set(PORT_1, GLCD_D7) << 7U);
	  DATA = (DATA & 0xBF) | (bit_is_set(PORT_1, GLCD_D6) << 6U);
		DATA = (DATA & 0xDF) | (bit_is_set(PORT_1, GLCD_D5) << 5U);
	  DATA = (DATA & 0xEF) | (bit_is_set(PORT_1, GLCD_D4) << 4U);
		DATA = (DATA & 0xF7) | (bit_is_set(PORT_1, GLCD_D3) << 3U);
	  DATA = (DATA & 0xFB) | (bit_is_set(PORT_1, GLCD_D2) << 2U);
		DATA = (DATA & 0xFD) | (bit_is_set(PORT_1, GLCD_D1) << 1U);
	  DATA = (DATA & 0xFE) | bit_is_set(PORT_1, GLCD_D0);
    GLCD_DISABLE;           //Pull the EN line down
    GLCD_Delay();

    return DATA;
}
//*************************END primary functions of GLCD**********************


//*************************Direct used Function*******************************
//----Initialize GLCD---------------------
void GLCD_Init(void){
    GLCD_SetSide(0); // left side
    GLCD_SetDISPLAY(1);
    GLCD_SetYADDRESS(0);
    GLCD_SetXADDRESS(0);
    GLCD_StartLine(0);
    
        GLCD_SetSide(1); //right side
    GLCD_SetDISPLAY(1);
    GLCD_SetYADDRESS(0);
    GLCD_SetXADDRESS(0);
    GLCD_StartLine(0); 
    
    GLCD_SetSide(2); //right side
    GLCD_SetDISPLAY(1);
    GLCD_SetYADDRESS(0);
    GLCD_SetXADDRESS(0);
    GLCD_StartLine(0);    
    

}
//------move the pointer to the X, Y position---------
void GLCD_GotoXY(uint8_t Line, uint8_t Col){ //Line:0-7, Col: 0-127
    uint8_t Side;
    Side=Col/64 ; //Select controller, 0:Left, 1: Right
    GLCD_SetSide(Side);
    Col -= 64*Side; //Update real Col : 0-63
    GLCD_SetYADDRESS(Col);
    GLCD_SetXADDRESS(Line);    
}
void GLCD_Clr(void){
    uint8_t Line, Col;        
    for (Line=0; Line<8; Line++){
        GLCD_GotoXY(Line,0);        
        for (Col=0; Col<64; Col++) GLCD_WriteDATA(0);
    }
    for (Line=0; Line<8; Line++){
        GLCD_GotoXY(Line,64);        
        for (Col=0; Col<64; Col++) GLCD_WriteDATA(0);
    } 
    for (Line=0; Line<8; Line++){
        GLCD_GotoXY(Line,128);        
        for (Col=0; Col<64; Col++) GLCD_WriteDATA(0);
    }    
}
//-------Print a character with 7x8 size onto GLCD-------
void GLCD_PutChar78(uint8_t Line, uint8_t Col, uint16_t chr){
    uint16_t i;
    if ((Col>57) && (Col<64)){ //there is a "jump" from left->right
        GLCD_GotoXY(Line, Col);    //left first
        for(i=0;i<64-Col;i++)             
            GLCD_WriteDATA(font7x8[((chr - 32) * 7) + i]);        
        GLCD_GotoXY(Line, 64);   // then right
        for(i=64-Col;i<7;i++)                         
            GLCD_WriteDATA(font7x8[((chr - 32) * 7) + i]);        
    }
    else{
        GLCD_GotoXY(Line, Col);
        for(i=0;i<7;i++)             
            GLCD_WriteDATA(font7x8[((chr - 32) * 7) + i]);
        
    }
} 


void GLCD_PutChar_setup(uint8_t Line, uint8_t Col, uint16_t chr){
    uint16_t i,j,k;
    uint16_t width_1=0;
    uint8_t width = font_setup[0];
    uint8_t height = font_setup[1];    
        for(j=4;j<(chr - 28);j++)
        {
         width_1= width_1+font_setup[j];
        } 
    if ((Col>(64-font_setup[chr - 28])) && (Col<64)){ //there is a "jump" from left->right
        GLCD_GotoXY(Line, Col);    //left first
        for(i=0;i<64-Col;i++)             
        GLCD_WriteDATA(font_setup[((width_1*2)+100) + i]);    
        GLCD_GotoXY(Line+1, Col);         
        for(i=0;i<64-Col;i++)  
        GLCD_WriteDATA(font_setup[((width_1*2)+100) + i+font_setup[chr - 28]]);      
        GLCD_GotoXY(Line, 64);   // then right
        for(i=64-Col;i<font_setup[chr - 28];i++)                         
        GLCD_WriteDATA(font_setup[((width_1*2)+100) + i]);       
        GLCD_GotoXY(Line+1, 64);   // then right
        for(k=64-Col;k<font_setup[chr - 28];k++)                         
        GLCD_WriteDATA(font_setup[((width_1*2)+100) + k+font_setup[chr - 28]]);    
    }  
    else     if ((Col>(128-font_setup[chr - 28])) && (Col<128))
    { //there is a "jump" from left->right
        GLCD_GotoXY(Line, Col);    //left first
        for(i=0;i<128-Col;i++)             
        GLCD_WriteDATA(font_setup[((width_1*2)+100) + i]);    
        GLCD_GotoXY(Line+1, Col);         
        for(i=0;i<128-Col;i++)  
         GLCD_WriteDATA(font_setup[((width_1*2)+100) + i+font_setup[chr - 28]]);      
         GLCD_GotoXY(Line, 128);   // then right
        for(i=128-Col;i<font_setup[chr - 28];i++)                         
            GLCD_WriteDATA(font_setup[((width_1*2)+100) + i]);       
        GLCD_GotoXY(Line+1, 128);   // then right
        for(k=128-Col;k<font_setup[chr - 28];k++)                         
            GLCD_WriteDATA(font_setup[((width_1*2)+100) + k+font_setup[chr - 28]]);    
    }
    else
    {
        GLCD_GotoXY(Line, Col);   
        for(i=((width_1*2)+100);i<((width_1*2)+100+font_setup[chr - 28]);i++)
        GLCD_WriteDATA(font_setup[i]);  
        GLCD_GotoXY(Line+1, Col);   
        for(k=((width_1*2)+100);k<((width_1*2)+100+font_setup[chr - 28]);k++)
        GLCD_WriteDATA(font_setup[k+font_setup[chr - 28]]);
     }   
} 

void GLCD_Printf_setup(uint8_t Line, uint8_t Col, const char* str)
{
    uint8_t i, x;  
    uint16_t chr;
    x=Col;   
//    chr = str;
    for (i=0; str[i]!=0; i++){
        if (x>=192){
            Col=0;
            x=Col;
            Line++;
        }
        GLCD_PutChar_setup(Line, x , str[i]);     
        x+=font_setup[(str[i]-28)];
    }
}

void GLCD_Print_setup(uint8_t Line, uint8_t Col,char* str){
    uint8_t i, x;
    x=Col;
    for (i=0; str[i]!=0; i++){
        if (x>=192){
            Col=0;
            x=Col;
            Line++;
        }
        GLCD_PutChar_setup(Line, x , str[i]);     
        x+=font_setup[(str[i]-28)];
    }
}
///////////////////////////////TEN CHUONG TRINH /////////////////////////////////////
void GLCD_PutChar_prog(uint8_t Line, uint8_t Col, uint16_t chr){
    uint16_t i,j,k;
    uint16_t width_1=0;
    uint8_t width = font_ten_ct[0];
    uint8_t height = font_ten_ct[1];    
        for(j=4;j<(chr - 28);j++)
        {
         width_1= width_1+font_ten_ct[j];
        } 
    if ((Col>(64-font_ten_ct[chr - 28])) && (Col<64)){ //there is a "jump" from left->right
        GLCD_GotoXY(Line, Col);    //left first
        for(i=0;i<64-Col;i++)             
        GLCD_WriteDATA(font_ten_ct[((width_1*2)+100) + i]);    
        GLCD_GotoXY(Line+1, Col);         
        for(i=0;i<64-Col;i++)  
        GLCD_WriteDATA(font_ten_ct[((width_1*2)+100) + i+font_ten_ct[chr - 28]]);      
        GLCD_GotoXY(Line, 64);   // then right
        for(i=64-Col;i<font_ten_ct[chr - 28];i++)                         
        GLCD_WriteDATA(font_ten_ct[((width_1*2)+100) + i]);       
        GLCD_GotoXY(Line+1, 64);   // then right
        for(k=64-Col;k<font_ten_ct[chr - 28];k++)                         
        GLCD_WriteDATA(font_ten_ct[((width_1*2)+100) + k+font_ten_ct[chr - 28]]);    
    }  
    else     if ((Col>(128-font_ten_ct[chr - 28])) && (Col<128))
    { //there is a "jump" from left->right
        GLCD_GotoXY(Line, Col);    //left first
        for(i=0;i<128-Col;i++)             
        GLCD_WriteDATA(font_ten_ct[((width_1*2)+100) + i]);    
        GLCD_GotoXY(Line+1, Col);         
        for(i=0;i<128-Col;i++)  
         GLCD_WriteDATA(font_ten_ct[((width_1*2)+100) + i+font_ten_ct[chr - 28]]);      
         GLCD_GotoXY(Line, 128);   // then right
        for(i=128-Col;i<font_ten_ct[chr - 28];i++)                         
            GLCD_WriteDATA(font_ten_ct[((width_1*2)+100) + i]);       
        GLCD_GotoXY(Line+1, 128);   // then right
        for(k=128-Col;k<font_ten_ct[chr - 28];k++)                         
            GLCD_WriteDATA(font_ten_ct[((width_1*2)+100) + k+font_ten_ct[chr - 28]]);    
    }
    else
    {
        GLCD_GotoXY(Line, Col);   
        for(i=((width_1*2)+100);i<((width_1*2)+100+font_ten_ct[chr - 28]);i++)
        GLCD_WriteDATA(font_ten_ct[i]);  
        GLCD_GotoXY(Line+1, Col);   
        for(k=((width_1*2)+100);k<((width_1*2)+100+font_ten_ct[chr - 28]);k++)
        GLCD_WriteDATA(font_ten_ct[k+font_ten_ct[chr - 28]]);
     }   
} 

void GLCD_Printf_prog(uint8_t Line, uint8_t Col,const char* str)
{
    uint8_t i, x;  
    uint16_t chr;
    x=Col;   
//    chr = str;
    for (i=0; str[i]!=0; i++){
        if (x>=192){
            Col=0;
            x=Col;
            Line++;
        }
        GLCD_PutChar_prog(Line, x , str[i]);     
        x+=font_ten_ct[(str[i]-28)];
    }
}

void GLCD_Print_prog(uint8_t Line, uint8_t Col,char* str){
    uint8_t i, x;
    x=Col;
    for (i=0; str[i]!=0; i++){
        if (x>=192){
            Col=0;
            x=Col;
            Line++;
        }
        GLCD_PutChar_prog(Line, x , str[i]);     
        x+=font_ten_ct[(str[i]-28)];
    }
}
//print an array of character onto GLCD----
void GLCD_Printf78(uint8_t Line, uint8_t Col,const char* str){
    uint8_t i, x;
    x=Col;
    for (i=0; str[i]!=0; i++){
        if (x>=192){
            Col=0;
            x=Col;
            Line++;
        }
        GLCD_PutChar78(Line, x , str[i]);     
        x+=8;
    }
}


void GLCD_Print78(uint8_t Line, uint8_t Col,char* str){
    uint8_t i, x;
    x=Col;
    for (i=0; str[i]!=0; i++){
        if (x>=192){
            Col=0;
            x=Col;
            Line++;
        }
        GLCD_PutChar78(Line, x , str[i]);     
        x+=8;
    }
}


void GLCD_PutBMP(uint8_t * bmp, uint8_t x, uint8_t y)
{
uint8_t scr_y = y/8;//(y/8)*8;
uint8_t i, j;
uint8_t width = bmp[0];
uint8_t height = bmp[2];

for(j = 0; j < height / 8; j++)
  {
      GLCD_GotoXY( scr_y + j,x);
      for(i = 0; i < width; i++)                    //0-192
      {
        if (((x + i)%64) == 0)
        {
            GLCD_GotoXY( scr_y + j,x + i);
        }
        GLCD_WriteDATA(bmp[j * width + i + 4]);
      }
  }
}