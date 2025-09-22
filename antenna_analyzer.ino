//#define DEBUG

#include <PinChangeInterrupt.h>
#include <glcd.h>
#include <fonts/allFonts.h>
#include <EEPROM.h>

//
//     / \  | \ | |_   _|   _(_)_ __   ___  
//    / _ \ |  \| | | || | | | | '_ \ / _ \ 
//   / ___ \| |\  | | || |_| | | | | | (_) |
//  /_/   \_\_| \_| |_| \__,_|_|_| |_|\___/ 
//
//
//  Upload Tips:
//    Under  'Tools / Board', Choose "Arduino Nano"
//    Under 'Tools / Processor", Choose - "Atmega328P (Old Bootloader)"
//
//  Libraries to Add:
//    EEPROM
//    PinChangeInterrupt
//    GLCD Library (Version 3)
//
//    Custom Pin Changes are made to GLCD, so that Analog pins might be freed for use.
//    File: ks0108_Arduino.h in glcd.zip - config folder pin numbers have been altered.
//
//    Updating or otherwise using a 'standard' GLCD will result in NON-FUNCTIONAL LCD.
//
//    The KS0108 Display is 128 wide by 64 high.
//    +------------------------------------+
//    |                                    |
//    |                                    |
//    |                                    |
//    |                                    |
//    |                                    |
//    |                                    |
//    |                                    |
//    |                                    |
//    |                                    |
//    +------------------------------------+
//       
//  Version 2.3 Changes:
//    - Interrupt based encoder replaces polled encoder motion (updateEncoder)
//    - Tidied up DVM for stability and consistency
//    - All Serial.Prints are enclosed by #ifdef DEBUG. Just define DEBUG to debug with Serial.Print
//
//  MODs
//    - Suggested: add three 0.1 uf (100nf) from pins A1, A2, and A3 to ground to improve debounce.
//    - The Previously discussed VTVM (more of a +V DVM, range 0 to 50 VDC).
//
//

unsigned long centerFreq = 14000000l;  //initially set to 25 MHz
unsigned long spanFreq = 25000000l;    //intiially set to 50 MHz
long spans[] = {
  1000l,
  3000l,
  10000l,
  30000l,

  100000l,
  300000l,
  1000000l,

  3000000l,
  10000000l,
  30000000l
};

int selectedSpan = 0;
#define MAX_SPANS 9

uint32_t xtal_freq_calibrated = 27000000l;

/* I/O ports to read the tuning mechanism */
#define ENCODER_PIN_A A3
#define ENCODER_PIN_B A1
#define FBUTTON A2

#define CLK A3
#define DAT A1

volatile int encoderCount = 0;
volatile int lastEncoded = 0;

volatile int currentStateCLK;
volatile int lastStateCLK = 0;
volatile int encoderDir = 0;  // [-1, 0, +1] Rotating Left, Stopped, or Right


/* offsets into the EEPROM storage for calibration */
#define MASTER_CAL 0
#define LAST_FREQ 4
#define OPEN_HF 8
#define OPEN_VHF 12
#define OPEN_UHF 16
#define LAST_SPAN 20
#define LAST_MODE 24

//to switch on/off various clocks
#define SI_CLK0_CONTROL 16  // Register definitions
#define SI_CLK1_CONTROL 17
#define SI_CLK2_CONTROL 18

#define IF_FREQ (24993000l)
#define MODE_ANTENNA_ANALYZER 0
#define MODE_MEASUREMENT_RX 1
#define MODE_NETWORK_ANALYZER 2
char mode = MODE_ANTENNA_ANALYZER;

char b[32], c[32], serial_in[32];
int return_loss;
unsigned long frequency = 10000000l;
int openHF = 96;
int openVHF = 96;
int openUHF = 68;

#define DBM_READING (A6)
int dbmOffset = -114;

int menuOn = 0;
unsigned long timeOut = 0;

//used by the plotting module
int16_t plot_readings[128];
unsigned long f, f1, f2;
uint32_t current_freq = 10000000l;
uint8_t plot_cursor = 64;
uint32_t step_size;  //this not the span size, this is steps in hertz per step



/*
 * The return loss in dB to vswr look up.
 * The VSWR is stored as multiplied by 10, i.e. VSWR of 1.5 is stored as 15
 */
