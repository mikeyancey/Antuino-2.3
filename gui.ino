//#define DEBUG

int x1, y1, w, h, x2, y2;
#define X_OFFSET 18

#define MENU_EXIT 0
#define MENU_CHANGE_MHZ 1
#define MENU_CHANGE_KHZ 2
#define MENU_CHANGE_HZ 3
#define MENU_SPAN 4
#define MENU_MODE_SWR 5
#define MENU_MODE_PWR 6
#define MENU_MODE_SNA 7

//#define MENU_MODE 5
//#define MENU_BACK1 6

#define MENU_PLOT 7

#define ACTION_SELECT 1
#define ACTION_DESELECT 2
#define ACTION_UP 3
#define ACTION_DOWN 4

#define BUTTON_NORMAL 0
#define BUTTON_FOCUSED 1
#define BUTTON_SELECTED 2

/*
 * The GUI is defined by these buttons.
 * It is quite primitive and not even well written at the moment.
 * The rotary encoder jumps between 'buttons'. Each button has a
 * funciton associated with it like uiFreq, uiSpan etc.
 * 
 * A global variable uiFocus stores the id of the button
 * that currently has the focus ('hover'). the button with the
 * focus is drawn with a square around it.
 * 
 * If you click on the button, the text is reversed to
 * white on black. all the encoder reading is done with the button's 
 * function until another button click releases the selection
 */

struct Button {
  int id;
  int x, y;
  char text[16];
  uint8_t state;
};

// Number of Buttons and their Locations
#define MAX_BUTTONS 8
struct Button buttons[MAX_BUTTONS] = {
  { MENU_EXIT, 30, 2, "OK", 0 },
  { MENU_CHANGE_MHZ, 30, 15, "MHZ", 0 },
  { MENU_CHANGE_KHZ, 55, 15, "KHz", 0 },
  { MENU_CHANGE_HZ, 80, 15, "Hz", 0 },

  { MENU_SPAN, 30, 28, "SPAN", 0 },
  { MENU_MODE_SWR, 30, 41, "SWR", 0 },
  { MENU_MODE_PWR, 60, 41, "PWR", 0 },
  { MENU_MODE_SNA, 90, 41, "SNA", 0 }
};

int uiFocus = MENU_CHANGE_MHZ, knob = 0, uiSelected = -1;


//returns true if the button is pressed
int btnDown() {
  if (digitalRead(FBUTTON) == HIGH)
    return 0;
  else
    return 1;
}

// waits for the button to be released
// and then some more for the button to
// settle down
void button_released() {
  while (btnDown())
    delay(0);
  delay(100);
}

// //reads the current state of the encoders lines
// // why are we using analogRead? no clue i have forgotten
// //
// // ('cause the interrupt pins (2, 3) and all the digital pins were used for something else)
// //
// //  return: SUM of 1 or 0 on ENC_A and 2 or 0 on ENC_B
// //          This shows the direction the encoder is going.
// //          Be easier to do this by interrupt.
// //
// byte enc_state(void) {
//   //    return (analogRead(ENCODER_PIN_A) > 500 ? 1 : 0) + (analogRead(ENCODER_PIN__B) > 500 ? 2: 0);

//   return (analogRead(ENCODER_PIN_A) > 500 ? 1 : 0) + (analogRead(ENCODER_PIN_B) > 500 ? 2 : 0);
// }

/*
 * The two variables enc_history and enc_prev state together work this.
 * enc_history waits for that many clicks before reporting a result.
 * at enc_history = 5 (like, for button selection), the encoder moves slower. at enc-history = 1 it moves 
 * faster (like tuning)
*/
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


// int enc_history = 5;
// int enc_prev_state = 3;
// //
// //  enc_read - DEPRECATED - CURRENTLY IN USE; REPLACE WITH interruptHandler
// //
// //  IN: enc_speed [ENC_SLOW, ENC_FAST]
// // OUT: result [-1, 1]
// //
// int enc_read(uint8_t enc_speed) {
//   int result = 0;
//   byte newState;

//   newState = enc_state();  // Get current state

//   if (newState != enc_prev_state)
//     delay(1);

//   if (enc_state() != newState || newState == enc_prev_state)
//     return 0;

