/*
 * Vapemacerator Mk1
 * Oscar Gonzalez August 2015
 * https://github.com/oscargonzalez/vapemacerator-mk1
 * www.bricogeek.com 
 */

#include <AccelStepper.h>

// Define a stepper and the pins it will use
AccelStepper stepper; // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5

#include <SoftwareSerial.h>

// Attach the serial display's RX line to digital pin 2
SoftwareSerial mySerial(6,7); // pin 2 = TX, pin 3 = RX (unused)

void LCDclear()
{
  mySerial.write(254); // move cursor to beginning of first line
  mySerial.write(128);

  mySerial.write("                "); // clear display
  mySerial.write("                ");  
}

// Line: 1 or 2
// Pos: 0-15
void LCDsetpos(int line, int pos)
{
  int rp=0;
  if (line == 1) { rp = 128+pos; }
  if (line == 2) { rp = 192+pos; }
  mySerial.write(254); // move cursor to beginning of first line
  mySerial.write(rp);  
}

void setup()
{  
  mySerial.begin(9600);
   stepper.setMaxSpeed(3000);
   stepper.setSpeed(3000);

  LCDclear();
  LCDsetpos(1,0); 
  //mySerial.write("Hello, world!");
  LCDsetpos(2,0);
  mySerial.write("Macerator MK1");
   
}

void loop()
{
  
  stepper.runSpeed();

}