const int PROGMEM vswr[] = {
  999,  // 0 db
  174,  // 1 db
  87,   // 2 db
  58,   // 3 db
  40,   // 4 db
  35,   // 5 db
  30,   // 6 db
  26,   // 7 db
  20,   // 8 db
  19,   // 9 db
  18,   // 10 db
  17,   // 11 db
  16,   // 12 db
  15,   // 13 db
  14,   // 14 db
  14,   // 15 db
  14,   // 16 db
  13,   // 17 db
  13,   // 18 db
  12,   // 19 db
  12,   // 20 db
  12,   // 21 db
  12,   // 22 db
  11,   // 23 db
  11,   // 24 db
  11,   // 25 db
  11,   // 26 db
  10,   // 27 db
  10,   // 28 db
  10,   // 29 db
  10    //30 db
};


/*
 * The return loss in db to vswr look up.
 * The VSWR is stored as multiplied by 10, i.e. VSWR of 1.5 is stored as 15
 */
const int PROGMEM vswr_lin[] = {
  999,  // 0 db
  174,  // 1 db
  87,   // 2 db
  58,   // 3 db
  44,   // 4 db
  35,   // 5 db
  30,   // 6 db
  26,   // 7 db
  23,   // 8 db
  20,   // 9 db
  19,   // 10 db
  17,   // 11 db
  17,   // 12 db
  16,   // 13 db
  15,   // 14 db
  14,   // 15 db
  14,   // 16 db
  13,   // 17 db
  13,   // 18 db
  12,   // 19 db
  12,   // 20 db
  12,   // 21 db
  12,   // 22 db
  11,   // 23 db
  11,   // 24 db
  11,   // 25 db
  11,   // 26 db
  10,   // 27 db
  10,   // 28 db
  10,   // 29 db
  10    //30 db
};

// const int PROGMEM db_distortion[] = {};    // Appears NOT In Use

void active_delay(int delay_by) {
  unsigned long timeStart = millis();

  while (millis() - timeStart <= delay_by) {
    //Background Work
  }
}

// int tuningClicks = 0;    // Apparently NO LONGER IN USE
// int tuningSpeed = 0;     // Apparently NO LONGER IN USE

// void updateDisplay() {      // Can be used for DEBUG. Not included in #ifdef DEBUG
//   sprintf(b, "%ldK, %ldK/div", frequency/1000, spanFreq/10000); 
//   GLCD.DrawString(b, 20, 57);
// }



/* it takes the explicit parameter of the newfreq
 *  and the global variable mode to setup the 
 *  oscillators for the freq and mode
 */
void tune_to(long newfreq) {
  long local_osc;

  if (newfreq < 20000l)
    newfreq = 20000l;
  if (newfreq < 285000000l)
    local_osc = newfreq + IF_FREQ;
  else {
    newfreq = newfreq / 3;
    local_osc = newfreq - IF_FREQ / 3;
  }

  switch (mode) {
    case MODE_MEASUREMENT_RX:
      si5351_setfreq(2, local_osc);
      break;
    case MODE_NETWORK_ANALYZER:
      si5351_setfreq(2, local_osc);
      si5351_setfreq(0, newfreq);
      break;
    default:
      si5351_setfreq(2, local_osc);
      si5351_setfreq(1, newfreq);
  }
}

/*
 * GUI that allows you to tweak the actual frequency of the reference
 * crystal until the clock is generating exactly 10 MHz signal on the
 * RF out port.
 */