//   //these transitions point to the encoder being rotated anti-clockwise
//   if ((enc_prev_state == 0 && newState == 2) || (enc_prev_state == 2 && newState == 3) || (enc_prev_state == 3 && newState == 1) || (enc_prev_state == 1 && newState == 0)) {
//     enc_history--;
//     //result = -1;
//   }
//   //these transitions point o the enccoder being rotated clockwise
//   if ((enc_prev_state == 0 && newState == 1) || (enc_prev_state == 1 && newState == 3) || (enc_prev_state == 3 && newState == 2) || (enc_prev_state == 2 && newState == 0)) {
//     enc_history++;
//   }
//   enc_prev_state = newState;  // Record state for next pulse interpretation
//   if (enc_history > enc_speed) {
//     result = 1;
//     enc_history = 0;
//   }
//   if (enc_history < -enc_speed) {
//     result = -1;
//     enc_history = 0;
//   }
//   return result;
// }

//
// formats frequency into a string with proper dots in between
//
void freqtoa(unsigned long f, char *s) {
  char p[16];

  memset(p, 0, sizeof(p));
  ultoa(f, p, DEC);
  s[0] = 0;
  //  strcpy(s, "FREQ: ");

  //one mhz digit if less than 10 M, two digits if more

  if (f >= 100000000l) {
    strncat(s, p, 3);
    strcat(s, ".");
    strncat(s, &p[3], 3);
    strcat(s, ".");
    strncat(s, &p[6], 3);
  } else if (f >= 10000000l) {
    strcat(s, " ");
    strncat(s, p, 2);
    strcat(s, ".");
    strncat(s, &p[2], 3);
    strcat(s, ".");
    strncat(s, &p[5], 3);
  } else {
    strcat(s, "  ");
    strncat(s, p, 1);
    strcat(s, ".");
    strncat(s, &p[1], 3);
    strcat(s, ".");
    strncat(s, &p[4], 3);
  }
}


struct Button *get_button(int id) {
  return buttons + id;
}

/*
 * draws the button. if the global variable uiFocus 
 * matches the button id, we draw the frame around it.
 * if the button state is marked selected, then the
 * text and background are inverted
 */
void draw_button(struct Button *btn) {
  int nchars = strlen(btn->text);
  //each char is 5 pixels wide with a 1 pixel gap in-betwen
  // we add a pixel to account for space on the other side
  int width = nchars * 6;
  GLCD.FillRect(btn->x - 2, btn->y - 2, width + 4, 12, WHITE);
  // Serial.print("drawing button");
  GLCD.DrawString(btn->text, btn->x + 1, btn->y + 1);

  if (btn->state == BUTTON_SELECTED) {
    //Serial.print(" inverted ");
    GLCD.InvertRect(btn->x, btn->y, width, 8);
  }
  if (uiFocus == btn->id) {
    //Serial.print(" selected");
    GLCD.DrawRect(btn->x - 2, btn->y - 2, width + 4, 12);
  }
  //Serial.print("\n");
}

//sets up a new state and redraws the button to reflect it
void update_button_state(struct Button *b, uint8_t new_state) {
  b->state = new_state;
  draw_button(b);
}

//changes the button text and redraws it
void update_button_text(struct Button *b, char *text) {
  b->text[11] = 0;
  if (strlen(text) > 11)
    strncpy(b->text, text, 11);
  else
    strcpy(b->text, text);
  draw_button(b);
}

void drawCalibrationMenu(int selection) {

  GLCD.ClearScreen();
  GLCD.FillRect(0, 0, 128, 64, WHITE);
  GLCD.DrawString("--Calibration Menu--", 0, 0);
  GLCD.DrawString("Freq Calibrate", 20, 20);
  GLCD.DrawString("Return Loss", 20, 40);

  if (selection == 0)
    GLCD.DrawRect(15, 15, 100, 20);
  if (selection == 1)
    GLCD.DrawRect(15, 35, 100, 20);
}

