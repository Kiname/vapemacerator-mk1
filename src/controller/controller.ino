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
#define MOTOR_MS1         9 // See https://www.pololu.com/product/1182 for details
#define MOTOR_MS2         10
#define MOTOR_MS3         13
#define LCD_RX            6 // Not used for LCD but initialized by Softserial 
#define LCD_TX            7
#define ENCODER_A         3
#define ENCODER_B         2
#define ENCODER_BUTTON    12

#define LCD_BAUDRATE      9600

// All seting vslues.
int   setting_microstep         = 0;
char *setting_microstep_str[]   = {"1/1", "1/2", "1/4", "1/8", "1/16"};
int setting_rotatedir           = 0;
char *setting_rotatedir_str[]   = {"Left", "Right"};
int setting_speed               = 40;
int setting_mode                = 0;
char *setting_mode_str[]        = {"Constant Speed", "Flip-Flop","Shake"};

// ************************************************************************************************************
// Global Objects
// ************************************************************************************************************
// Define a stepper and the pins it will use
AccelStepper stepper; // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5

// Attach the serial display's RX line to digital pin 2
SoftwareSerial lcd(LCD_RX, LCD_TX); // pin 2 = TX, pin 3 = RX (unused)

// Encoder
Encoder encoder(ENCODER_A, ENCODER_B);

int menu_position = 0; // Default 0
int menu_mode     = 0; // 0: Select 1: Set value
bool isworking    = false;
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

void writeParams()
{
    EEPROM.write(2, setting_microstep);
    EEPROM.write(3, setting_rotatedir);
    EEPROM.write(4, setting_speed);
    EEPROM.write(5, setting_mode);  
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

  Serial.println("menu");
  char buf[16];                  

  while (menu_position > 0)
  {
    switch (menu_position)
    {
      case 0:
      {            
        LCDclear();
        LCDtext("VP-Macerator MK1",1);      
        LCDtext("PUSH TO START",2);
      }
      break;
  
      case 1:
      {            
        LCDclear();        
        LCDtext("Microstep:", 1);            
        LCDtext(setting_microstep_str[setting_microstep], 2);          // 1/1 [1/2] [1/4] [1/8] [1/16]
      }
      break;
  
      case 2:
      {            
        LCDclear();  
        LCDtext("Direction:",1);      
        LCDtext(setting_rotatedir_str[setting_rotatedir], 2);        // Left [Right]
      }
      break;
  
      case 3:
      {            
        LCDclear();  
        LCDtext("Speed:",1);
                
        sprintf(buf, "%3d%%", setting_speed);
        LCDtext(buf, 2);        // 1 - 250%
      }
      break;
  
      case 4:
      {            
        LCDclear();  
        LCDtext("Mode:",1);
        LCDtext(setting_mode_str[setting_mode],2);        // Constant Speed  [Flip-Flop] [Shake (Low)] [Shake (High)]
      }
      break;                
  
      case 5:
      {            
        LCDclear();  
        LCDtext("Default values:",1);
        LCDtext("RESET NOW",2);        // Constant Speed  [Flip-Flop] [Shake (Low)] [Shake (High)]
      }
      break;                    
    }  

    // Track mode
    if (menu_mode == 0)
    {
      // Mode change menu
      menu_position = getEncoderValue();    
      
      if (menu_position < 0) { menu_position=0; encoder.write(0); }
      if (menu_position > 5) { menu_position=5; encoder.write(5*4);}    
    }
    else if (menu_mode == 1)
    {
      LCDsetpos(1, 1);      
      // Mode change value
      switch (menu_position)
      {
        case 1:
          setting_microstep = getEncoderValue();         
          // Limit min/max values 
          if (setting_microstep > 4) { setting_microstep=4; encoder.write(4*4); }
          if (setting_microstep < 0) { setting_microstep=0; encoder.write(0); }                    
          break;
          
        case 2:          
          setting_rotatedir = getEncoderValue();         
          // Limit min/max values 
          if (setting_rotatedir > 1) { setting_rotatedir=1; encoder.write(1*4); }
          if (setting_rotatedir < 0) { setting_rotatedir=0; encoder.write(0); }        
          break;        
          
        case 3:
          setting_speed = getEncoderValue();  
          // Limit min/max values 
          if (setting_speed > 250) { setting_speed=250; encoder.write(250*4); }
          if (setting_speed < 1)   { setting_speed=1; encoder.write(1*4); }
          break;

        case 4:
          setting_mode = getEncoderValue();  
          // Limit min/max values 
          if (setting_mode > 2) { setting_mode=2; encoder.write(2*4); }
          if (setting_mode < 0) { setting_mode=0; encoder.write(0); }
          break;          
      }
    }

    if (!digitalRead(ENCODER_BUTTON))
    {
      if (menu_mode == 0) 
      { 
        menu_mode = 1; 
        if (menu_position == 1) { encoder.write(setting_microstep*4); }
        if (menu_position == 2) {  }        
        if (menu_position == 3) { encoder.write(setting_speed*4); }        
      }
      else 
      { 
        menu_mode = 0; 
        encoder.write(menu_position*4); 
      }
      delay(120);
      Serial.print("Menu mode: ");
      Serial.println(menu_mode);
    }    
        
  }
  Serial.println("Exit menu");
  LCDclear();
  writeParams();    
  LCDtext("VP-Macerator MK1",1);      
  LCDtext("PUSH TO START",2);  
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
  long newPosition = encoder.read()/4;
  if (newPosition != oldPosition) {
    oldPosition = newPosition;    
    return newPosition;
  }  
}

// ************************************************************************************************************
// Main loop
// ************************************************************************************************************
void loop()
{

  long newPosition = encoder.read()/4;
  if (newPosition < 0) { encoder.write(0); newPosition = 0; }
  if (newPosition != oldPosition) {
    oldPosition = newPosition;
    Serial.println(newPosition);
    menu_position = getEncoderValue();
    processMenu();
  }  
  
  
  if (!digitalRead(ENCODER_BUTTON))
  {
    if (menu_position == 0) 
    {  
      if (isworking == false)
      {        
        // Start motor here
        isworking = true; 
        stepper.setMaxSpeed(10000);
        stepper.setSpeed((3000*100)/setting_speed);
      }
      else
      {
        isworking = false;
      }
      delay(150);
    }
  }      

  if (isworking == true)
  {
    stepper.runSpeed();      
  }
  
}