void calibrateClock() {
  int knob = 0;
  int32_t prev_calibration;
  char *p;

  GLCD.ClearScreen();
  GLCD.DrawString("1 Monitor RF Out port", 0, 0);
  GLCD.DrawString(" on 10 MHz freq.", 0, 10);
  GLCD.DrawString("2 Tune to zero-beat", 0, 20);
  GLCD.DrawString("3 Click to Save", 0, 30);

  GLCD.DrawString("Save", 64, 45);
  GLCD.DrawRect(60, 40, 35, 20);


  //keep clear of any previous button press
  while (btnDown())
    active_delay(100);
  active_delay(100);

  prev_calibration = xtal_freq_calibrated;
  xtal_freq_calibrated = 27000000l;

  si5351_setfreq(0, 10000000l);
  si5351_reset();

  ltoa(xtal_freq_calibrated - 27000000l, c, 10);
  GLCD.FillRect(0, 40, 50, 15, WHITE);
  GLCD.DrawString(c, 4, 45);

  while (!btnDown()) {

    switch (encoderDir) {
      case -1:
        xtal_freq_calibrated -= 10;
        break;
      case 1:
        xtal_freq_calibrated += 10;
        break;
      default:
        //don't update the frequency or the display
        break;
    }
    encoderDir = 0;

    si5351_setfreq(0, 10000000l);
    si5351_reset();
    ltoa(xtal_freq_calibrated - 27000000l, c, 10);
    GLCD.FillRect(0, 40, 50, 15, WHITE);
    GLCD.DrawString(c, 4, 45);
  }

  while (btnDown())
    delay(100);
  delay(100);
  GLCD.ClearScreen();
  GLCD.DrawString("Calibration Saved!", 0, 25);

  EEPROM.put(MASTER_CAL, xtal_freq_calibrated);
  delay(2000);
}

/*
 * readOpen reads the open port of RFIN,
 * it assumes that the mode is already set to
 * MODE_ANTENNA_ANALYZER. This is stored
 * and used as the 0db return losss.
 */

int readOpen(unsigned long f) {
  int i, r;

  tune_to(f);
  delay(100);

  r = 0;
  for (i = 0; i < 10; i++) {
    r += analogRead(DBM_READING) / 5;
    delay(50);
  }
  //debug the open reading
  itoa(r, c, 10);
  //sprintf(c, "%04d", r);
  GLCD.DrawString(c, 0, 42);

  delay(1000);

  return (r + 5) / 10;
}

/*
 * This actually calibrates the return loss at open port.
 * The calibration uses HF, VHF and UHF as three distinct numbers for those ranges
 */
void calibrateMeter() {

  GLCD.ClearScreen();
  GLCD.DrawString("Disconnect Antenna", 0, 0);
  GLCD.DrawString("port and press Button", 0, 10);
  GLCD.DrawString("to Calibrate SWR", 0, 20);
  GLCD.DrawString("OK", 10, 42);
  GLCD.DrawRect(5, 35, 20, 20);

  //wait for a button down
  while (!btnDown())
    active_delay(50);

  GLCD.ClearScreen();
  GLCD.DrawString("Calibrating.....", 10, 25);
  delay(1000);

  int i, r;
  mode = MODE_ANTENNA_ANALYZER;
  delay(100);

  r = readOpen(20000000l);

#ifdef DEBUG
  Serial.print("open reading of HF is ");
  Serial.println(r);
  EEPROM.put(OPEN_HF, r);

  r = readOpen(140000000l);
  Serial.print("open reading of VHF is ");
  Serial.println(r);
  EEPROM.put(OPEN_VHF, r);

  r = readOpen(440000000l);
  Serial.print("open reading of UHF is ");
  Serial.println(r);
  EEPROM.put(OPEN_UHF, r);
#endif

  menuOn = 0;

  GLCD.ClearScreen();
  GLCD.DrawString("Done!", 10, 25);
  delay(1000);

  //switch off just the tracking source
  si5351a_clkoff(SI_CLK0_CONTROL);
}

/*
 * Returns the open reading expected on a particular frequency,
 * determines which of the three readings to be returned as 
 * measured during the last calibration. 
 * These readings are stored in the EEPROM.
 */
int openReading(unsigned long f) {
  if (f < 60000000l)
    return openHF;
  else if (f < 150000000l)
    return openVHF;
  else
    return openUHF;
}

/*
 * The test_sweep is never called by any program. It is 
 * just something to demonstrate how to sweep
 */
void test_sweep() {
  int32_t f;
  for (int i = 0; i < 100; i++) {
    uint32_t start_time = millis();
    int count = 0;
    for (f = 10000000l; f < 20000000l; f += 5000l) {
      /*analogRead(DBM_READING);
      analogRead(DBM_READING);
      analogRead(DBM_READING);*/
      si5351_setfreq(0, f);
      delayMicroseconds(30);
      analogRead(DBM_READING);
      analogRead(DBM_READING);
      //analogRead(DBM_READING);
      //analogRead(DBM_READING);
      //delay(20);
      count++;
    }
#ifdef DEBUG
    Serial.print("iteration:");
    Serial.print(i);
    Serial.print(", sweep time:");
    Serial.println(millis() - start_time);
    Serial.print(", samples:");
    Serial.println(count);
#endif
  }
  delay(30000);
}

