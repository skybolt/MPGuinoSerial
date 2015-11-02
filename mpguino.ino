// Opengauge MPGuino code, modified by Meelis Pärjasaar
// webpage - http://mpguino.wiseman.ee

#include "mpguino_conf.h"
#include <avr/pgmspace.h>
#include <EEPROM.h>
#include "mpguino.h"
#include "lcd.h"

#if( SLEEP_CFG == 3 ) 
#include <avr/sleep.h>
#endif

#if (OUTSIDE_TEMP_CFG == 1)
#include "temperature.h"
#endif

/* --- Global Variable Declarations -------------------------- */

static unsigned long MAXLOOPLENGTH = 0; // see if we are overutilizing the CPU 
unsigned long LASTLOOPLENGTH = 0;  
unsigned long screenDelay = 600;
static unsigned long maxDelay = 0;
static boolean  setupRan = 0;  
//static long genericTimer = 0;
//static long idleTimer = 0;
//static long gphTimer = 0;
//static unsigned long lastGoodRPM = 433000; 
//static unsigned long lastGoodGPH = 0; 
//static unsigned long loopStart;


//for display computing
static unsigned long tmp1[2];
static unsigned long tmp2[2];
static unsigned long tmp3[2];

#if (CFG_BIGFONT_TYPE == 1)
  static char chars[] PROGMEM = {
    B11111, B00000, B11111, B11111, B00000,
    B11111, B00000, B11111, B11111, B00000,
    B11111, B00000, B11111, B11111, B00000,
    B00000, B00000, B00000, B11111, B00000,
    B00000, B00000, B00000, B11111, B00000,
    B00000, B11111, B11111, B11111, B01110,
    B00000, B11111, B11111, B11111, B01110,
    B00000, B11111, B11111, B11111, B01110};
#elif (CFG_BIGFONT_TYPE == 2)
  /* XXX: For whatever reason I can not figure out how 
   * to store more than 8 chars in the LCD CGRAM */
  static char chars[] PROGMEM = {
    B11111, B00000, B11111, B11111, B00000, B11111, B00111, B11100, 
    B11111, B00000, B11111, B11111, B00000, B11111, B01111, B11110, 
    B00000, B00000, B00000, B11111, B00000, B11111, B11111, B11111, 
    B00000, B00000, B00000, B11111, B00000, B11111, B11111, B11111, 
    B00000, B00000, B00000, B11111, B00000, B11111, B11111, B11111, 
    B00000, B00000, B00000, B11111, B01110, B11111, B11111, B11111,
    B00000, B11111, B11111, B01111, B01110, B11110, B11111, B11111,
    B00000, B11111, B11111, B00111, B01110, B11100, B11111, B11111};
#endif

#if (BARGRAPH_DISPLAY_CFG == 1)
  const unsigned char LcdBarChars = 8;
  static char barchars[] PROGMEM = {
    B00000, B00000, B00000, B00000, B00000, B00000, B00000, B11111,
    B00000, B00000, B00000, B00000, B00000, B00000, B01111, B01111,
    B00000, B00000, B00000, B00000, B00000, B01111, B01111, B01111, 
    B00000, B00000, B00000, B00000, B01111, B01111, B01111, B01111,
    B00000, B00000, B00000, B11111, B11111, B11111, B11111, B11111,
    B00000, B00000, B01111, B01111, B01111, B01111, B01111, B01111,
    B00000, B01111, B01111, B01111, B01111, B01111, B01111, B01111,
    B01111, B01111, B01111, B01111, B01111, B01111, B01111, B01111};

  /* map numbers to bar segments.  Example:
   * ascii_barmap[10] --> all eight segments filled in
   * ascii_barmap[4]  --> four segments filled in */
  char ascii_barmap[] = {0x20, 0x01, 0x02, 0x03, 0x04, 0x05, 
                         0x06, 0x07, 0x08, 0xFF, 0xFF}; 
#endif

#if (CFG_BIGFONT_TYPE == 1)
   /* 32 = 0x20 = space */
   const unsigned char LcdNewChars = 5;
   char bignumchars1[]={4,1,4,0, 1,4,32,0, 3,3,4,0, 1,3,4,0, 4,2,4,0, 
                        4,3,3,0,  4,3,3,0, 1,1,4,0, 4,3,4,0, 4,3,4,0}; 
   char bignumchars2[]={4,2,4,0, 2,4,2,0,   4,2,2,0, 2,2,4,0, 32,32,4,0, 
                        2,2,4,0, 4,2,4,0, 32,4,32,0, 4,2,4,0,   2,2,4,0};  
#elif (CFG_BIGFONT_TYPE == 2)
   /* 32 = 0x20 = space */
   /* 255 = 0xFF = all black character */
   const unsigned char LcdNewChars = 8;
   char bignumchars1[]={  7,1,8,0,  1,255,32,0,   3,3,8,0, 1,3,8,0, 255,2,255,0,  
                        255,3,3,0,     7,3,3,0,   1,1,6,0, 7,3,8,0,     7,3,8,0};
   char bignumchars2[]={  4,2,6,0, 32,255,32,0, 255,2,2,0, 2,2,6,0, 32,32,255,0,
                          2,2,6,0,     4,2,6,0, 32,7,32,0, 4,2,6,0,     2,2,6,0};
#endif

//middle button cycles through these brightness settings      
//unsigned char brightness[]={255,220,150,10}; // moved to mpguino.h
unsigned char brightnessIdx=3;

#define brightnessLength (sizeof(brightness)/sizeof(unsigned char)) //array size

volatile unsigned long timer2_overflow_count;

/* --- End Global Variable Declarations ---------------------- */


/*** Set up the Events ***
We have our own ISR for timer2 which gets called about once a millisecond.
So we define certain event functions that we can schedule by calling addEvent
with the event ID and the number of milliseconds to wait before calling the event. 
The milliseconds is approximate.

Keep the event functions SMALL!!!  This is an interrupt!
*/

//event functions
void enableLButton(){PCMSK1 |= (1 << PCINT11);}
void enableMButton(){PCMSK1 |= (1 << PCINT12);}
void enableRButton(){PCMSK1 |= (1 << PCINT13);}
//array of the event functions
pFunc eventFuncs[] ={enableVSS, enableLButton,enableMButton,enableRButton};
#define eventFuncSize (sizeof(eventFuncs)/sizeof(pFunc)) 
//define the event IDs
#define enableVSSID 0
#define enableLButtonID 1
#define enableMButtonID 2
#define enableRButtonID 3

#define useDebug false

//ms counters
unsigned int eventFuncCounts[eventFuncSize];

//schedule an event to occur ms milliseconds from now
void addEvent(unsigned char eventID, unsigned int ms){
   if(ms == 0) {
      eventFuncs[eventID]();
   }
   else {
      eventFuncCounts[eventID]=ms;
   }
}

/* this ISR gets called every 1.024 milliseconds, we will call that a 
 * millisecond for our purposes go through all the event counts, if any 
 * are non zero subtract 1 and call the associated function if it just 
 * turned zero.  */
ISR(TIMER2_OVF_vect) {
   unsigned char eventID;
   timer2_overflow_count++;
   for(eventID = 0; eventID < eventFuncSize; eventID++) {
      if(eventFuncCounts[eventID]!= 0) {
         eventFuncCounts[eventID]--;
         if(eventFuncCounts[eventID] == 0) {
            eventFuncs[eventID](); 
         }
      }  
   }
} /* ISR(TIMER2_OVF_vect) */

unsigned char buttonState = buttonsUp;      
 
//overflow counter used by millis2()      
unsigned long lastMicroSeconds=millis2() * 1000;   
unsigned long microSeconds(void) {     
   unsigned long tmp_timer2_overflow_count;    
   unsigned long tmp;    
   unsigned char tmp_tcnt2;    
   cli(); //disable interrupts    
   tmp_timer2_overflow_count = timer2_overflow_count;    
   tmp_tcnt2 = TCNT2;    
   sei(); // enable interrupts    
   tmp = ((tmp_timer2_overflow_count << 8) + tmp_tcnt2) * 4;     
   if((tmp<=lastMicroSeconds) && (lastMicroSeconds<4290560000ul)) {
      return microSeconds();     
   }
   lastMicroSeconds=tmp;   
   return tmp;     
}    
 
unsigned long elapsedMicroseconds(unsigned long startMicroSeconds, unsigned long currentMicroseconds) {      
   if(currentMicroseconds >= startMicroSeconds) {
      return currentMicroseconds-startMicroSeconds;      
   }
   return 0xFFFFFFFF - (startMicroSeconds-currentMicroseconds);      
}      

unsigned long elapsedMicroseconds(unsigned long startMicroSeconds ){      
   return elapsedMicroseconds(startMicroSeconds, microSeconds());
}      
 
 
//main objects we will be working with:      
unsigned long injHiStart; //for timing injector pulses      
Trip tmpTrip;      
Trip instant;      
Trip current;      
Trip tank;
#if (BARGRAPH_DISPLAY_CFG == 1)
Trip periodic;
#endif
#if (DRAGRACE_DISPLAY_CFG)
Drag myDrag;
#endif

