#include "macros.h"
#include "fastio.h"

// X-axis: 
// Belt: 5.08mm pitch. 
// Pulley: 10 teeth
// Stepper: 1.8 deg / step = 200 steps / round.
// 200 / (5.08*10) * 25.4 * 2 * 2

#define X_DPI 400

// Y-axis:
// Lead screw 3mm pitch
// Stepper: 1.8 deg / step = 200 steps / round.
// 200 / 3 * 25.4 

#define Y_DPI 1693

#define X_STEP_PIN         54
#define X_DIR_PIN          55
#define X_ENABLE_PIN       38
#define X_CS_PIN           53

#define Y_STEP_PIN         60
#define Y_DIR_PIN          61
#define Y_ENABLE_PIN       56
#define Y_CS_PIN           49

#define X_ENDSTOP          3
#define Y_ENDSTOP          14
#define LASER 11

#define ENABLE_STEPPER_DRIVER_INTERRUPT()  SBI(TIMSK1, OCIE1A)
#define DISABLE_STEPPER_DRIVER_INTERRUPT() CBI(TIMSK1, OCIE1A)

void setup() {

  SET_OUTPUT(X_STEP_PIN);
  SET_OUTPUT(X_DIR_PIN);
  SET_OUTPUT(X_ENABLE_PIN);
//  SET_OUTPUT(X_CS_PIN);

  WRITE(X_STEP_PIN,LOW);
  WRITE(X_DIR_PIN,LOW);
  WRITE(X_ENABLE_PIN,LOW);
//  WRITE(X_CS_PIN,0);

  SET_OUTPUT(Y_STEP_PIN);
  SET_OUTPUT(Y_DIR_PIN);
  SET_OUTPUT(Y_ENABLE_PIN);
//  SET_OUTPUT(X_CS_PIN);

  WRITE(Y_STEP_PIN,LOW);
  WRITE(Y_DIR_PIN,LOW);
  WRITE(Y_ENABLE_PIN,LOW);
//  WRITE(X_CS_PIN,0);

  pinMode(X_ENDSTOP, INPUT_PULLUP); 
  pinMode(Y_ENDSTOP, INPUT_PULLUP); 
  
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LASER, OUTPUT);
  setupInterrupt();
  
  WRITE(X_DIR_PIN,HIGH);
  WRITE(LASER,LOW);
  Serial.begin(9600);
  Serial.write("Setup done.");
}

void setupInterrupt()
{
 // cli();
  // waveform generation = 0100 = CTC
  CBI(TCCR1B, WGM13);
  SBI(TCCR1B, WGM12);
  CBI(TCCR1A, WGM11);
  CBI(TCCR1A, WGM10);

  // output mode = 00 (disconnected)
  TCCR1A &= ~(3 << COM1A0);
  TCCR1A &= ~(3 << COM1B0);

  // Set the timer pre-scaler
  // Generally we use a divider of 8, resulting in a 2MHz timer
  // frequency on a 16MHz MCU. If you are going to change this, be
  // sure to regenerate speed_lookuptable.h with
  // create_speed_lookuptable.py
  TCCR1B = (TCCR1B & ~(0x07 << CS10)) | (4 << CS10);

  // Init Stepper ISR to 122 Hz for quick starting
  OCR1A = 0x01;
  TCNT1 = 0;
  
  //sei();
  ENABLE_STEPPER_DRIVER_INTERRUPT();
}

#define MODE_NONE 0
#define MODE_PRINT 1
#define MODE_RETURN 2
volatile byte mode=0;
// divider, smaller is faster
volatile byte maxSpeed=80;
volatile unsigned int maxPosition=1000;

#define DATA_BUFFER_SIZE 5000
volatile byte dataBuffer[DATA_BUFFER_SIZE];

int direction=0;
unsigned int divi=0;
unsigned int maxDivi=2000;
unsigned long position=0;


#define BUFFER_BIT_ROLL 3

#define GET_STEPPER_POSITION(POS) (POS)
#define GET_BUFFER_POSITION(POS) (POS >> BUFFER_BIT_ROLL)



ISR(TIMER1_COMPA_vect)
{

  if(mode==MODE_NONE)
  {  
    return;
  }

  if((direction != 1) && mode==MODE_PRINT)
  {
    direction=1;
    WRITE(X_DIR_PIN,HIGH);
  }
    
  divi++;
  if(divi<maxDivi)
    return;  
  divi=0;

  position+=direction;

  if(position==maxPosition)
  {
    direction=-1;    
    WRITE(X_DIR_PIN,LOW);
    mode=MODE_RETURN;
    WRITE(LASER,LOW);
  }
  
  if(position==0 && mode==MODE_RETURN)
  {
    direction=0;    
    mode=MODE_NONE;
  }

  int bi = GET_BUFFER_POSITION(position) % 8;
  byte d=dataBuffer[GET_BUFFER_POSITION(position) / 8];

  if(mode==MODE_PRINT)
    WRITE(LASER,(d & ( 1 << bi)) ? HIGH : LOW );

  WRITE(X_STEP_PIN,GET_STEPPER_POSITION(position) & 1);  

  int distance;

  if(position<(maxPosition/2))
    distance=position;
  else
    distance=maxPosition-position;

  long tmp=200-(distance*5);
  if(tmp<maxSpeed)
    tmp=maxSpeed;

  if(tmp>200)
    tmp=200;

  maxDivi=tmp;
}