void setup() {
  GLCD.Init();
  GLCD.SelectFont(System5x7);

  Serial.begin(115200);
  b[0] = 0;

#ifdef DEBUG
  Serial.flush();
  Serial.println(F("*Antuino v2.21 KM5Z Edition"));
#endif
  analogReference(DEFAULT);

  unsigned long last_freq = 0;
  EEPROM.get(MASTER_CAL, xtal_freq_calibrated);
  EEPROM.get(LAST_FREQ, last_freq);
  EEPROM.get(OPEN_HF, openHF);
  EEPROM.get(OPEN_VHF, openVHF);
  EEPROM.get(OPEN_UHF, openUHF);
  EEPROM.get(LAST_SPAN, selectedSpan);
  EEPROM.get(LAST_MODE, mode);

#ifdef DEBUG
  Serial.print("*hf_open:");
  Serial.println(openHF);
#endif

  //the openHF reading is actually -19.5 dbm
  dbmOffset = -23 - openHF;

#ifdef DEBUG
  Serial.println(last_freq);
#endif
  if (0 < last_freq && last_freq < 500000000l)
    centerFreq = last_freq;

  if (xtal_freq_calibrated < 26900000l || xtal_freq_calibrated > 27100000l)
    xtal_freq_calibrated = 27000000l;

  if (mode < 0 || mode > 2)
    mode = 0;

  spanFreq = spans[selectedSpan];

  pinMode(ENCODER_PIN_A, INPUT_PULLUP);  // Make them act like Digital Input Pins
  pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  pinMode(FBUTTON, INPUT_PULLUP);

  // Attach a pin change interrupt to both encoder pins
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(ENCODER_PIN_A), updateEncoder, CHANGE);
  attachPinChangeInterrupt(digitalPinToPinChangeInterrupt(ENCODER_PIN_B), updateEncoder, CHANGE);

  si5351a_clkoff(SI_CLK0_CONTROL);
  si5351_reset();

  if (btnDown())
    calibration_mode();
  else
    updateScreen();

  setupSweep();
}

static int prev_dvm = 0;
unsigned long profile;
void loop() {
  updateCursor();
  doSweep();
  if (btnDown())
    doMenu();
}

//
// Quadrature Encoders
//
// rotation direction 'CW'
//      __    __    __    __    __
// A __|  |__|  |__|  |__|  |__|        A leads B
//       __    __    __    __    __
// B  __|  |__|  |__|  |__|  |__|
//
// rotation direction 'CCW'
//        __    __    __    __    __
// A   __|  |__|  |__|  |__|  |__|      B leads A
//       __    __    __    __    __
// B  __|  |__|  |__|  |__|  |__|
//
// we interrupt on a change of PHASE
//  when A leads B
//  if A=1 and B=0 or
//  if A=0 and B=1  count++  or CW
//
//  when B leads A
//  if A=1 and B=1 or
//  if A=0 and B=0 count--  or CCW
//

//
// updateEncoder
//   Interrupt Routine to catch Encoder movement
//   No need to Poll and Wait for encoder clicks
//
void updateEncoder() {
  // Read the current state of CLK
  currentStateCLK = digitalRead(CLK);

  // If last and current state of CLK are different, then pulse occurred
  // React to only 1 state change to avoid double count
  if (currentStateCLK != lastStateCLK && currentStateCLK == 1) {

    // If the DT state is different than the CLK state then
    // the encoder is rotating CCW so decrement
    if (digitalRead(DAT) == currentStateCLK) {
      encoderCount--;
      encoderDir = -1;
    } else {
      // Encoder is rotating CW so increment
      encoderCount++;
      encoderDir = +1;
    }
  }

  // Remember last CLK state
  lastStateCLK = currentStateCLK;

#ifdef DEBUG
  Serial.print(encoderDir);
  Serial.println(encoderCount);
#endif
}