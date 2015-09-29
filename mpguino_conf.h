/* --- Compile-time options ---------------------------------- */
#define ATMEGA328 1

#if ATMEGA328 == 1
   #define BARGRAPH_DISPLAY_CFG       1  /* 0=Off 1=On (+968 bytes) */
   #define CFG_BIGFONT_TYPE           0  /* 0=BigNum screens off(-552) 1=Default 2=Modified (+24 bytes) */
   #define CFG_FUELCUT_INDICATOR      3  /* 0=Off 1=char (+54) 2=spinner 3=bubble (+74) */
   #define CFG_IDLE_MESSAGE           0 
   #define CFG_SERIAL_TX              0  /* 0=Off 1=On (+232) */
   #define SLEEP_CFG                  3  /* 0=Off 1=bl (+34) 2=lcd (+14) 3=both (+46)   */
   #define TANK_IN_EEPROM_CFG         1  /* 0=Off 1=On (+34)  */
   #define OUTSIDE_TEMP_CFG           0  /* 0=Off 1=On        */
   #define BATTERY_VOLTAGE            1  /* 0=Off 1=On (+920) */
   #define DRAGRACE_DISPLAY_CFG       1
#else //ATMEGA 168 - not enough memory for fancy stuff
   #define BARGRAPH_DISPLAY_CFG       0  /* 0=Off 1=On (+968 bytes) */
   #define CFG_BIGFONT_TYPE           0  /* 0=BigNum screens off(-552) 1=Default 2=Modified (+24 bytes) */
   #define CFG_FUELCUT_INDICATOR      1  /* 0=Off 1=char (+54) 2=spinner 3=bubble (+74) */
   #define CFG_IDLE_MESSAGE           0 
   #define CFG_SERIAL_TX              0  /* 0=Off 1=On (+232) */
   #define SLEEP_CFG                  3  /* 0=Off 1=bl (+34) 2=lcd (+14) 3=both (+46)   */
   #define TANK_IN_EEPROM_CFG         1  /* 0=Off 1=On (+34)  */
   #define OUTSIDE_TEMP_CFG           0  /* 0=Off 1=On        */
   #define BATTERY_VOLTAGE            0  /* 0=Off 1=On (+920) */
   #define DRAGRACE_DISPLAY_CFG       0
#endif

#define CFG_UNITS                 1  /* 1=US  2=METRIC  */
