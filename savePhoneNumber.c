#include <GSM.h>
GSM gsmAccess(true);
int sizer = 200;
char myNumber[200];
int timeout = 5000; // in milli seconds

void setup()
{
  Serial.begin(9600);

  boolean notConnected = true;

  Serial.println("Connecting to the GSM network");

  while(notConnected){
    if(gsmAccess.begin() == GSM_READY) // Note: I do not require PIN #
      notConnected = false;
    else {
      Serial.println("Not connected, trying again");
      delay(1000);
    }
  }

  Serial.println("Connected");

  theGSM3ShieldV1ModemCore.println("AT+CPBS=\"SM\""); 
  int start1 = millis();   
  while((millis() - start1) < timeout){
    theGSM3ShieldV1ModemCore.theBuffer().read();
  }   
  Serial.print("Set to look at SIM card storage");

  // search for contact name "test"
  theGSM3ShieldV1ModemCore.println("AT+CPBF=\"test\""); 
  start1 = 0;
  start1 = millis();   
  while((millis() - start1) < timeout && !theGSM3ShieldV1ModemCore.theBuffer().extractSubstring(",\"", "\",", myNumber, sizer)){
    theGSM3ShieldV1ModemCore.theBuffer().read();
  }   
  Serial.print("Got contact number");

  // print out the phone of "test"
  Serial.println(myNumber);

}

void loop()
{

}