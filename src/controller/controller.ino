/*
 * Vapemacerator Mk1
 * Oscar Gonzalez August 2015
 * https://github.com/oscargonzalez/vapemacerator-mk1
 * www.bricogeek.com 
 */
#include <AccelStepper.h>
#include <Encoder.h>
#include <SoftwareSerial.h>
#include <EEPROM.h>

//#define ENCODER_DO_NOT_USE_INTERRUPTS
//#define ENCODER_OPTIMIZE_INTERRUPTS

// ************************************************************************************************************
// Define arduino pins
// ************************************************************************************************************
#define MOTOR_DIR         4
#define MOTOR_STEP        5 
#define MOTOR_MS1         0 // See https://www.pololu.com/product/1182 for details
#define MOTOR_MS2         0
#define MOTOR_MS3         0
#define LCD_RX            6 // Not used for LCD but initialized by Softserial 
#define LCD_TX            7
#define ENCODER_A         3
#define ENCODER_B         2
#define ENCODER_BUTTON    12

#define LCD_BAUDRATE      9600

// All seting vslues.
int setting_microstep     = 0;
int setting_rotatedir     = 0;
int setting_speed         = 0;
int setting_mode          = 0;

// ************************************************************************************************************
// Global Objects
// ************************************************************************************************************
// Define a stepper and the pins it will use
AccelStepper stepper; // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5

// Attach the serial display's RX line to digital pin 2
SoftwareSerial lcd(LCD_RX, LCD_TX); // pin 2 = TX, pin 3 = RX (unused)

// Encoder
Encoder encoder(ENCODER_A, ENCODER_B);

int menu_position=0; // Default 0
long oldPosition  = -999;

// ************************************************************************************************************
// Memory functions
// Memory map: (all sectors are 1 byte length)
/*
 * Byte 0, 1: Header (fixed value)
 * value: MK (2bytes)
 * 
 * Byte 2: Microstep 
 * Values:  0     (1/1)
 *          1     (1/2)
 *          2     (1/4)
 *          3     (1/8)
 *          4     (1/16)
 *          
 * Byte 3: Rotate dir        
 * Values:  0     (Left)
 *          1     (Right)
 *          
 * Byte 4: Speed
 * Values:  0 to 100
 * 
 * Byte 5: Mode
 * Values:  0     (Constant Speed)
 *          1     (Flip-Flop)
 *          2     (Shake)
*/
// ************************************************************************************************************
void readParams()
{
  // First check 2 bytes for "MK"
  if ( (EEPROM.read(0) == 'M') && (EEPROM.read(1) == 'K') )
  {
    Serial.println("Read values...");
    setting_microstep     = EEPROM.read(2);
    setting_rotatedir     = EEPROM.read(3);
    setting_speed         = EEPROM.read(4);
    setting_mode          = EEPROM.read(5);  
  }
  else
  {
    // Header not found. Reset to default values
    Serial.println("Default values set!");
    EEPROM.write(0, 'M');
    EEPROM.write(1, 'K');
    EEPROM.write(2, setting_microstep);
    EEPROM.write(3, setting_rotatedir);
    EEPROM.write(4, setting_speed);
    EEPROM.write(5, setting_mode);
  }
  
}

// ************************************************************************************************************
// LCD funcions
// ************************************************************************************************************
void LCDclear()
{
  lcd.write(254); // move cursor to beginning of first line
  lcd.write(128);

  lcd.write("                "); // clear display
  lcd.write("                ");  
}

// Line: 1 or 2
// Pos: 0-15
void LCDsetpos(int line, int pos)
{
  int rp=0;
  if (line == 1) { rp = 128+pos; }
  if (line == 2) { rp = 192+pos; }
  lcd.write(254); // move cursor to beginning of first line
  lcd.write(rp);  
}

// Centered text (LCD 16x2)
void LCDtext(char *strText, int line)
{
  int xpos = 8-(strlen(strText)/2);
  LCDsetpos(line, xpos);  
  lcd.write(strText);  
}

// Process LCD menu
void processMenu()
{
  LCDclear();
  if (menu_position < 0) { menu_position=0; encoder.write(0); }
  if (menu_position > 5) { menu_position=5; encoder.write(5*4);}
  switch (menu_position)
  {
    case 0:
    {            
      LCDtext("VP-Macerator MK1",1);      
      LCDtext("PUSH TO START",2);
    }
    break;

    case 1:
    {            
      LCDtext("Microstep:", 1);            
      LCDtext("1/1", 2);          // 1/1 [1/2] [1/4] [1/8] [1/16]
    }
    break;

    case 2:
    {      
      LCDtext("Rotate way:",1);      
      LCDtext("Left",2);        // Left [Right]
    }
    break;

    case 3:
    {      
      LCDtext("Speed:",1);
      LCDtext("80%",2);        // 1 - 100%
    }
    break;

    case 4:
    {      
      LCDtext("Mode:",1);
      LCDtext("Constant Speed",2);        // Constant Speed  [Flip-Flop] [Shake (Low)] [Shake (High)]
    }
    break;                

    case 5:
    {      
      LCDtext("Default values:",1);
      LCDtext("RESET NOW",2);        // Constant Speed  [Flip-Flop] [Shake (Low)] [Shake (High)]
    }
    break;                    
  }

}

// ************************************************************************************************************
// Main Setup function
// ************************************************************************************************************
void setup()
{  
  // Setup pins
  pinMode(ENCODER_A,INPUT_PULLUP);
  pinMode(ENCODER_B,INPUT_PULLUP);
  pinMode(ENCODER_BUTTON,INPUT_PULLUP);
  
  pinMode(MOTOR_MS1,OUTPUT);
  pinMode(MOTOR_MS2,OUTPUT);
  pinMode(MOTOR_MS3,OUTPUT);
  
  lcd.begin(LCD_BAUDRATE);
  
  Serial.begin(9600);

  readParams();

  LCDclear();
  processMenu();
   
}

int getEncoderValue()
{
  long newPosition = encoder.read();
  if (newPosition != oldPosition) {
    oldPosition = newPosition;    
    return newPosition/4;
  }  
}

// ************************************************************************************************************
// Main loop
// ************************************************************************************************************
void loop()
{

  long newPosition = encoder.read();
  if (newPosition != oldPosition) {
    menu_position = getEncoderValue();
    processMenu();
  }  
  
  /*
  if (!digitalRead(ENCODER_BUTTON))
  {
    if (menu_position == 0) { menu_position=1; processMenu(); }    
  }
  */
}
