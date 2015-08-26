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

// Thanks to Mem for this! (http://forum.arduino.cc/index.php?topic=42211.0)
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)

/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)  
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN) 
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)  

#define CYCLE_MAX               54  // In steps of 10 mins. 54*10 = 540 minutes = 9 hours
#define DURATION_MAX            54  // In steps of 10 mins. 54*10 = 540 minutes = 9 hours

// All seting vslues.
int   setting_microstep         = 0;
char *setting_microstep_str[]   = {"1/1", "1/2", "1/4", "1/8", "1/16"};
int setting_speed               = 40;
int setting_cycle               = 2;
int setting_duration            = 2;
int setting_mode                = 0;
char *setting_mode_str[]        = {"Constant Speed", "Flip-Flop","Shake"};

char *rotator[4] = {"|", "/", "-", "\\" };
int rotator_count=0;

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
 * Byte 3: Cycle
 * Values:  1 to 50 
 *          Multiply by 10 to get total cycle duration in minutes: Example: 5*10 = 50 minutes = 0.83 hours or 24*10 = 120 minutes = 4 hours
 *          Display shows step of 10 minutes
 * 
 * Byte 4: Duration
 * Values:  1 to 100 
 *          Multiply by 10 to get total duration in minutes: Same as Cycle
 *          Display shows step of 10 minutes. Never less than Cycle duration!
 * 
 *  
 * Byte 5: Speed
 * Values:  0 to 100* 
 *          
 * 
 * Byte 6: Mode
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
    setting_cycle         = EEPROM.read(3);
    setting_duration      = EEPROM.read(4);
    setting_speed         = EEPROM.read(5);
    setting_mode          = EEPROM.read(6);  
  }
  else
  {
    // Header not found. Reset to default values
    Serial.println("Default values set!");
    EEPROM.write(0, 'M');
    EEPROM.write(1, 'K');
    EEPROM.write(2, setting_microstep);
    EEPROM.write(3, setting_cycle);
    EEPROM.write(4, setting_duration);
    EEPROM.write(5, setting_speed);
    EEPROM.write(6, setting_mode);
  }
  
}

void writeParams()
{
    EEPROM.write(2, setting_microstep);
    EEPROM.write(3, setting_cycle);
    EEPROM.write(4, setting_duration);
    EEPROM.write(5, setting_speed);
    EEPROM.write(6, setting_mode);  
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
        LCDtext("Macerator MK1",1);      
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
        LCDtext("Cycle:",1);      
        int hours = numberOfHours(setting_cycle*10*60);
        int minutes = numberOfMinutes(setting_cycle*10*60);
        sprintf(buf,"%02d Hours %02d Mins", hours, minutes);        
        
        LCDtext(buf, 2);        // Cycle in minutes
      }
      break;

      case 3:
      {            
        LCDclear();  
        LCDtext("Duration:",1);    

        int hours = numberOfHours(setting_duration*10*60);
        int minutes = numberOfMinutes(setting_duration*10*60);
        sprintf(buf,"%02d Hours %02d Mins", hours, minutes);
        
        LCDtext(buf, 2);        // Duration in minutes
      }
      break;      
  
      case 4:
      {            
        LCDclear();  
        LCDtext("Speed:",1);
                
        sprintf(buf, "%3d%%", setting_speed);
        LCDtext(buf, 2);        // 1 - 250%
      }
      break;
  
      case 5:
      {            
        LCDclear();  
        LCDtext("Mode:",1);
        LCDtext(setting_mode_str[setting_mode],2);        // Constant Speed  [Flip-Flop] [Shake (Low)] [Shake (High)]
      }
      break;                
  
      case 6:
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
      if (menu_position > 6) { menu_position=6; encoder.write(6*4);}    
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
          setting_cycle = getEncoderValue();         
          // Limit min/max values 
          if (setting_cycle > CYCLE_MAX) { setting_cycle=CYCLE_MAX; encoder.write(CYCLE_MAX*4); }
          if (setting_cycle < 1) { setting_cycle=1; encoder.write(1*4); } 

          // Cycle must be lower of equal to duration
          if (setting_cycle > setting_duration) { setting_duration=setting_cycle; }
          break;        

        case 3:          
          setting_duration = getEncoderValue();         
          // Limit min/max values 
          if (setting_duration > DURATION_MAX) { setting_duration=DURATION_MAX; encoder.write(DURATION_MAX*4); }
          if (setting_duration < 1) { setting_duration=1; encoder.write(1*4); }        

          // Duration must be equal or greater than Cycle
          if (setting_duration < setting_cycle) { setting_cycle=setting_duration; }
          break;                  
          
        case 4:
          setting_speed = getEncoderValue();  
          // Limit min/max values 
          if (setting_speed > 250) { setting_speed=250; encoder.write(250*4); }
          if (setting_speed < 1)   { setting_speed=1; encoder.write(1*4); }
          break;

        case 5:
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
        if (menu_position == 2) { encoder.write(setting_cycle*4); }        
        if (menu_position == 3) { encoder.write(setting_duration*4); }        
        if (menu_position == 4) { encoder.write(setting_speed*4); }        
        if (menu_position == 5) { encoder.write(setting_mode*4); }                
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
  LCDtext("Macerator MK1",1);      
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
    isworking = false; // Stop everything before menu
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
        stepper.setSpeed(setting_speed*10);
        LCDtext("      STOP       ",2);  
      }
      else
      {
        isworking = false;
        LCDtext("PUSH TO START",2);  
      }
      while (!digitalRead(ENCODER_BUTTON)) { } // Wait for release
      delay(150);
    }
  }      

  if (isworking == true)
  {
    stepper.runSpeed();      
    /*
    LCDtext("                ",2);  
    LCDtext(rotator[rotator_count],2);  

    rotator_count++;
    if (rotator_count > 4) { rotator_count=0; }
    */
  }
  
}
