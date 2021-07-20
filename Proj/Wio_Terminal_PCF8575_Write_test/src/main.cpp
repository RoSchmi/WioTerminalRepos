 // Working example PCF8574 
 // library: https://github.com/RobTillaart/PCF8574

 // https://www.nxp.com/docs/en/data-sheet/PCF8574_PCF8574A.pdf

 // The PCF8574 ia a "Remote 8-bit I/O expander for I2C-bus with interrupt"

 // You can write to and read from 8 parallel Input/Output pins

 

 // Write needed time to output 10000 bytes
#include <Arduino.h>
#include <Wire.h>
#include "PCF8574.h"

  // adjust addresses if needed
//PCF8574 pcf8574(0x20);   // For PCF8574

PCF8574 pcf8574(0x38);     // For PCF8574A

uint8_t byteToWrite = 0;

void jumpToEnd()
{
  Serial.println("Error !!, Halted");
  while (true)
  {

  }
}

void setup()
{
  Serial.begin(115200);
  while(!Serial);
  Wire.begin();
  Wire.setClock(400000);
  //Wire.setClock(100000);
  Serial.println(__FILE__);
  Serial.print("PCF8574_LIB_VERSION:\t");
  Serial.println(PCF8574_LIB_VERSION);

// Set pinMode to OUTPUT
	
  pcf8574.begin();
  int x = pcf8574.read8();
  Serial.print("Read ");
  Serial.println(x, HEX);
  delay(1000);
}

void loop() {
  uint32_t start = millis();
  Serial.println("Output of 10000 bytes");

  for (int i =0; i < 10000; i++)
  {
    pcf8574.write8(byteToWrite++);
  }
  Serial.print("Needed Millis: ");
  Serial.println(millis() - start);

  pcf8574.write8(0xff);

  while (true)
  {
    
  
  Serial.println("High");
  pcf8574.write(4, HIGH);
  int x = pcf8574.read8();
  Serial.print("Read ");
  Serial.println(x, HEX);
  if (x != 0xFF)
  {
    jumpToEnd();
  }
  
  delay(5000);
  
  pcf8574.write(4, LOW);
  delay(10);
  x = pcf8574.read8();
  Serial.print("Read Low ");
  Serial.println(x, HEX);
  if (x != 0xEF)
  {
    jumpToEnd();
  }
  delay(5000);
  
  }

}