unsigned volatile long instInjStart=nil; 
unsigned volatile long tmpInstInjStart=nil; 
unsigned volatile long instInjEnd; 
unsigned volatile long tmpInstInjEnd; 
unsigned volatile long instInjTot; 
unsigned volatile long tmpInstInjTot;     
unsigned volatile long instInjCount; 
unsigned volatile long tmpInstInjCount;     

boolean lookForOpenPulse = true;

void processInjOpen(void){      
  //20110831 AJT Peak and Hold ;)
  if(lookForOpenPulse || parms[injEdgeIdx] != 2){
    injHiStart = microSeconds();
  }
}      

void processInjClosed(void){
  //20110831 AJT Peak and Hold ;)
  long t =  microSeconds();
  long x = elapsedMicroseconds(injHiStart, t) - parms[injectorSettleTimeIdx];   

  if( parms[injEdgeIdx]==3 ){
    if (x >0) {
      tmpTrip.injHius += x;
    }
    tmpTrip.injPulses++;      
    if (tmpInstInjStart != nil) {
      if (x >0) {
        tmpInstInjTot += x;
      }
      tmpInstInjCount++;
    } 
    else {
      tmpInstInjStart = t;
    }
    tmpInstInjEnd = t;
  }
  else {
    if(parms[injEdgeIdx]==2 && x < 0){
      lookForOpenPulse=false;
    } 
    else {
      if(parms[injEdgeIdx]==2 || x > 0){
        tmpTrip.injHius += x;
      }

      tmpTrip.injPulses++;  
      if (tmpInstInjStart != nil) {
        if(parms[injEdgeIdx]==2 || x > 0){
          tmpInstInjTot += x;     
        }
        tmpInstInjCount++;
      } 
      else {
        tmpInstInjStart = t;
      }  
      tmpInstInjEnd = t;
      lookForOpenPulse=true;
    }
  }
}

volatile boolean vssFlop = 0;

void enableVSS(){
   vssFlop = !vssFlop;
}

unsigned volatile long lastVSS1;
unsigned volatile long lastVSSTime;
unsigned volatile long lastVSS2;

volatile boolean lastVssFlop = vssFlop;