void loop() {

  /*
  WRITE(X_DIR_PIN,LOW);
  delay(1);
  WRITE(X_STEP_PIN,HIGH);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1);
  WRITE(X_STEP_PIN,LOW);
  digitalWrite(LED_BUILTIN, LOW);


  if(digitalRead(X_ENDSTOP))  
    Serial.write("u");
  else
    Serial.write("d");

  if(digitalRead(Y_ENDSTOP))  
    Serial.write("a");
  else
    Serial.write("b");
*/
  
  if( Serial.available())
  {
    char ch = Serial.read();

    // Print - Receives length of array and then array of data. Prints row.
    if(ch=='P')
    {
      if(readBuffer())
      {
        mode=MODE_PRINT;
        while(mode!=MODE_NONE)
          ;
        Serial.write("OK\r\n");
      }
      else
        Serial.write("Error\r\n");        
    }
    //Move - Moves X or Y axis count of steps. Example MX+0010    
    else if(ch=='M')
    {
      if(MoveAxis())
        Serial.write("OK\r\n");
      else
        Serial.write("Error\r\n");  
    }
    else if(ch=='T')
    {
      maxPosition=400;
      mode=MODE_PRINT;
      while(mode!=MODE_NONE)
        ;
      Serial.write("OK\r\n");
    }
    // Laser on/off. Example L1
    else if(ch=='L')
    {
      unsigned int val;
      if(!ReadValue(&val,1))
        Serial.write("Error\r\n");
      else
      {
        WRITE(LASER,val ? HIGH : LOW );
        Serial.write("OK\r\n");  
      }
    }
    // Get capabilities 
    else if(ch=='X')
    {
      sendHex(X_DPI,4);
      sendHex(Y_DPI,4);      
      Serial.write("\r\nOK\r\n");        
    }
    // Set print speed - Example S0010 lower number is faster
    else if(ch=='S')
    {
      unsigned int val;
      if(!ReadValue(&val,4))
        Serial.write("Error\r\n");
      else
      {
        maxSpeed=val;
        Serial.write("OK\r\n");  
      }
    }
    // Read speed setting
    else if(ch=='R')
    {
        sendHex(maxSpeed,4);
        Serial.write("\r\nOK\r\n");        
    }
    else if(ch=='Z')
    {
      MoveToZero();
      Serial.write("\r\nOK\r\n");
    }
  }
}

char hexCodes[] ="0123456789ABCDEF";

void sendHex(unsigned int val,int len)
{
  for(int i=len-1;i>=0;i--)  
    Serial.write(hexCodes[((val >> (i * 4)) & 0x0f)]); 
}

void MoveToZero()
{
    while(!Serial.available());  
    char axis = Serial.read();
    if(axis!='X' && axis!='Y')
      return(false);

    if(axis=='X')
    {
      WRITE(X_DIR_PIN,LOW);
      for(int i=0;i<10000;i++)
      {
        WRITE(X_STEP_PIN,i & 1);
        delay(2);
        if(digitalRead(X_ENDSTOP))
          break;
      }          
    }

    if(axis=='Y')
    {
      WRITE(Y_DIR_PIN,LOW);
      for(int i=0;i<100000;i++)
      {
        WRITE(Y_STEP_PIN,i & 1);
        delay(1);
        if(digitalRead(Y_ENDSTOP))
          break;
      }          
    }
}

bool MoveAxis()
{
    while(!Serial.available());  
    char axis = Serial.read();
    if(axis!='X' && axis!='Y')
      return(false);
      
    while(!Serial.available());  
    char dir = Serial.read();
    if(dir!='+' && dir!='-')
      return(false);
      
    unsigned int steps;
    if(!ReadValue(&steps,4))
      return(false);

    if(axis=='X')  
      WRITE(X_DIR_PIN,dir == '+' ? HIGH : LOW);

    if(axis=='Y')  
      WRITE(Y_DIR_PIN,dir == '+' ? HIGH : LOW);

    for(int i=0;i<steps*2;i++)
    {
      if(axis=='X')  
        WRITE(X_STEP_PIN,i & 1);

      if(axis=='Y')  
        WRITE(Y_STEP_PIN,i & 1);  

      delay(1);           
    }
    return(true);
}

bool readBuffer()
{
  unsigned int dataPtr=0;
  unsigned int len;
  if(!ReadValue(&len,4))
  {
    Serial.write("Len");
    return(false);
  }
  maxPosition=(len*8) << BUFFER_BIT_ROLL;
  for(int i=0;i<len;i++)
  {
    unsigned int val;  
    if(!ReadValue(&val,2))
    {
      Serial.write("Val");    
      return(false);
    }
    dataBuffer[dataPtr]=(byte)val;
    dataPtr++;
    if(dataPtr>DATA_BUFFER_SIZE)
      return(false);
  }
  return(true);
}



bool ReadValue(unsigned int *retval,byte bytes)
{
  *retval=0;
  
  for(int i=0;i<bytes;i++)
  {
    while(!Serial.available());  
    char ch = Serial.read();
    int val=HexToVal(ch);
    if(val==-1)
      return(false);
    *retval=(*retval) << 4;
    *retval=(*retval) | val;      
  }
  return(true);  
}

int HexToVal(char ch)
{
  if(ch>='0' && ch <='9')
    return((int)ch-'0');

  if(ch>='A' && ch <='F')
    return((int)ch-'A'+10);

  return(-1);    
}
