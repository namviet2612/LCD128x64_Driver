#include "DHT.h"

#define DHTTYPE DHT11                            // DHT 11 
#define DHTPIN  A0                               // Chan ket noi 

#define TEMPERATURE_WARNING  28                  // Ngương canh bao nhiet do 28*C

const String myphone = "01689951815";            // Thay so cua ban vao day
const int PWR_KEY =  9;                          // Chan so 9 arduino uno dung lam chan dieu khien bat tat module sim900A

float Temperature = 0;
 
void Gsm_Power_On();                             // Bat module Sim 900A
void Gsm_Init();                                 // Cau hinh Module Sim 900A
void Gsm_MakeCall();                             // Ham goi dien
void Gsm_MakeSMS();                              // Ham nhan tin canh bao

DHT dht(DHTPIN, DHTTYPE);
 
void setup() {
  Serial.begin(9600);                           // Cau hinh UART de giao tiep module Sim 900A
 
  digitalWrite(PWR_KEY, LOW);                   // Khai bao chan PWR_KEY de dieu khien bat bat module Sim 900A
  pinMode(PWR_KEY, OUTPUT);

  dht.begin();                                  // Bat dau dung DHT
  
  delay(1000);                       
  Gsm_Power_On();                               // Bat Module Sim 900A
  Gsm_Init();                                   // Cau hinh module Sim 900A
  Gsm_MakeCall();                               // Test cuoc goi 
}
 
void loop() {
  Temperature = dht.readTemperature();
  Serial.print("Nhiet do : ");                  // Gui noi dung
  Serial.print(Temperature);
  Serial.println(" *C");
  if(Temperature > TEMPERATURE_WARNING)
  {
    Gsm_MakeSMS();
    delay(20000);
  }
  delay(2000);
}

void Gsm_Power_On()
{
  digitalWrite(PWR_KEY, HIGH);                // Du chan PWR_KEY len cao it nhat 1s 
  delay(1500);                                // o day ta de 1,5s
  digitalWrite(PWR_KEY, LOW);                 // Du chan PWR_KEY xuong thap
  delay(10000);                               // cac ban xem trong Hardware designed sim900A de hieu ro hon
}
 
void Gsm_Init()
{
  Serial.println("ATE0");                     // Tat che do phan hoi (Echo mode)
  delay(2000);
  Serial.println("AT+IPR=9600");              // Dat toc do truyen nhan du lieu 9600 bps
  delay(2000);
  Serial.println("AT+CMGF=1");                // Chon che do TEXT Mode
  delay(2000);
  Serial.println("AT+CLIP=1");                // Hien thi thong tin nguoi goi den
  delay(2000);
  Serial.println("AT+CNMI=2,2");              // Hien thi truc tiep noi dung tin nhan
  delay(2000);
}
 
void Gsm_MakeCall()           
{
  Serial.println("ATD" + myphone + ";");       // Goi dien 
  delay(15000);                                // Sau 15s
  Serial.println("ATH");                       // Ngat cuoc goi
  delay(2000);
}
 
void Gsm_MakeSMS()
{
  Serial.println("AT+CMGS=\"" + myphone + "\"");     // Lenh gui tin nhan
  delay(5000);                                       // Cho ky tu '>' phan hoi ve 
  Serial.print("Nhiet do vuot nguong: ");                             // Gui noi dung
  Serial.print(Temperature);
  Serial.print(" *C");
  Serial.print((char)26);                            // Gui Ctrl+Z hay 26 de ket thuc noi dung tin nhan va gui tin di
  delay(5000);                                       // delay 5s
}