//
// Calibrate Mode
//   Hold Function Button DOWN during Power On
//
void calibration_mode() {
  int select_item = 0;  // select_item [0..1]
  int prev_select_item = 0;

  button_released();

  // Initial Calibration Screen
  drawCalibrationMenu(select_item);

  while (1) {

    while (!btnDown()) {
      // Exits on any CLICK

      switch (encoderDir) {
        case -1:
          if (select_item > 0)
            select_item += encoderDir;
          break;
        case 1:
          if (select_item < 1)
            select_item += encoderDir;
          break;
        default:
          // Nothin' to do, Function Knob ain't rotatin'
          // encoderDir == 0;
          break;
      }
      encoderDir = 0;

      // Repaint if there've been changes
      if (select_item != prev_select_item) {
        drawCalibrationMenu(select_item);
        prev_select_item = select_item;
      }
      delay(50);
    }

    button_released();

    if (!select_item)
      calibrateClock();
    else
      calibrateMeter();
  }
}
void uiSWR(int action) {
  struct Button *b = get_button(MENU_MODE_SWR);

  if (mode == MODE_ANTENNA_ANALYZER)
    update_button_state(b, BUTTON_SELECTED);
  else
    update_button_state(b, 0);

  if (action == ACTION_SELECT) {
    update_button_state(b, BUTTON_SELECTED);
    mode = MODE_ANTENNA_ANALYZER;
    uiSNA(0);
    uiPWR(0);
    EEPROM.put(LAST_MODE, mode);
  }
  if (mode == MODE_ANTENNA_ANALYZER)
    update_button_state(b, BUTTON_SELECTED);
  else
    draw_button(b);
}

void uiPWR(int action) {
  struct Button *b = get_button(MENU_MODE_PWR);

  if (mode == MODE_MEASUREMENT_RX)
    update_button_state(b, BUTTON_SELECTED);
  else
    update_button_state(b, 0);

  if (action == ACTION_SELECT) {
    update_button_state(b, BUTTON_SELECTED);
    Serial.println("*** selecting mode to power");
    mode = MODE_MEASUREMENT_RX;
    uiSNA(0);
    uiSWR(0);
    //updateScreen();
    EEPROM.put(LAST_MODE, mode);
  }
  draw_button(b);
}

void uiSNA(int action) {
  struct Button *b = get_button(MENU_MODE_SNA);

  if (mode == MODE_NETWORK_ANALYZER)
    update_button_state(b, BUTTON_SELECTED);
  else
    update_button_state(b, 0);

  if (action == ACTION_SELECT) {
    mode = MODE_NETWORK_ANALYZER;
    update_button_state(b, BUTTON_SELECTED);
    uiPWR(0);
    uiSWR(0);
    //updateScreen();
    EEPROM.put(LAST_MODE, mode);
  }
  draw_button(b);
}

void uiSpan(int action) {
  struct Button *b = get_button(MENU_SPAN);
  if (spanFreq >= 1000000l)
    sprintf(c, "%3ldM", spanFreq / 1000000l);
  else
    sprintf(c, "%3ldK", spanFreq / 1000l);
  update_button_text(b, c);
  update_button_state(b, 0);

  if (action == ACTION_SELECT) {
    b->state = BUTTON_SELECTED;
    //invert the selection
    draw_button(b);
    button_released();

    while (!btnDown()) {  // While Button is UP
      switch (encoderDir) {
        case -1:
          if (selectedSpan > 0) {
            selectedSpan--;
            spanFreq = spans[selectedSpan];
            EEPROM.put(LAST_SPAN, selectedSpan);
          }
          break;
        case 1:
          if (selectedSpan < MAX_SPANS) {
            selectedSpan++;
            spanFreq = spans[selectedSpan];
            EEPROM.put(LAST_SPAN, selectedSpan);
            encoderDir = 0;
          }
          break;
        default:
          continue;
          break;
      }
      encoderDir = 0;
      delay(50);

      if (spanFreq >= 1000000l)
        sprintf(b->text, "%3ldM", spanFreq / 1000000l);
      else
        sprintf(b->text, "%3ldK", spanFreq / 1000l);
      draw_button(b);
    }
    button_released();
  }
  draw_button(b);
}