//attach the vss/buttons interrupt      
ISR(PCINT1_vect) {    
#if (useDebug)
   Serial.println(("PCINT1_vect"); 
#endif
   static unsigned char vsspinstate=0;      
   unsigned char p = PINC;//bypassing digitalRead for interrupt performance      

   if ((p & vssBit) != (vsspinstate & vssBit)){      
      addEvent(enableVSSID,parms[vsspause] ); //check back in a couple milli
   }
   if(lastVssFlop != vssFlop){
      #if (DRAGRACE_DISPLAY_CFG)
      if(myDrag.running) {
         myDrag.update();
      }
      else if(myDrag.waiting_start) {
         myDrag.start();
      }
      #endif

      lastVSS1=lastVSS2;
      unsigned long t = microSeconds();
      lastVSS2=elapsedMicroseconds(lastVSSTime,t);
      lastVSSTime=t;
#if (UNO_MODIFICATIONS == 1)
      tmpTrip.vssPulses = tmpTrip.vssPulses + 10000000;
#else
      tmpTrip.vssPulses++; 
#endif
      tmpTrip.vssPulseLength += lastVSS2;
      lastVssFlop = vssFlop;
   }
   vsspinstate = p;      
   buttonState &= p;      
} /* ISR(PCINT1_vect) */
 
 
pFunc displayFuncs[] ={ 
   doDisplayInstantCurrent, //0
   doDisplayInstantTank,  //1
   #if CFG_BIGFONT_TYPE > 0  
   doDisplayBigInstant,     //
   doDisplayBigSpeed,     //
   doDisplayCurrentTripData, //2
   #endif
   doDisplayTankTripData,    //3  2
   doDisplayEOCIdleData,     //4  3
   doDisplaySystemInfo,      //5  4
   doDisplayCarSensors,      //6  5
   #if (BARGRAPH_DISPLAY_CFG == 1)
   doDisplayBarGraph,        //7  6
   #endif
   #if (DRAGRACE_DISPLAY_CFG)
   doDisplayDragRace,        //8  7
   #endif 
};      

#define displayFuncSize (sizeof(displayFuncs)/sizeof(pFunc)) //array size      

prog_char  * displayFuncNames[displayFuncSize]; 
unsigned char newRun = 0;
#if (DRAGRACE_DISPLAY_CFG)
unsigned char dragSceenIdx;
#endif

void setup (void) {
   unsigned char x = 0;

   CLOCK = 0;
   #if UNO_MODIFICATIONS == 1
   SCREEN = 4;
   #else 
   SCREEN = 5;
   #endif
   #if (useDebug)
   SCREEN = 3;
   #endif
   HOLD_DISPLAY = 0;

   #if (CFG_IDLE_MESSAGE != 0)
   IDLE_DISPLAY_DELAY = 0;
   #endif

   init2();
   newRun = load();//load the default parameters

//   displayFuncNames[x++]=  PSTR("Custom"); 
   displayFuncNames[x++]=  PSTR("Instant/Current"); 
   displayFuncNames[x++]=  PSTR("Instant/Tank"); 
   #if CFG_BIGFONT_TYPE > 0
   displayFuncNames[x++]=  PSTR("BIG Instant"); 
   //displayFuncNames[x++]=  PSTR("BIG Current"); 
   displayFuncNames[x++]=  PSTR("BIG Speed");
   //displayFuncNames[x++]=  PSTR("BIG Tank");
   displayFuncNames[x++]=  PSTR("Current");  
   #endif
   displayFuncNames[x++]=  PSTR("Tank"); 
   #if (CFG_UNITS == 2)
   displayFuncNames[x++]=  PSTR("EOC km/Idle Litr");
   #else
   displayFuncNames[x++]=  PSTR("eco mil/Idle Gal");
   #endif


   displayFuncNames[x++]=  PSTR("CPU Monitor");
   displayFuncNames[x++]=  PSTR("Car Sensors");    
   #if (BARGRAPH_DISPLAY_CFG == 1)
   displayFuncNames[x++]=  PSTR(BARGRAPH_LABEL);
   #endif
   #if (DRAGRACE_DISPLAY_CFG)
   dragSceenIdx = x;
   displayFuncNames[x++]=  PSTR("Drag Race");
   #endif 

   pinMode(BrightnessPin,OUTPUT);      
   analogWrite(BrightnessPin,brightness[brightnessIdx]);      
   pinMode(EnablePin,OUTPUT);       
   pinMode(DIPin,OUTPUT);       
   pinMode(DB4Pin,OUTPUT);       
   pinMode(DB5Pin,OUTPUT);       
   pinMode(DB6Pin,OUTPUT);       
   pinMode(DB7Pin,OUTPUT);       

   //delay2(500);      

   pinMode(ContrastPin,OUTPUT);      
   analogWrite(ContrastPin,parms[contrastIdx]);  
   LCD::init();      
   LCD::LcdCommandWrite(LCD_ClearDisplay);            // clear display, set cursor position to zero         
   LCD::LcdCommandWrite(LCD_SetDDRAM);                // set dram to zero
   LCD::print(getStr(PSTR("OpenGauge")));      
   LCD::gotoXY(2,1);      
   LCD::print(getStr(PSTR(STARTUP_TEXT)));

   pinMode(InjectorOpenPin, INPUT);       
   pinMode(InjectorClosedPin, INPUT);       
   pinMode(VSSPin, INPUT);            
   attachInterrupt(0, processInjOpen, parms[injEdgeIdx]==1? RISING:FALLING);      
   attachInterrupt(1, processInjClosed, parms[injEdgeIdx]==1? FALLING:RISING); 

   pinMode( lbuttonPin, INPUT );       
   pinMode( mbuttonPin, INPUT );       
   pinMode( rbuttonPin, INPUT );      

   //"turn on" the internal pullup resistors      
   digitalWrite( lbuttonPin, HIGH);       
   digitalWrite( mbuttonPin, HIGH);       
   digitalWrite( rbuttonPin, HIGH);       
   //  digitalWrite( VSSPin, HIGH);       

   //low level interrupt enable stuff      
   PCMSK1 |= (1 << PCINT8);
   enableLButton();
   enableMButton();
   enableRButton();
   PCICR |= (1 << PCIE1);       

   #if (OUTSIDE_TEMP_CFG == 1)
   INIT_OUTSIDE_TEMP();
   #endif

   delay2(1500);
   maxDelay = 0;
} /* void setup (void) */
 
void loop (void) {
   unsigned long lastActivity;
   unsigned long tankHold;      //state at point of last activity
   unsigned long loopStart;
   unsigned long temp;          //scratch variable

   lastActivity = microSeconds();

   if(newRun !=1) {
      //go through the initialization screen
      initGuino();
   }

   while (true) {

      loopStart=microSeconds();      

      #if (CFG_FUELCUT_INDICATOR != 0)
      FCUT_POS = 8;
      #endif
      
      #if (OUTSIDE_TEMP_CFG == 1)
      CALC_FILTERED_TEMP();
      #endif
      
      instant.reset();           //clear instant      
      cli();
      instant.update(tmpTrip);   //"copy" of tmpTrip in instant now      
      tmpTrip.reset();           //reset tmpTrip first so we don't lose too many interrupts      
      instInjStart=tmpInstInjStart; 
      instInjEnd=tmpInstInjEnd; 
      instInjTot=tmpInstInjTot;     
      instInjCount=tmpInstInjCount;
      
      tmpInstInjStart=nil; 
      tmpInstInjEnd=nil; 
      tmpInstInjTot=0;     
      tmpInstInjCount=0;

      sei();

      #if (CFG_SERIAL_TX == 1)
      /* send out instantmpg * 1000, instantmph * 1000, the injector/vss raw data */
      if (true) {
      simpletx(format(instantmpg()));
      simpletx(",");
      simpletx(format(instantmph()));
      simpletx(",");
      simpletx(format(instant.injHius   * 1000));
      simpletx(",");
      simpletx(format(instant.injPulses * 1000));
      simpletx(",");
      simpletx(format(instant.vssPulses * 1000)); 
      simpletx("\n");  
      }
      #endif

      /* --- update all of the trip objects */
      current.update(instant);       //use instant to update current      
      tank.update(instant);          //use instant to update tank
      #if (BARGRAPH_DISPLAY_CFG == 1)
      if (lastActivity != nil) {
         periodic.update(instant);   //use instant to update periodic 
      }
      #endif

      #if (BARGRAPH_DISPLAY_CFG == 1)
      /* --- For bargraph: reset periodic every n seconds, where n = loopcount / loopspersec, so 60 is 30 seconds */
      if (periodic.loopCount / loopsPerSecond >= 30) {
         temp = MIN((periodic.mpg()/10), 0xFFFF);
         /* add temp into first element and shift items in array */
         insert((int*)PERIODIC_HIST, (unsigned short)temp, mylength(PERIODIC_HIST), 0);
         periodic.reset();   /* reset */
      }
      #endif
      
      /* --- Decide whether to go to sleep or wake up */
      if (    (instant.vssPulses  == 0) 
            && (instantrpm() == 0) 
            && (HOLD_DISPLAY      == 0) 
          ) 
      {
         if(   (elapsedMicroseconds(lastActivity) > parms[currentTripResetTimeoutUSIdx])
            && (lastActivity != nil)
           )
         {
            #if (TANK_IN_EEPROM_CFG)
            //writeEepBlock32(eepBlkAddr_Tank, &tank.var[0], eepBlkSize_Tank);
			saveTankData();
            #endif
            #if (SLEEP_CFG & Sleep_bkl)
            analogWrite(BrightnessPin,brightness[0]);    //nitey night
            #endif
            #if (SLEEP_CFG & Sleep_lcd)
            LCD::LcdCommandWrite(LCD_DisplayOnOffCtrl);  //LCD off unless explicitly told ON
            system_sleep(); //system PowerDown mode to save power
            #endif
            lastActivity = nil;
         }
      }
      else {
         /* wake up! */
         if (lastActivity == nil) {
            #if (SLEEP_CFG & Sleep_bkl)
            analogWrite(BrightnessPin,brightness[brightnessIdx]);    
            #endif
            #if (SLEEP_CFG & Sleep_lcd)
            /* Turn on the LCD again.  Display should be restored. */
            LCD::LcdCommandWrite(LCD_DisplayOnOffCtrl | LCD_DisplayOnOffCtrl_DispOn);
            /* TODO:  Does the above cause a problem if sleep happens during a settings mode? 
             *        Said another way, we don't get the cursor back unless we ask for it. */
            #endif
            lastActivity=loopStart;
            tank.loopCount = tankHold;
            current.update(instant); 
            tank.update(instant); 
            #if (BARGRAPH_DISPLAY_CFG == 1)
            #if (false) 
            #else
            periodic.reset();
            #endif
            periodic.update(instant);
            #endif
         }
         else {
            lastActivity=loopStart;
            tankHold = tank.loopCount;
         }
      }

   if (HOLD_DISPLAY == 0) {

      #if (CFG_IDLE_MESSAGE == 0)
      displayFuncs[SCREEN]();    //call the appropriate display routine      
      #elif (CFG_IDLE_MESSAGE == 1)
      /* --- during idle, jump to EOC information */
      if (    (instant.injPulses >  0) 
           && (instant.vssPulses == 0) 
         ) 
      {
         /* the intention of the below logic is to avoid the display flipping 
            in stop and go traffic.  When you come to a stop, the delay timer 
            starts incrementing.  When you drive off, it decrements.  When the
            timer is zero, the display is always at the user-specified screen */
         if (IDLE_DISPLAY_DELAY < 6) {
            /* count up until delay time is reached */
            IDLE_DISPLAY_DELAY++;
         }
      }
      else {
         if (IdleDisplayRequested) {   //IdleDisplayRequested     (IDLE_DISPLAY_DELAY > 0)
            /* count from delay time back down to zero */
            IDLE_DISPLAY_DELAY--;
         }
         else if (IDLE_DISPLAY_DELAY < 0) {
            /* if the user selected a new screen while stopped, reset 
               the delay timer after driveoff */
            IDLE_DISPLAY_DELAY = 0;
         }
      }

      if (IdleDisplayRequested) {
         doDisplayEOCIdleData();
      }
      else {
         displayFuncs[SCREEN]();
      }
      #endif

      #if (CFG_FUELCUT_INDICATOR != 0)
      /* --- insert visual indication that fuel cut is happening */
      if (    (instantrpm() == 0)
           && (instant.vssPulses > 0) 
         ) 
      {
         #if (CFG_FUELCUT_INDICATOR == 1)
         LCDBUF1[FCUT_POS] = 'x';
         #elif ((CFG_FUELCUT_INDICATOR == 2) || (CFG_FUELCUT_INDICATOR == 3))
         LCDBUF1[FCUT_POS] = spinner[CLOCK & 0x03];
         #endif
      }
      #endif
      
      if (    (instant.injPulses  > 0) 
           && (instant.vssPulses == 0) 
         ) 
      {
      LCDBUF1[FCUT_POS] = spinner[CLOCK & 0x03];
      }

      /* --- ensure that we have terminating nulls */
      LCDBUF1[16] = 0;
      LCDBUF2[16] = 0;

      /* print line 1 */
      LCD::LcdCommandWrite(LCD_ReturnHome);
      LCD::print(LCDBUF1);

      /* print line 2 */
      LCD::gotoXY(0,1);
      LCD::print(LCDBUF2);
      
      //startOfButtonPresses
      
      if (LeftButtonPressed || RightButtonPressed || MiddleButtonPressed) {
         LCD::LcdCommandWrite(LCD_ClearDisplay);  // clear display, set cursor position to zero       
      }
      else {
         LCD::LcdCommandWrite(LCD_ReturnHome); 
      }
      
      /* --- see if any buttons were pressed, display a brief message if so --- */
      if (LeftButtonPressed && RightButtonPressed) {
         LCD::print(getStr(PSTR("Setup ")));
         initGuino();  
      }
      else if (LeftButtonPressed && MiddleButtonPressed) {
         tank.reset();      
         LCD::print(getStr(PSTR("Tank Reset ")));
      }
      else if (MiddleButtonPressed && RightButtonPressed) {
         current.reset();      
         LCD::print(getStr(PSTR("Current Reset ")));  
     
      }
      #if (CFG_IDLE_MESSAGE == 1)
      else if ((LeftButtonPressed || RightButtonPressed) && (IdleDisplayRequested)) {
         /* if the idle display is up and the user hits the left or right button,
          * intercept this press (nonoe of the elseifs will be hit below) 
          * only in this circumstance and get out of the idle display for a while.
          * This will return the user to his default screen. */
         IDLE_DISPLAY_DELAY = -60;
      }
      #endif
      else if (LeftButtonPressed) {
         // left is rotate through screens to the left      
         if (SCREEN!=0) {
             SCREEN--;
             /*if (SCREEN == dragSceenIdx) {
               delay2(screenDelay); 
             }*/
         }
         else {
            SCREEN=displayFuncSize-1;      
         }
         LCD::print(getStr(displayFuncNames[SCREEN])); 
      }
      else if (MiddleButtonPressed) {
         #if (DRAGRACE_DISPLAY_CFG)
         if(SCREEN == dragSceenIdx) {
            myDrag.reset();   
            LCD::print(getStr(PSTR("DragRace Reset")));
         } else
         #endif
         {
           
         brightnessIdx = (brightnessIdx + 1) % brightnessLength;
         if(brightnessIdx==0) { //just in case when display gets corrupted, you can cycle to brightness 0 and display will be initialized again
           LCD::init();           
           //LCD::LcdCommandWrite(LCD_SetDDRAM); // set dram to zero
         }
         analogWrite(BrightnessPin,brightness[brightnessIdx]);
         LCD::print(getStr(PSTR("Brightness ")));
         LCD::LcdDataWrite('0' + brightnessIdx);
         LCD::print(" ");  
         }      
      }
      else if (RightButtonPressed) {
         // right is rotate through screens to the right     
         SCREEN=(SCREEN+1)%displayFuncSize;      
         LCD::print(getStr(displayFuncNames[SCREEN])); 
         /*if (SCREEN == dragSceenIdx) {
               delay2(screenDelay); 
             }*/
      }      

      #if (CFG_IDLE_MESSAGE == 1)
      if (LeftButtonPressed || RightButtonPressed) {
         /* When the user wants to change screens, continue to 
          * avoid the idle screen for a while */
         IDLE_DISPLAY_DELAY = -60;
      }
      #endif

      if (buttonState!=buttonsUp) {
         HOLD_DISPLAY = 1;  
      }

   }  /* if (HOLD_DISPLAY == 0) */
   else {
      HOLD_DISPLAY = 0;
   } 

   // reset the buttons      
   buttonState=buttonsUp;
   
#if UNO_MODIFICATIONS == 1  //begin UNO modifications
  int softButton;
  softButton = analogRead (0);
  
  if (softButton < 60) { //reads zero 
      //Serial.println(("Soft Right"); 
   LCD::LcdCommandWrite(LCD_ClearDisplay);    
         SCREEN=(SCREEN+1)%displayFuncSize;      
         LCD::print(getStr(displayFuncNames[SCREEN])); 

         delay2(screenDelay);  

  }
  else if (softButton < 200){  //reads 99

      tank.reset();
      current.reset();
  }
  else if (softButton < 400){ //reads 255

   LCD::LcdCommandWrite(LCD_ClearDisplay);
         #if (DRAGRACE_DISPLAY_CFG)
         if(SCREEN == dragSceenIdx) {
            myDrag.reset();   
            LCD::print(getStr(PSTR("DragRace Reset"))); 
            //delay2(125);  
         } else
         #endif
         {
         brightnessIdx = (brightnessIdx + 1) % brightnessLength;
         if(brightnessIdx==0) { //just in case when display gets corrupted, you can cycle to brightness 0 and display will be initialized again
           LCD::init();           
         }
         analogWrite(BrightnessPin,brightness[brightnessIdx]);
         LCD::print(getStr(PSTR("Brightness ")));
         LCD::LcdDataWrite('0' + brightnessIdx);
         LCD::print(" ");
         delay2(screenDelay);
         }    
  }
  else if (softButton < 600){ //reads 409
   LCD::LcdCommandWrite(LCD_ClearDisplay);
      if (SCREEN!=0) {
             SCREEN--;       
         }
         else {
            SCREEN=displayFuncSize-1;      
         }
         LCD::print(getStr(displayFuncNames[SCREEN]));
         delay2(screenDelay);  
  }
  else if (softButton < 800){  //
   LCD::LcdCommandWrite(LCD_ClearDisplay);
         LCD::print(getStr(PSTR("Setup ")));  
         delay2(screenDelay); 
         initGuino();  
  } else {  //reads 1023
  }
#endif //end if UNO_MODIFICATIONS

       if (setupRan == 0) {  
       } else { //reset loopstart so MxLL isn't too long based on setup delay
         loopStart=microSeconds();  
         setupRan = 0;
       }  
       
   // keep track of how long the loops take before we go int waiting. 
   LASTLOOPLENGTH = elapsedMicroseconds(loopStart);   
   MAXLOOPLENGTH = MAX(MAXLOOPLENGTH, elapsedMicroseconds(loopStart));
   
   if (myDrag.running || myDrag.waiting_start) {
   //  if (SCREEN == dragSceenIdx) {
   }
   
   //short iRpm = instantrpm(); 
   else if (instantrpm() <= 2500 * 1000) {
      while (elapsedMicroseconds(loopStart) < (looptime * 1)) {
      // wait for the end of the loop to arrive (default was .5, calcualte from header values)
      // if this number is less than millis2() - loopStart, loops will not be delayed
      continue;
   }
   
  }

   CLOCK++;

   } /* while (true) */
} /* loop (void) */
 
//--------------------------------------------------------
char *format(unsigned long num) {
   static char fBuff[7];  //used by format
   unsigned char dp = 3;
   unsigned char x = 6;

   while (num > 999999) {
      num /= 10;
      dp++;
      if( dp == 5 ) break; /* We'll lose the top numbers like an odometer */
   }
   if (dp == 5) {
      dp = 99;
   }                       /* We don't need a decimal point here. */

   /* Round off the non-printed value. */
   if((num % 10) > 4) {
      num += 10;
   }

   num /= 10;


   while (x > 0) {
      x--;
      if (x == dp) {
         /* time to poke in the decimal point? */
         fBuff[x]='.';
      }
      else {
         /* poke the ascii character for the digit. */
         fBuff[x]= '0' + (num % 10);
         num /= 10;
      }
   }

   if( fBuff[0]=='0' ) { //if leftmost char is 0 then replace it with space: '099.56' => ' 99.56'
      fBuff[0]=' ';
      if( fBuff[1]=='0' ) fBuff[1]=' ';
   }

   fBuff[6] = 0;
   return fBuff;
} 

//--------------------------------------------------------
char* intformat(unsigned long num, byte numlength=6)
{
  static char fBuff[7];  //used by format

  //if number doesn't fit, show 9999 (92 bytes)
  if(numlength==6 && num > 999999999){
    num = 999999000ul;
  }
  else if(numlength==5 && num > 99999999){
    num = 99999000ul;
  }
  else if(numlength==4 && num > 9999999){
    num = 9999000ul;
  }
 
  
  num /= 100;
  // Round off the non-printed value.
  if((num % 10) > 4) num += 10;
  num /= 10;
  
  byte x = numlength;
  fBuff[x] = 0;//end string with \0
  while(x > 0){
    x--;
    fBuff[x]= '0' + (num % 10);//poke the ascii character for the digit.
    num /= 10; 
  }
  
  //replace leading 0 with ' '
  boolean leadnulls = true;
  while(x < numlength-1 && leadnulls){
    fBuff[x]=='0' ? fBuff[x]=' ' : leadnulls = false;
    x++;
  }
  
  return fBuff;  
}
 
//get a string from flash 
char *getStr(prog_char * str) { 
   static char mBuff[17]; //used by getStr 
   strcpy_P(mBuff, str); 
   return mBuff; 
} 

#if (OUTSIDE_TEMP_CFG == 1) 
void doDisplayCustom() {
  
     displayTripCombo("Im",0,instantmpg(), "t",0,OUTSIDE_TEMP_FILT,
                      "GH",0,instantgph(), "m",0,current.mpg());
}      
#else
void doDisplayCustom() {
  unsigned long impg = instantmpg();
  char * mBuff = "Im"; //2 char buffer

  // --- during idle, show GPH
  if(instant.injPulses > 0 && instant.vssPulses == 0) {
    impg=instantgph();
    #if (CFG_UNITS == 2)
      mBuff = "Lh";
    #else
      mBuff = "Gh";
    #endif
  }

  displayTripCombo  (mBuff,0,impg,  "T",0,current.time(),
                   "Cm",0,current.mpg(), "$",0,current.fuelCost());
}      
#endif

void doDisplayCarSensors() {
  int mod = 100000;
  if (instantrpm() < 500) {
    mod = 0; 
  }
#if (BATTERY_VOLTAGE == 1)
   #if (CFG_UNITS == 2)
   displayTripCombo("v ",0,batteryVoltage(), "spd",1,instantmph(),
                    "Lh",0,instantgph(),     "rpm",1,instantrpm());
   #else
   displayTripCombo("v ",0,batteryVoltage(), "spd",1,instantmph(),
                    "gh",0,instantgph(),     "rpm",1,instantrpm()+mod);
   #endif
#else
   #if (CFG_UNITS == 2)
   displayTripCombo("CT",0,current.time(), "spd",1,instantmph(),
                    "Lh",0,instantgph(),   "rpm",1,instantrpm());
   #else
   displayTripCombo("CT",0,current.time(), "spd",1,instantmph(),
                    "gh",0,instantgph(),   "rpm",1,instantrpm());
   #endif
#endif
}

#if(DRAGRACE_DISPLAY_CFG)
void doDisplayDragRace() {
String dSpeed = String(parms[dragSpeed]);
char * mBuff = "Imm"; //3 char buffer
itoa(parms[dragSpeed], mBuff, 10);

#if (CFG_UNITS == 2) 
   displayTripCombo("ET",0,myDrag.time(),       "Spd",1,myDrag.trapspeed(),
                    "100",0,myDrag.time100kmh(), "m",1,myDrag.distance() );    
#else
   displayTripCombo("ET",0,myDrag.time(),       "Spd",1,myDrag.trapspeed(),
                    mBuff,0,myDrag.time100kmh(), "Ft",1,myDrag.distance() );                  //       "Dis",1,myDrag.power());  //"Dst",1,myDrag.distance() );
#endif
}
#endif

void doDisplayEOCIdleData() {
   #if (CFG_UNITS == 2)
   displayTripCombo("eC",0,current.eocMiles(), "iL",0,current.idleGallons(),
                    "eT",0,tank.eocMiles(),    "iL",0,tank.idleGallons());
   #else
   displayTripCombo("iC",0,current.idleGallons(), "e",0,current.eocMiles(),
                    "iT",0,tank.idleGallons(),    "e",0,tank.eocMiles());
   #endif
}      

void doDisplayInstantCurrent() {
  unsigned long impg = instantmpg();
  char * mBuff = "Im"; //2 char buffer

  // --- during idle, show GPH
  //if(instant.injPulses > 0 && instant.vssPulses == 0) {
  if(instantrpm() > 0 && instant.vssPulses == 0) {
    impg=instantgph();
    #if (CFG_UNITS == 2)
      mBuff = "Lh";
    #else
      mBuff = "Gh";
    #endif
  }
   //displayTripCombo(mBuff,0,impg,  "Spd",1,instantmph(),
   //                 "Cm",0,current.mpg(), "D",0,current.miles());
   displayTripCombo(mBuff,0,impg,  "RNG",1,getRNG(),
                    "Cm",0,current.mpg(), "Fu",0,tank.gallons());
}      

void doDisplayInstantTank() {
  unsigned long impg = instantmpg();
  char * mBuff = "Im"; //2 char buffer

  // --- during idle, show GPH
  if(instant.injPulses > 0 && instant.vssPulses == 0) {
    impg=instantgph();
    #if (CFG_UNITS == 2)
      mBuff = "Lh";
    #else
      mBuff = "Gh";
    #endif
  }
                    
   displayTripCombo(mBuff,0,impg,  "DTE",1,getDTE(),
                    "Tm",0,tank.mpg(), "Fr",0, (parms[tankSizeIdx] - tank.gallons()) * 1//(tank.mpg() * 1000)    
                  );
        }      

#if CFG_BIGFONT_TYPE > 0
void doDisplayBigInstant() {
//   #if (CFG_UNITS == 2)
//   bigNum(instantmpg(),"Inst","L/Km");
//   #else
//   bigNum(instantmpg(),"Inst","MPG");
//   #endif 
  unsigned long impg = instantmpg();
  #if (CFG_UNITS == 2)
  char * mBuff = "L/Km"; //4 char buffer;
  #else
  char * mBuff = "MPG "; //4 char buffer;
  #endif

  // --- during idle, show GPH
  if(instant.injPulses > 0 && instant.vssPulses == 0) {
    impg=instantgph();
    #if (CFG_UNITS == 2)
      mBuff = "L/h ";
    #else
      mBuff = "G/h ";
    #endif
  }
  
  bigNum(impg,"Inst",mBuff);
}      

void doDisplayBigCurrent() {
   #if (CFG_UNITS == 2)
   bigNum(current.mpg(),"Curr","L/Km");
   #else
   bigNum(current.mpg(),"Curr","MPG");
   #endif
}

void doDisplayBigSpeed() {
   #if (CFG_UNITS == 2)
   bigNum(instantmph(),"Inst","Km/h");
   #else
   bigNum(instantmph(),"Inst","MPH");
   #endif
}  

void doDisplayBigTank()    {
   #if (CFG_UNITS == 2)
   bigNum(tank.mpg(),"Tank","L/Km");
   #else
   bigNum(tank.mpg(),"Tank","MPG");
   #endif
}


void doDisplayCurrentTripData(void) {
   /* display current trip formatted data */
   tDisplay(&current);
   //How about: x$?         Mi
   //           xMpg?       Fu/Gal
}   
#endif

void doDisplayTankTripData(void) {
   /* display tank trip formatted data */
   tDisplay(&tank);
}      

void doDisplaySystemInfo(void) {  
   /* display max cpu utilization and ram */
   strcpy(&LCDBUF1[0], "MxL ");
   strcpy(&LCDBUF1[4], intformat(   MAXLOOPLENGTH-(maxDelay*1000)      ,4));
   strcpy(&LCDBUF1[8], " Ls");
   //strcpy(&LCDBUF1[11], intformat    ((/*LASTLOOPLENGTH*/1000000/(LASTLOOPLENGTH/1000)),5)   );  
   strcpy(&LCDBUF1[11], intformat    (( (LASTLOOPLENGTH)),5)   );  
   /*Serial.println(LASTLOOPLENGTH); 
   Serial.println(maxDelay * 1000); */

   unsigned long mem = memoryTest();      
   mem*=1000;      
   strcpy(&LCDBUF2[0], "Free mem: ");
   strcpy(&LCDBUF2[10],  intformat(mem,6));
}    

#if (BARGRAPH_DISPLAY_CFG == 1)
   void doDisplayBarGraph(void) {
   unsigned short impg = instantmpg(); 
   signed short temp = 0;
   signed short mpg_temp = 0; 
   unsigned char i = 0;
   unsigned char j = 0;
   unsigned short stemp = 0;

   /* Load the bargraph characters if necessary */
   if (DISPLAY_TYPE != dtBarGraph) {
      LCD::writeCGRAM(&barchars[0], LcdBarChars);
      DISPLAY_TYPE = dtBarGraph;
   }

   /* plot bars */
   for(i=8; i>0; i--) {

      temp = MAX((signed short)(PERIODIC_HIST[i-1] - BAR_MIN), 0); //at least x or bar low 
      stemp = MIN(temp, BAR_LIMIT); //not more than x or bar_high
      
      
      temp = (signed short)    ( (stemp*16) / ((BAR_LIMIT-BAR_MIN)/10) ); //convert to 0-160, or 1000 * 16 (16000 / (2000) 
      temp = (temp+5)/10; //round, /10 (so 160 = 16, etc).   
      temp = MIN(temp, 16);  //not more than ...
      temp = MAX(temp,  0);  //at least ... 
      /* line 1 graph */
      LCDBUF1[j] = ascii_barmap[MAX(temp-8,0)];
      /* line 2 graph */
      LCDBUF2[j] = ascii_barmap[MIN(temp,8)];
      j++;
   }
   
   /* end of line 1: show current mpg */
   LCDBUF1[8] = ' ';
    if(instantrpm() > 0 && instant.vssPulses == 0) {
      LCDBUF1[9] = 'g';
      strcpy(&LCDBUF1[10], format(instantgph()));
    }
    else {
      LCDBUF1[9] = 'i';
      strcpy(&LCDBUF1[10], format(impg));
    }
    

        mpg_temp = impg;  
        mpg_temp = mpg_temp/10; 
        mpg_temp = mpg_temp - BAR_MIN;//  / (BAR_LIMIT-BAR_MIN)/1;
        mpg_temp = (( mpg_temp * 10 ) / ((BAR_LIMIT - BAR_MIN)/10 ) ); 
        mpg_temp = (mpg_temp * 16) / 10; 
        mpg_temp = (mpg_temp+5)/10; //round, /10 (so 160 = 16, etc). 
        mpg_temp = MIN(mpg_temp, 16);  //not more than ...
        mpg_temp = MAX(mpg_temp,  0);  //at least ... 
        LCDBUF1[8] = ascii_barmap[MAX(mpg_temp-8,0)];
        LCDBUF2[8] = ascii_barmap[MIN(mpg_temp,8)];

   /* end of line 2: show periodic mpg */
   //LCDBUF2[8] = ' ';
   LCDBUF2[9] = 'p';
   strcpy(&LCDBUF2[10], format(periodic.mpg()));

   #if (CFG_FUELCUT_INDICATOR != 0)
   /* where should the fuel cut indication go? */
   FCUT_POS = 8;
   #endif
}
#endif

unsigned long getDTE(void){
   unsigned long dte;
   signed long fuel_remaining;
   fuel_remaining = parms[tankSizeIdx] - tank.gallons();
   fuel_remaining = MAX(fuel_remaining, 0);

   #if (CFG_UNITS == 2)
   //dte = (fuel_remaining * 100000) / tank.mpg(); //dont work, multiplied number gets too big
   dte = ((fuel_remaining * 1000) / tank.mpg()) * 100;
   #else
   dte = (fuel_remaining * tank.mpg()) / 1000;
   #endif
   
   return dte;
} 


unsigned long getRNG(void){
  //return 5000; 
   unsigned long rng;
   signed long fuel_capacity;
   fuel_capacity = parms[tankSizeIdx];
   fuel_capacity = MAX(parms[tankSizeIdx], 0);
   
   #if (CFG_UNITS == 2)
   //dte = (fuel_remaining * 100000) / tank.mpg(); //dont work, multiplied number gets too big
   rng = (parms[tankSizeIdx]/1000) * tank.mpg(); //* 100;
   #else
   rng = (parms[tankSizeIdx]/1000) * tank.mpg();// / 1000;
   #endif
   
   return rng;
} 


#if (BATTERY_VOLTAGE == 1)
unsigned long batteryVoltage(void){
   float vout = 0.0;
   float vin = 0.0;
   float R1 = 27000.0;    // !! resistance of R1 !! 21500
   float R2 = 9600.0;     // !! resistance of R2 !! 9500
   int value = 0;

   value = analogRead(temp2Pin);
   vout = (value * 5.0) / 1024.0;
   vin = vout / (R2/(R1+R2));

   // actual voltage is 0.7V higher - we are measuring after 1N4004 diode voltage drop
   //return vin * 1000 - 300 + parms[voltmeterOffsetIdx];

   vin = vin * 1000 - 300 + parms[voltmeterOffsetIdx];   
   return ((unsigned long)vin / 100)*100; //make last digit to be 0
} 
#endif

void displayTripCombo(char *L1, char isInt1,unsigned long V1,   char *L2, char isInt2, unsigned long V2, 
                      char *L3, char isInt3, unsigned long V3,  char *L4, char isInt4, unsigned long V4) {
   byte len = 0;
   /* Process line 1 of the display */
   strcpy(&LCDBUF1[0], L1);
   //strcpy(&LCDBUF1[2], format(V1));
   len = strlen(L1);
   strcpy(&LCDBUF1[len], (isInt1 ? intformat(V1,8-len) : format(V1)) );
   LCDBUF1[8] = ' ';
   strcpy(&LCDBUF1[9], L2);
   //strcpy(&LCDBUF1[10], format(V2));
   len = strlen(L2);
   strcpy(&LCDBUF1[9+len], (isInt2 ? intformat(V2,7-len) : format(V2)) );

   /* Process line 2 of the display */
   strcpy(&LCDBUF2[0], L3);
   //strcpy(&LCDBUF2[2], format(V3));
   len = strlen(L3);
   strcpy(&LCDBUF2[len], (isInt3 ? intformat(V3,8-len) : format(V3)) );
   LCDBUF2[8] = ' ';
   strcpy(&LCDBUF2[9], L4);
   //strcpy(&LCDBUF2[10], format(V4));
   len = strlen(L4);
   strcpy(&LCDBUF2[9+len], (isInt4 ? intformat(V4,7-len) : format(V4)) );

   #if (CFG_FUELCUT_INDICATOR != 0)
   FCUT_POS = 8;
   #endif
}      
 
//arduino doesn't do well with types defined in a script as parameters, so have to pass as void * and use -> notation.      
void tDisplay( void * r){ //display trip functions.        
   Trip *t = (Trip *)r;
   
   #if (CFG_UNITS == 2)

      
   strcpy(&LCDBUF1[0], "km");
   strcpy(&LCDBUF1[2], format(t->miles()));
   strcpy(&LCDBUF2[0], "Li");
   strcpy(&LCDBUF2[2], format(t->gallons()));
   
   strcpy(&LCDBUF1[8], " |");  
   strcpy(&LCDBUF1[10], format(t->fuelCost()) );  //current.fuelCost()
   //strcpy(&LCDBUF1[10], intformat(t->gallons()*  (current.fuelCost() / 1000)  );
   strcpy(&LCDBUF2[8], " p");
   strcpy(&LCDBUF2[10], format(t->mpg()));

   #else
   strcpy(&LCDBUF1[0], "mi");
   strcpy(&LCDBUF1[2], format(t->miles()));
   strcpy(&LCDBUF2[0], "Ga");
   strcpy(&LCDBUF2[2], format(t->gallons()));
   
   strcpy(&LCDBUF1[8], " $");  //fuel cust
   //strcpy(&LCDBUF1[10], intformat(t->mph()));
   strcpy(&LCDBUF1[10], format(t->fuelCost()) );
   strcpy(&LCDBUF2[8], " p");
   strcpy(&LCDBUF2[10], format(t->mpg()));
   #endif

}      
    
 
// this function will return the number of bytes currently free in RAM      
extern int  __bss_end; 
extern int  *__brkval; 
int memoryTest(){ 
  int free_memory; 
  if((int)__brkval == 0) 
    free_memory = ((int)&free_memory) - ((int)&__bss_end); 
  else 
    free_memory = ((int)&free_memory) - ((int)__brkval); 
  return free_memory; 
} 
 
//Trip::Trip(){      
//}      
 

unsigned long instantmph(){  
  //unsigned long vssPulseTimeuS = (lastVSS1 + lastVSS2) / 2;
  unsigned long vssPulseTimeuS = instant.vssPulseLength/instant.vssPulses;

  init64(tmp1,0,1000000000ul);
  init64(tmp2,0,parms[vssPulsesPerMileIdx]);
  div64(tmp1,tmp2);
  init64(tmp2,0,3600);
  mul64(tmp1,tmp2);
  init64(tmp2,0,vssPulseTimeuS);
  div64(tmp1,tmp2);
   
  return tmp1[1];
}


unsigned long instantmpg(){     
  
  unsigned long imph=instantmph();
  unsigned long igph=instantgph();
  #if UNO_MODIFICATIONS
  return random(0, 50000);
  #endif

#if(CFG_UNITS==2) // km
  if(imph == 0) return 999999000;
  if(igph == 0) return 0;
#else // miles
  if(imph == 0) return 0;
  if(igph == 0) return 999999000;  
#endif

  init64(tmp1,0,1000ul);
  init64(tmp2,0,imph);
  mul64(tmp1,tmp2);
  init64(tmp2,0,igph);
  div64(tmp1,tmp2);
  
#if(CFG_UNITS==2) // km
  //convert km/L to L/100km
  init64(tmp2,0,100000000ul);
  div64(tmp2,tmp1);
  return tmp2[1];
#else 
  return tmp1[1];
#endif 
}

unsigned long instantgph(){      
//  unsigned long vssPulseTimeuS = instant.vssPulseLength/instant.vssPulses;
  
//  unsigned long instInjStart=nil; 
//unsigned long instInjEnd; 
//unsigned long instInjTot; 
  init64(tmp1,0,instInjTot);
  init64(tmp2,0,3600000000ul);
  mul64(tmp1,tmp2);
  init64(tmp2,0,1000ul);
  mul64(tmp1,tmp2);
  init64(tmp2,0,parms[microSecondsPerGallonIdx]);
  div64(tmp1,tmp2);
  init64(tmp2,0,instInjEnd-instInjStart);
  div64(tmp1,tmp2);
  return tmp1[1];      
}

unsigned long instantrpm(){      
  init64(tmp1,0,instInjCount);
  init64(tmp2,0,120000000ul);
  mul64(tmp1,tmp2);
  init64(tmp2,0,1000ul);
  mul64(tmp1,tmp2);
  init64(tmp2,0,parms[injPulsesPer2Revolutions]);
  div64(tmp1,tmp2);
  init64(tmp2,0,instInjEnd-instInjStart);
  div64(tmp1,tmp2);
  
  return (tmp1[1] / 10000)*10000; //make last digit to be 0
  //return tmp1[1];
} 

unsigned long Trip::miles(){ 
  init64(tmp1,0,vssPulses);
  init64(tmp2,0,1000);
  mul64(tmp1,tmp2);
  init64(tmp2,0,parms[vssPulsesPerMileIdx]);
  div64(tmp1,tmp2);
  return tmp1[1];      
}      
 
unsigned long Trip::eocMiles(){      
  init64(tmp1,0,vssEOCPulses);
  init64(tmp2,0,1000);
  mul64(tmp1,tmp2);
  init64(tmp2,0,parms[vssPulsesPerMileIdx]);
  div64(tmp1,tmp2);
  return tmp1[1];      
}       
 
unsigned long Trip::mph(){      
  if(loopCount == 0)     
     return 0;     
  init64(tmp1,0,loopsPerSecond);
  init64(tmp2,0,vssPulses);
  mul64(tmp1,tmp2);
  init64(tmp2,0,3600000);
  mul64(tmp1,tmp2);
  init64(tmp2,0,parms[vssPulsesPerMileIdx]);
  div64(tmp1,tmp2);
  init64(tmp2,0,loopCount);
  div64(tmp1,tmp2);
  return tmp1[1];      
}      
 
unsigned long  Trip::gallons(){      
  init64(tmp1,0,injHiSec);
  init64(tmp2,0,1000000);
  mul64(tmp1,tmp2);
  init64(tmp2,0,injHius);
  add64(tmp1,tmp2);
  init64(tmp2,0,1000);
  mul64(tmp1,tmp2);
  init64(tmp2,0,parms[microSecondsPerGallonIdx]);
  div64(tmp1,tmp2);
  #if useDebug
  return 1000;
  #endif
  return tmp1[1];      
}      

unsigned long Trip::idleGallons(){      
  init64(tmp1,0,injIdleHiSec);
  init64(tmp2,0,1000000);
  mul64(tmp1,tmp2);
  init64(tmp2,0,injIdleHius);
  add64(tmp1,tmp2);
  init64(tmp2,0,1000);
  mul64(tmp1,tmp2);
  init64(tmp2,0,parms[microSecondsPerGallonIdx]);
  div64(tmp1,tmp2);
  return tmp1[1];      
}      

//eocMiles
//idleGallons
 
unsigned long  Trip::mpg(){
#if (UNO_MODIFICATIONS == 1)
#if (useDebug)
    if (CFG_UNITS==2) {
      return 11700; //if L/100km, or return 8500 if km/L
    }
    else { 
  return 20000;
    } 
#endif
#endif
  #if(CFG_UNITS==2) // km
    if(vssPulses==0) return 999999000;      
    if(injPulses==0) return 0;
  #else // miles
    if(vssPulses==0) return 0;      
    if(injPulses==0) return 999999000; //who doesn't like to see 999999?  :)    
  #endif
 
  init64(tmp1,0,injHiSec);
  init64(tmp3,0,1000000);
  mul64(tmp3,tmp1);
  init64(tmp1,0,injHius);
  add64(tmp3,tmp1);
  init64(tmp1,0,parms[vssPulsesPerMileIdx]);
  mul64(tmp3,tmp1);
 
  init64(tmp1,0,parms[microSecondsPerGallonIdx]);
  init64(tmp2,0,1000);
  mul64(tmp1,tmp2);
  init64(tmp2,0,vssPulses);
  mul64(tmp1,tmp2);
 
  div64(tmp1,tmp3);
  
#if(CFG_UNITS==2) // km
  //convert km/L to L/100km
  init64(tmp2,0,100000000ul);
  div64(tmp2,tmp1);
  return tmp2[1];
#else
  return tmp1[1];
#endif 
}      
 
//return the hhh:mm      
unsigned long Trip::time(){          
   unsigned long seconds = (loopCount/loopsPerSecond)/60; //minutes actually         
   return ((seconds/60)*1000) + ((seconds%60) * 10);       
}

unsigned long Trip::fuelCost(){
/*
   unsigned long tripGallons = Trip::gallons();
   init64(tmp1,0,parms[fuelCostIdx]);
   init64(tmp2,0,tripGallons);
   mul64(tmp1,tmp2);
   init64(tmp2,0,1000);
   div64(tmp1,tmp2);
   return tmp1[1];
*/
   return (parms[fuelCostIdx] * Trip::gallons() / 1000);
}
 
void Trip::reset(){      
   loopCount = 0;      
   injPulses = 0;      
   injHius = 0;      
   injHiSec = 0;      
   vssPulses = 0;  
   vssPulseLength = 0;
   injIdleHiSec = 0;
   injIdleHius = 0;
   vssEOCPulses = 0;
}      
 
void Trip::update(Trip t) {
   if(t.vssPulses || t.injPulses) { 
      loopCount++;  //we call update once per loop
   }
   vssPulses += t.vssPulses;      
   vssPulseLength += t.vssPulseLength;
   if ( t.injPulses == 0 )  //track distance traveled with engine off
      vssEOCPulses += t.vssPulses;

   if ( t.injPulses > 2 && t.injHius < 500000 ) {// chasing ghosts      
      injPulses += t.injPulses;      
      injHius += t.injHius;      
      if (injHius >= 1000000){  
         // rollover into the injHiSec counter      
         injHiSec++;      
         injHius -= 1000000;      
      }
      if(t.vssPulses == 0) {    
         // track gallons spent sitting still
         injIdleHius += t.injHius;      
         if (injIdleHius >= 1000000) {  //r
            injIdleHiSec++;
            injIdleHius -= 1000000;      
         }      
      }
   }      
}   
 

#if (CFG_BIGFONT_TYPE > 0)
void bigNum (unsigned long t, char * txt1, char * txt2){      
  char dp = ' ';       // decimal point is a space
  char *r = "009.99";  // default to 999
  if (DISPLAY_TYPE != dtBigChars) {
     LCD::writeCGRAM(&chars[0], LcdNewChars);
     DISPLAY_TYPE = dtBigChars;
  }
  if(t<=99500){ 
     r=format(t/10);   // 009.86 
     dp=5;             // special character 5
  }
  else if(t<=999500){ 
     r=format(t/100);  // 009.86 
  }   
 
  strcpy(&LCDBUF1[0], (bignumchars1+(r[2]-'0')*4));
  LCDBUF1[3] = ' ';
  strcpy(&LCDBUF1[4], (bignumchars1+(r[4]-'0')*4));
  LCDBUF1[7] = ' ';
  strcpy(&LCDBUF1[8], (bignumchars1+(r[5]-'0')*4));
  LCDBUF1[11] = ' ';
  strcpy(&LCDBUF1[12], txt1);
 
  strcpy(&LCDBUF2[0], (bignumchars2+(r[2]-'0')*4));
  LCDBUF2[3] = ' ';
  strcpy(&LCDBUF2[4], (bignumchars2+(r[4]-'0')*4));
  LCDBUF2[7] = dp;
  strcpy(&LCDBUF2[8], (bignumchars2+(r[5]-'0')*4));
  LCDBUF2[11] = ' ';
  strcpy(&LCDBUF2[12], txt2);

  #if (CFG_FUELCUT_INDICATOR != 0)
  FCUT_POS = 11;
  #endif
  
}
#endif

int insert(int *array, int val, size_t size, size_t at)
{
  size_t i;

  /* In range? */
  if (at >= size)
    return -1;

  /* Shift elements to make a hole */
  for (i = size - 1; i > at; i--)
    array[i] = array[i - 1];
  /* Insertion! */
  array[at] = val;

  return 0;
}
  
void save(){
  EEPROM.write(0,guinosig);
  EEPROM.write(1,mylength(parms));
  writeEepBlock32(0x04, &parms[0], mylength(parms));
}

void writeEepBlock32(unsigned int start_addr, unsigned long *val, unsigned int size) {
  unsigned char p = 0;
  unsigned char shift = 0;
  int i = 0;
  for(start_addr; p < size; start_addr+=4) {
    for (i=0; i<4; i++) {
      shift = (8 * (3-i));  /* 24, 16, 8, 0 */
      EEPROM.write(start_addr + i, (val[p]>>shift) & 0xFF);
    }
    p++;
  }
}

void readEepBlock32(unsigned int start_addr, unsigned long *val, unsigned int size) {
   unsigned long v = 0;
   unsigned char p = 0;
   unsigned char temp = 0;
   unsigned char i = 0;
   for(start_addr; p < size; start_addr+=4) {
      v = 0;   /* clear the scratch variable every loop! */
      for (i=0; i<4; i++) {
         temp = (i > 0) ? 1 : 0;  /* 0, 1, 1, 1  */
         v = (v << (temp * 8)) + EEPROM.read(start_addr + i);
      }
      val[p] = v;
      p++;
   }
}

#if (TANK_IN_EEPROM_CFG == 1)
void saveTankData() {
  unsigned long var[] = {
    tank.loopCount,
    tank.injPulses,
    tank.injHiSec,
    tank.injHius,
    tank.injIdleHiSec,
    tank.injIdleHius,
    tank.vssPulses,
    tank.vssEOCPulses,
    tank.vssPulseLength
  };
  writeEepBlock32(eepBlkAddr_Tank, &var[0], eepBlkSize_Tank);
}

void loadTankData() {
  unsigned long var[9];
  readEepBlock32(eepBlkAddr_Tank, &var[0], eepBlkSize_Tank);
  tank.loopCount = var[0];
  tank.injPulses = var[1];
  tank.injHiSec = var[2];
  tank.injHius = var[3];
  tank.injIdleHiSec = var[4];
  tank.injIdleHius = var[5];
  tank.vssPulses = var[6];
  tank.vssEOCPulses = var[7];
  tank.vssPulseLength = var[8];
}
#endif

unsigned char load(){ //return 1 if loaded ok

  #ifdef usedefaults
    return 1;
  #endif
  
  unsigned char b = EEPROM.read(0);
  unsigned char c = EEPROM.read(1);
  if(b == guinosigold)
    c=9; //before fancy parameter counter

  if(b == guinosig || b == guinosigold){
    readEepBlock32(0x04, &parms[0], mylength(parms));
    #if (TANK_IN_EEPROM_CFG == 1)
    /* read out the tank variables on boot */
    /* TODO:  eepBlkSize_Tank is appropriate for size? */
    //readEepBlock32(eepBlkAddr_Tank, &tank.var[0], eepBlkSize_Tank);
    loadTankData();
    #endif
    return 1;
  }
  return 0;
}

char * uformat(unsigned long val){ 
  static char mBuff[17];
  unsigned long d = 1000000000ul;
  unsigned char p;
  for(p = 0; p < 10 ; p++){
    mBuff[p]='0' + (val/d);
    val=val-(val/d*d);
    d/=10;
  }
  mBuff[10]=0;
  return mBuff;
} 

unsigned long rformat(char * val){ 
  unsigned long d = 1000000000ul;
  unsigned long v = 0ul;
  for(unsigned char p = 0; p < 10 ; p++){
    v=v+(d*(val[p]-'0'));
    d/=10;
  }
  return v;
} 


boolean editParm(unsigned char parmIdx){
  #if (useDebug)
  //Serial.println(("Edit Parms called"); 
  //Serial.println(("setupRan");
  //Serial.println((setupRan); 
  #endif
   setupRan = 1; 
  #if (useDebug) 
  //Serial.println(("setupRan");
  //Serial.println((setupRan);
  #endif
   unsigned long v = parms[parmIdx];
   unsigned char p=9;  //right end of 10 digit number
   unsigned char keyLock=1;    
   char *fmtv = uformat(v);

   /* -- line 1 -- */
   strcpy(&LCDBUF1[0], parmLabels[parmIdx]);

   /* -- line 2 -- */
   strcpy(&LCDBUF2[0], fmtv);
   strcpy(&LCDBUF2[10], " OK Ca");

   /* -- write to display -- */
   LCDBUF1[16] = 0; 
   LCDBUF2[16] = 0;
   LCD::LcdCommandWrite(LCD_ClearDisplay);
   LCD::print(LCDBUF1);
   LCD::gotoXY(0,1);    
   LCD::print(LCDBUF2);

   /* -- turn the cursor on -- */
   LCD::LcdCommandWrite(LCD_DisplayOnOffCtrl | LCD_DisplayOnOffCtrl_DispOn | LCD_DisplayOnOffCtrl_CursOn);

   /* cursor on 'OK' by default except for contrast */
   (parmIdx == contrastIdx) ? p=10 : p=10;  

  while(true){

    if(p<10)
      LCD::gotoXY(p,1);   
    if(p==10)     
      LCD::gotoXY(11,1);   
    if(p==11)     
      LCD::gotoXY(14,1);   

     if(keyLock == 0) { 
        if (LeftButtonPressed && RightButtonPressed) {
            if (p<10)
               p=10;
            else if (p==10) 
               p=11;
            else {
               /* cursor on 'OK' by default except for contrast */
               (parmIdx == contrastIdx) ? p=8 : p=10;  
            }
        }
        else if (LeftButtonPressed) {
            p--;
            if(p==255)p=11;
        }
        else if(RightButtonPressed) {
             p++;
            if(p==12)p=0;
        }
        else if(MiddleButtonPressed) {

             if(p==10){  //ok selected
                parms[parmIdx]=rformat(fmtv);
                LCD::LcdCommandWrite(B00001100); //cursor off
                delay2(250);
                return true;
             }
             else if(p==11){  //cancel selected
                LCD::LcdCommandWrite(B00001100); //cursor off
                delay2(250);
                return false;
             }   			 

             unsigned char n = fmtv[p]-'0';
             n++;
             if (n > 9) n=0;
             if(p==0 && n > 3) n=0;
             fmtv[p]='0'+ n;
             LCD::gotoXY(0,1);        
             LCD::print(fmtv);
             LCD::gotoXY(p,1);        
             if(parmIdx==contrastIdx)//adjust contrast dynamically
                 analogWrite(ContrastPin,rformat(fmtv));  
        }

        if(buttonState!=buttonsUp) keyLock=1;
     }
     else{
        keyLock=0;
     }
     buttonState=buttonsUp;
     
  //begin UNO modifications   
  if (UNO_MODIFICATIONS == 1) {
  int softButton;
  softButton = analogRead (0);
  if (softButton < 60) { //reads zero 
      //Serial.println(("Soft Right"); 
            p++;
            if(p==12)p=0; 
  }
  else if (softButton < 200){  //reads 99
      //Serial.println(("Soft Up");  
  }
  else if (softButton < 400){ //reads 255
      //Serial.println( ("Soft Down");
  }
  else if (softButton < 600){ //reads 409
      //Serial.println(("Soft Left"); 
            p--;
            if(p==255)p=11;      
  }
  else if (softButton < 800){  //
      //Serial.println( ("Soft Select");
  
             if(p==10){  //ok selected
                parms[parmIdx]=rformat(fmtv);
                LCD::LcdCommandWrite(B00001100); //cursor off
                delay2(250);
                return true;
             }
             else if(p==11){  //cancel selected
                LCD::LcdCommandWrite(B00001100); //cursor off
                delay2(250);
                return false;
             }   			 

             unsigned char n = fmtv[p]-'0';
             n++;
             if (n > 9) n=0;
             if(p==0 && n > 3) n=0;
             fmtv[p]='0'+ n;
             LCD::gotoXY(0,1);        
             LCD::print(fmtv);
             LCD::gotoXY(p,1);        
             if(parmIdx==contrastIdx)//adjust contrast dynamically
                 analogWrite(ContrastPin,rformat(fmtv));  
  } else {  //reads 1023
  }
} //end if UNO_MODIFICATIONS
     delay2(125);
  } 
}

void initGuino(){ //edit all the parameters
  for(int x = 0; x<mylength(parms); x++) {
    if (!editParm(x)) break;
  }
  save();
  HOLD_DISPLAY=1;
}  

unsigned long millis2(){
	return timer2_overflow_count * 64UL * 2 / (16000000UL / 128000UL);
}

void delay2(unsigned long ms){
	unsigned long start = millis2();
        while (millis2() - start < ms);
        maxDelay = MAX(maxDelay, ms);      
}

/* Delay for the given number of microseconds.  Assumes a 16 MHz clock. 
 * Disables interrupts, which will disrupt the millis2() function if used
 * too frequently. */
void delayMicroseconds2(unsigned int us){
	uint8_t oldSREG;
	if (--us == 0)	return;
	us <<= 2;
	us -= 2;
	oldSREG = SREG;
	cli();
	// busy wait
	__asm__ __volatile__ (
		"1: sbiw %0,1" "\n\t" // 2 cycles
		"brne 1b" : "=w" (us) : "0" (us) // 2 cycles
	);
	// reenable interrupts.
	SREG = oldSREG;
}

void init2(){
	// this needs to be called before setup() or some functions won't
	// work there
	sei();
	
	// timer 0 is used for millis2() and delay2()
	timer2_overflow_count = 0;
	// on the ATmega168, timer 0 is also used for fast hardware pwm
	// (using phase-correct PWM would mean that timer 0 overflowed half as often
	// resulting in different millis2() behavior on the ATmega8 and ATmega168)
        TCCR2A=1<<WGM20|1<<WGM21;
	// set timer 2 prescale factor to 64
        TCCR2B=1<<CS22;


//      TCCR2A=TCCR0A;
//      TCCR2B=TCCR0B;
	// enable timer 2 overflow interrupt
	TIMSK2|=1<<TOIE2;
	// disable timer 0 overflow interrupt
	TIMSK0&=!(1<<TOIE0);
}


#if (CFG_SERIAL_TX == 1)
void simpletx( char * string ){
 if (UCSR0B != (1<<TXEN0)){ //do we need to init the uart?
    UBRR0H = (unsigned char)(myubbr>>8);
    UBRR0L = (unsigned char)myubbr;
    UCSR0B = (1<<TXEN0);//Enable transmitter
    UCSR0C = (3<<UCSZ00);//N81
 }
 while (*string)
 {
   while ( !( UCSR0A & (1<<UDRE0)) );
   UDR0 = *string++; //send the data
 }
}
#endif

/* ------------------------------------------------------------------ */
#if( SLEEP_CFG == 3 )
void system_sleep() {
   set_sleep_mode(SLEEP_MODE_PWR_DOWN); // sleep mode is set here
   sleep_enable();
   sleep_mode();                        // System sleeps here
   sleep_disable();                     // System continues execution here when watchdog timed out 
}
#endif
/* ------------------------------------------------------------------ */
#if(DRAGRACE_DISPLAY_CFG)
// DRAG functions
void Drag::reset()
{
   time_400m_ms = 0;
   vss_pulses = 0;
   trap_speed = 0;
   time_100kmh_ms = 0;
   waiting_start = true;
   running = false;
   #if(CFG_UNITS==2)
   vss_400m = ((parms[dragDistance] + 1000) * parms[vssPulsesPerMileIdx]) / 1000;// 0.4;
   #else
   vss_400m = ((parms[dragDistance] + 1000) * parms[vssPulsesPerMileIdx]) / 5280;// 0.4;
   #endif
}

void Drag::start()
{
   time_millis = millis2();
   waiting_start = false;
   running = true;
}

void Drag::update()
{
   unsigned long speed;
   vss_pulses++;
   if( vss_pulses >= vss_400m ) {
     Drag::finish(); 
   }
      if (time_100kmh_ms == 0) {
      speed = instantmph();
      if(speed >= (parms[dragSpeed] * 1000)) { //is this necessary here AND in trap_speed? 
      time_100kmh_ms = millis2() - time_millis;
      }
   }
}

void Drag::finish()
{
   time_400m_ms = millis2() - time_millis;
   trap_speed = instantmph();
   //drag_distance = (vss_pulses * 5280) / parms[vssPulsesPerMileIdx];
   running = false;
}

unsigned long Drag::time()
{
   if( running ) {
      return millis2() - time_millis;
   }
   return time_400m_ms;
}

unsigned long Drag::distance()
{
  unsigned long drag_distance; 
  unsigned long drag_pulses = vss_pulses;

#if (CFG_UNITS == 2) 
  drag_distance = (drag_pulses * 1000) / parms[vssPulsesPerMileIdx];
#else
  drag_distance = (drag_pulses * 5280) / parms[vssPulsesPerMileIdx];
#endif

  if ( waiting_start ) {
    return (drag_distance * 1000);
  }
  else {
  return (drag_distance * 1000);// + 1000; 
  }
}

unsigned long Drag::time100kmh()
{
   return time_100kmh_ms;
}

unsigned long Drag::trapspeed()
{
   unsigned long speed;

   if( running ) {
      speed = instantmph();
      //if(time_100kmh_ms == 0 && speed >= 100000) { 
      //time_100kmh_ms = millis2() - time_millis;   }
      return speed;
   }
   return trap_speed;
}

unsigned long Drag::power()
{
/*
- arvutada massi ja loppkiiruse (trap) abil voimsus: 
trap_mh = trap_kmh / 1.609344;
weight_lbs = weight_kg / 0.45359237;
Hp = Math.pow(trap_mh/234, 3) * weight_lbs;
*/
   double trap_spd = trap_speed;
   double weight = parms[weightIdx];
   double temp1;

   #if(CFG_UNITS==2)
   trap_spd /= 1.609344;
   weight /= 0.45359237;
   #endif

   temp1 = (trap_spd/234000);
   temp1 = (temp1 * temp1 * temp1) * weight;
   
   return (long)(temp1 * 1000);
}
#endif