void set_button_freq(unsigned long f) {
  char p[16];

  struct Button *bmhz = get_button(MENU_CHANGE_MHZ);
  struct Button *bkhz = get_button(MENU_CHANGE_KHZ);
  struct Button *bhz = get_button(MENU_CHANGE_HZ);

  memset(p, 0, sizeof(p));
  ultoa(f, p, DEC);
  bmhz->text[0] = 0;
  bkhz->text[0] = 0;
  bhz->text[0] = 0;


  //one mhz digit if less than 10 M, two digits if more

  if (f >= 100000000l) {
    strncat(bmhz->text, p, 3);
    strncat(bkhz->text, &p[3], 3);
    strncat(bhz->text, &p[6], 3);
  } else if (f >= 10000000l) {
    strcat(bmhz->text, " ");
    strncat(bmhz->text, p, 2);
    strncat(bkhz->text, &p[2], 3);
    strncat(bhz->text, &p[5], 3);
  } else {
    strcat(bmhz->text, "  ");
    strncat(bmhz->text, p, 1);
    strncat(bkhz->text, &p[1], 3);
    strncat(bhz->text, &p[4], 3);
  }
}



void uiFreq(int action) {

  struct Button *bmhz = get_button(MENU_CHANGE_MHZ);
  struct Button *bkhz = get_button(MENU_CHANGE_KHZ);
  struct Button *bhz = get_button(MENU_CHANGE_HZ);

  if (action == 0) {
    //button_released();
    update_button_state(bmhz, 0);
    update_button_state(bkhz, 0);
    update_button_state(bhz, 0);
  }

  set_button_freq(centerFreq);
  draw_button(bmhz);
  draw_button(bkhz);
  draw_button(bhz);

#ifdef DEBUG
  Serial.println(__LINE__);
#endif

  if (action == ACTION_SELECT) {

    if (uiFocus == MENU_CHANGE_MHZ)
      update_button_state(bmhz, BUTTON_SELECTED);
    else if (uiFocus == MENU_CHANGE_KHZ)
      update_button_state(bkhz, BUTTON_SELECTED);
    else if (uiFocus == MENU_CHANGE_HZ)
      update_button_state(bhz, BUTTON_SELECTED);

    button_released();

    while (!btnDown()) {  // While Knob Button UP
      switch (encoderDir) {
        case -1:
          if (centerFreq > 500000l) {
            if (uiFocus == MENU_CHANGE_MHZ)
              centerFreq -= 1000000l;
            else if (uiFocus == MENU_CHANGE_KHZ)
              centerFreq -= 1000l;
            else if (uiFocus == MENU_CHANGE_HZ)
              centerFreq -= 1;
          }
          break;
        case 1:
          if (centerFreq < 499000000l) {
            if (uiFocus == MENU_CHANGE_MHZ)
              centerFreq += 1000000l;
            else if (uiFocus == MENU_CHANGE_KHZ)
              centerFreq += 1000l;
            else if (uiFocus == MENU_CHANGE_HZ)
              centerFreq += 1;
          }
          break;
        default:
          continue;
          break;
      }

      // encoderDir was not 0...
      encoderDir = 0;
      set_button_freq(centerFreq);
      draw_button(bmhz);
      draw_button(bkhz);
      draw_button(bhz);
    }
    delay(200);  //wait for the button to debounce

    update_button_state(bmhz, 0);
    update_button_state(bkhz, 0);
    update_button_state(bhz, 0);
  }
}

static int menu_alive = 0;

void uiExit(int action) {
  struct Button *b = get_button(MENU_EXIT);

  if (action == ACTION_SELECT) {
    menu_alive = 0;
    button_released();
  }
  draw_button(b);
}

void uiMessage(int id, int action) {

  switch (id) {
    case MENU_CHANGE_MHZ:
    case MENU_CHANGE_KHZ:
    case MENU_CHANGE_HZ:
      uiFreq(action);
      break;
    case MENU_SPAN:
      uiSpan(action);
      break;
    case MENU_MODE_SWR:
      uiSWR(action);
      break;
    case MENU_MODE_PWR:
      uiPWR(action);
      break;
    case MENU_MODE_SNA:
      uiSNA(action);
      break;
    case MENU_EXIT:
      uiExit(action);
      break;
    default:
      return;
  }
}

void updateScreen() {
  /*  uiFreq(0);
  uiSpan(0);
  uiMode(0); 
  uiMark(0); */
}

void doMenu() {
  unsigned long last_freq;
  int dvm_rest_count = 0;
  char dvm_value[32];

  // Paint the Screen with the Menu Display
  GLCD.FillRect(0, 0, 127, 63, WHITE);
  GLCD.DrawString("EXIT:", 0, 2);
  GLCD.DrawString("FREQ:", 0, 15);
  GLCD.DrawString("SPAN:", 0, 28);
  GLCD.DrawString("MODE:", 0, 41);
  GLCD.DrawString("VTVM:", 0, 52);
  uiFreq(0);
  uiSpan(0);
  uiPWR(0);
  uiSWR(0);
  uiSNA(0);
  uiExit(0);

  long press_time = millis();
  while (btnDown()) {
    delay(100);
    if (millis() - press_time > 2000) {
      uint32_t mark_freq = ((f2 - f1) * plot_cursor) / 128l + f1;
      centerFreq = mark_freq;
      uiFreq(0);
    }
  }
  delay(100);

  menu_alive = 1;

  while (menu_alive) {

    if (encoderDir != 0) {
      knob += encoderDir;
      encoderDir = 0;
    }

    // Handle cycling through 8 items in the menu
    if (knob < 0)
      knob = 0;
    if (knob > 7)
      knob = 7;

    // Other Strategy is to rotate around continuously
    // if (knob > 7)
    //   knob = 0;
    // if (knob < 0)
    //   knob = 7;

    if (btnDown()) {
      if (uiSelected == -1)
        uiMessage(uiFocus, ACTION_SELECT);
      if (uiSelected != -1)
        uiMessage(uiFocus, ACTION_DESELECT);
    }

    if (uiFocus != knob) {
      int prev = uiFocus;
      uiFocus = knob;
      uiMessage(prev, 0);
      uiMessage(uiFocus, 0);
    }

    // *** Experimental *** DVM
    // Fetch an Analog Value, stabilized by averaging
    int sensorValue = readAnalogAverage(A7);

    // if our value has changed at least by 0.0001
    //  if (abs(sensorValue - prev_dvm) > 10) {   //abs(sensorValue - prev_dvm) > 100
    // We had some change in the DVM....
    prev_dvm = sensorValue;

    // result of analog read ia 0..1023, so ratio to 0..50V, multiply by 50 Volts
    // to get our floating point voltage value divide by the 1024 counts / volt.
    float voltage = sensorValue * 57.0 / 1024.0;  // Convert to voltage  * 50.0 / 1024.0 (added correction factor)

    char displayVoltage[12];
    dtostrf(voltage, 6, 4, displayVoltage);

    // itoa(dvm, b, DEC);       // Convert Numeric to Alpha String (0 .. 1023*57)
    // Voltage is now in a float value representing the real voltage.

    if (fabs(voltage) >= 1.0)
      sprintf(c, "%s V", displayVoltage);
    else if (fabs(voltage) - fabs(0.0) > 0.0)
      sprintf(c, "%s mV", displayVoltage);
    else if (fabs(voltage) - fabs(0.0) == 0.0)
      sprintf(c, "%s V", displayVoltage);
    else
      sprintf(c, "error");

    // DVM Output to Screen
    GLCD.FillRect(41, 52, 80, 8, WHITE);  // Clear the dirty space
    GLCD.DrawString(c, 41, 52);           // VTVM (or ... DVM)

    // #ifdef DEBUG
    //       // Debug Output to Serial Monitor
    //     char atext[60];
    //     sprintf(atext, "A7 Average: %d, Voltage: %s V\n", sensorValue, displayVoltage);
    //     Serial.print(atext);
    //     Serial.print(" vs c:");
    //     Serial.println(c);
    // #endif

    //}
  }
  //on every button, save the freq.
  EEPROM.get(LAST_FREQ, last_freq);
  if (last_freq != centerFreq)
    EEPROM.put(LAST_FREQ, centerFreq);

  setupSweep();
}

//
// Do a short averaging on an Analog Input Pin
//   In: constant value representing Analog Input Pin
//   Out: INT value 0..1023, per Arduino Hardware Specs for
//        the A2D inputs
//
int readAnalogAverage(const uint8_t sensorPin) {
  int numReadings = 10;
  int total = 0;

  for (int i = 0; i < numReadings; i++) {
    total += analogRead(sensorPin);
    delay(5);  // Small delay to allow ADC to settle, if needed
  }

  // Calculate the average
  int averageReading = total / numReadings;

  return (averageReading);
}