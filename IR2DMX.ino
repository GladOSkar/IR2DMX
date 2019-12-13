#include <DmxSimple.h>
#include <LiquidCrystal.h>

#include "IR2DMX.h"

/* TODO:
 *  - Improve structure, split into multiple files
 */

/* HARDWARE NOTES:
 *  - Pin A0 is wired to a bunch of IR Photodiodes in parallel with a fairly big resistor. All to ground.
 *  - Pins A1-A6 are wired to linear potentiometers configured as variable voltage dividers. We used 10k.
 *  - Pins 2-5 are the data pins for the LCD. Exact mappings see below. Use manual.
 *  - Pin 6 is wired to the "Transmit" pin of a RS-485 Transceiver. Hook that up the way the manual says!
 *  - Pins 7-10 are wired to the buttons. Active HIGH. Exact mappings see below.
 *  - Pins 11 and 12 are control pins for the LCD. Exact Mappings see below.
 */

// DMX-512 channels
#define DEFAULT_CH_L 300
#define DEFAULT_CH_R 400
#define MIN_CH 1
#define MAX_CH 512

// How long the display shows currently adjusted values
#define DEFAULT_DISPLAY_TIMEOUT 8000 // loops

// How long the DMX pulse is held
#define DEFAULT_PULSE_LENGTH 2000 // ms
#define STEP_PULSE_LENGTH 100 // ms
#define MIN_PULSE_LENGTH 100 // ms
#define MAX_PULSE_LENGTH 8000 // ms

// At what signal level the IR pulse is registered
#define DEFAULT_SENSITIVITY_THRESHOLD 50 // ??
#define STEP_SENSITIVITY_THRESHOLD 10 // ??
#define MIN_SENSITIVITY_THRESHOLD 10 // ??
#define MAX_SENSITIVITY_THRESHOLD 200 // ??

// How long to press to trigger fast value scrolling
#define PRESS_HOLD_TIMEOUT 3600 // loops
// How long to wait in between values while fast value scrolling
#define PRESS_HOLD_WAIT 16 // ms

// How often the sliders are force-read
#define SLIDER_CHANGE_TIMEOUT_MASK 0b0000011111111111
// How fast the sliders are sampled
#define SLIDER_SAMPLE_TIMEOUT_MASK         0b01111111
// How much the slider value has to change to trigger a repaint
#define SLIDER_CHANGE_THRESHOLD 2

// LCD pins
const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

// Button pins
const int ldButton = 7, luButton = 8, rdButton = 9, ruButton = 10;

// IR Photodiode Pin
const int sensorPin = A0;

void setup(void) {

  // LCD setup
  lcd.begin(16, 2);

  lcd.setCursor(0, 0);
  lcd.print("L D*** S*** E***");
  lcd.setCursor(0, 1);
  lcd.print("R D*** S*** E***");


  //Serial.begin(115200);

  // DMX setup
  DmxSimple.usePin(6);

  // IR Photodiode
  pinMode(sensorPin, INPUT);

  // Slider
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A4, INPUT);
  pinMode(A5, INPUT);
  pinMode(A6, INPUT);

  // Buttons
  pinMode(ldButton, INPUT);
  pinMode(luButton, INPUT);
  pinMode(rdButton, INPUT);
  pinMode(ruButton, INPUT);

}

// Prints a number to LCD, 0-Padded to 4 digits
void print4digit(uint16_t nr) {

  if (nr < 1000) lcd.print("0");
  if (nr < 100) lcd.print("0");
  if (nr < 10) lcd.print("0");

  lcd.print(nr, DEC);

}

// Prints a number to LCD, 0-Padded to 3 digits
void print3digit(uint8_t nr) {

  if (nr < 100) lcd.print("0");
  if (nr < 10) lcd.print("0");

  lcd.print(nr, DEC);

}

// Channel data (channels and channel values)
output_t outputs[] = {
  { .channel = DEFAULT_CH_L, .vals = {0, 0, 0} },
  { .channel = DEFAULT_CH_R, .vals = {0, 0, 0} }
};

/*
 * Display Timeout
 * - Control loop decrements once per iteration
 *   - >1 -> Repaints suspended
 *   - =1 -> Repaint triggered
 *   - =0 -> Idle
 */
unsigned int displayTimeoutCounter = 0;

void updateDisplay(void) {

  if (displayTimeoutCounter > 1)
    return;

  lcd.setCursor(0, 0);
  lcd.print("L D");
  print3digit(outputs[L].vals[0]);
  lcd.print(" S");
  print3digit(outputs[L].vals[1]);
  lcd.print(" E");
  print3digit(outputs[L].vals[2]);

  lcd.setCursor(0, 1);
  lcd.print("R D");
  print3digit(outputs[R].vals[0]);
  lcd.print(" S");
  print3digit(outputs[R].vals[1]);
  lcd.print(" E");
  print3digit(outputs[R].vals[2]);

}

void handleTimeout(void) {

  if (displayTimeoutCounter == 1)
    updateDisplay();

  if (displayTimeoutCounter > 0)
    displayTimeoutCounter--;

}

// Sets all current channels to 0
// For when the channel is changed
void clearDMX(void) {

  DmxSimple.write(outputs[L].channel + 0, 0);
  DmxSimple.write(outputs[L].channel + 1, 0);
  DmxSimple.write(outputs[L].channel + 2, 0);

  DmxSimple.write(outputs[R].channel + 0, 0);
  DmxSimple.write(outputs[R].channel + 1, 0);
  DmxSimple.write(outputs[R].channel + 2, 0);

}

// Writes current values to DMX
// For when the channel is changed
void writeDMX(void) {

  DmxSimple.write(outputs[L].channel + 0, outputs[L].vals[0]);
  DmxSimple.write(outputs[L].channel + 1, outputs[L].vals[1]);
  DmxSimple.write(outputs[L].channel + 2, outputs[L].vals[2]);

  DmxSimple.write(outputs[R].channel + 0, outputs[L].vals[0]);
  DmxSimple.write(outputs[R].channel + 1, outputs[L].vals[1]);
  DmxSimple.write(outputs[R].channel + 2, outputs[L].vals[2]);

}

// How long the DMX pulse will be
uint16_t pulseLength = DEFAULT_PULSE_LENGTH; // ms

void triggerDMX(void) {

  // Set main channel
  DmxSimple.write(outputs[L].channel, outputs[L].vals[0]);
  DmxSimple.write(outputs[R].channel, outputs[R].vals[0]);

  // Indicate on screen
  lcd.setCursor(1, 0);
  lcd.print("!");
  lcd.setCursor(1, 1);
  lcd.print("!");

  // Wait for pulse length
  delay(pulseLength);

  // Reset main channel
  DmxSimple.write(outputs[L].channel, 0);
  DmxSimple.write(outputs[R].channel, 0);

  lcd.setCursor(1, 0);
  lcd.print(" ");
  lcd.setCursor(1, 1);
  lcd.print(" ");
  
}

void updateChannel(lr_t lr) {

  // Show main channel number on screen
  lcd.setCursor(2, lr);
  lcd.print("CH:           ");
  lcd.setCursor(6, lr);
  lcd.print(outputs[lr].channel, DEC);

  // Suspend slider repaints for a while
  displayTimeoutCounter = DEFAULT_DISPLAY_TIMEOUT;

}

// Amplitude threshold for IR Photodiodes
uint8_t sensitivityThreshold = DEFAULT_SENSITIVITY_THRESHOLD;

// Whether the settings screen should be displayed
bool settingsMode = false;

// Repaints the setting values
void paintSettingsValues(void) {

  lcd.setCursor(3, 1);
  print3digit(sensitivityThreshold);
  lcd.setCursor(10, 1);
  print4digit(pulseLength);

}

// Switch between normal and settings mode
void changeSettingsMode(void) {

  // Flip
  settingsMode = !settingsMode;

  if (settingsMode) {

    // Paint settings screen
    lcd.setCursor(0, 0);
    lcd.print("  Sensi: PlsLen:");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    
    paintSettingsValues();
    
  } else {

    // Reset display
    updateDisplay();

  }

}

// Set to true to ignore next buttonRelease event
bool preventNext = false;

// Commits and checks button value adjustments
void changeVal(lr_t lr, int8_t delta) {

  // Respect preventNext (s.a.)
  if (preventNext) {
    preventNext = false;
    return;
  }

  if (settingsMode) {

    if (lr == L) {

      // Change sensitivity

      // Check for limits
      if (sensitivityThreshold <= MIN_SENSITIVITY_THRESHOLD && delta < 0)
        return;

      if (sensitivityThreshold >= MAX_SENSITIVITY_THRESHOLD && delta > 0)
        return;

      // Change value
      sensitivityThreshold += STEP_SENSITIVITY_THRESHOLD * delta;
      
    } else if (lr == R) {

      // Change pulse length

      // Check for limits
      if (pulseLength <= MIN_PULSE_LENGTH && delta < 0)
        return;

      if (pulseLength >= MAX_PULSE_LENGTH && delta > 0)
        return;

      // Change value
      pulseLength += STEP_PULSE_LENGTH * delta;
      
    }

    paintSettingsValues();
    
    return;

  }

  // Check for limits
  // I know i should probably abstract this into a bounded data structure
  if (outputs[lr].channel == MIN_CH && delta < 0)
    return;

  if (outputs[lr].channel == MAX_CH && delta > 0)
    return;

  // Zero old channels
  clearDMX();

  // Change channel
  outputs[lr].channel += delta;
  updateChannel(lr);

  // Update new channels
  writeDMX();

  // Limit "Press & Hold" speed
  delay(PRESS_HOLD_WAIT);

}

// Timeout counters for "press & hold" feature
// Double as 'prev' values
uint16_t  cld = 0,
          clu = 0,
          crd = 0,
          cru = 0;

// Check all buttons for inputs and react
void handleButtons(void) {

  // Read current button state
  bool  ld = (digitalRead(ldButton) == HIGH),
        lu = (digitalRead(luButton) == HIGH),
        rd = (digitalRead(rdButton) == HIGH),
        ru = (digitalRead(ruButton) == HIGH);

  // "Both left"
  // One of the left buttons is released when they were both pressed before
  if ((!ld || !lu) && (cld && clu)) {
    
    changeSettingsMode();
    preventNext = true;
    
  }

  // "Both right"
  // One of the right buttons is released when they were both pressed before
  else if ((!rd || !ru) && (crd && cru)) {
    
    triggerDMX();
    preventNext = true;
    
  }

  else {

    // Button is released and was pressed before
    // OR is pressed and was held for a while
    if ((!ld && cld) || (ld && cld > PRESS_HOLD_TIMEOUT))
      changeVal(L, -1);

    if ((!lu && clu) || (lu && clu > PRESS_HOLD_TIMEOUT))
      changeVal(L, +1);

    if ((!rd && crd) || (rd && crd > PRESS_HOLD_TIMEOUT))
      changeVal(R, -1);

    if ((!ru && cru) || (ru && cru > PRESS_HOLD_TIMEOUT))
      changeVal(R, +1);

  }

  // Update timeout counters
  cld = ld ? cld + 1 : 0;
  clu = lu ? clu + 1 : 0;
  crd = rd ? crd + 1 : 0;
  cru = ru ? cru + 1 : 0;

}

// Timeout for when the final measured value settles in
uint16_t sliderTimeout = 255;

// Timeout for how fast to sample slider values for difference measurement for avoiding jitter
uint8_t sampleTimeout = 255;

// Previous values for all sliders to measure difference to avoid jitter
uint8_t pV[2][3];

// Check sliders for inputs and react
void handleSliders(void) {

  // Do not measure and repaint slider values if repaints are suspended
  if (displayTimeoutCounter > 0)
    return;

  // Do not measure and repaint slider values if settings menu is visible
  if (settingsMode)
    return;

  // Save current values into previous for comparison once every short while
  if (sampleTimeout == 0) {
    memcpy(pV[L], outputs[L].vals, 3);
    memcpy(pV[R], outputs[R].vals, 3);
  }

  // Read analog values
  outputs[L].vals[0] = analogRead(A1) / 4;
  outputs[L].vals[1] = analogRead(A2) / 4;
  outputs[L].vals[2] = analogRead(A3) / 4;
  outputs[R].vals[0] = analogRead(A4) / 4;
  outputs[R].vals[1] = analogRead(A5) / 4;
  outputs[R].vals[2] = analogRead(A6) / 4;

  // Update timeout counters
  sliderTimeout = (sliderTimeout + 1) & SLIDER_CHANGE_TIMEOUT_MASK;
  sampleTimeout = (sampleTimeout + 1) & SLIDER_SAMPLE_TIMEOUT_MASK;

  // Update screen values only after a while or on significant change to avoid flickering/jitter
  if ( sliderTimeout == 0 || abs( (int) outputs[L].vals[0] - pV[L][0] ) > SLIDER_CHANGE_THRESHOLD ) {
  
    lcd.setCursor(3, 0);
    print3digit(outputs[L].vals[0]);
  
  }
  
  if ( sliderTimeout == 0 || abs( (int) outputs[L].vals[1] - pV[L][1] ) > SLIDER_CHANGE_THRESHOLD ) {
    
    lcd.setCursor(8, 0);
    print3digit(outputs[L].vals[1]);

    // Also update DMX signal for non-main channels
    DmxSimple.write(outputs[L].channel + 1, outputs[L].vals[1]);
    
  }
  
  if ( sliderTimeout == 0 || abs( (int) outputs[L].vals[2] - pV[L][2] ) > SLIDER_CHANGE_THRESHOLD ) {
    
    lcd.setCursor(13, 0);
    print3digit(outputs[L].vals[2]);
    DmxSimple.write(outputs[L].channel + 2, outputs[L].vals[2]);
    
  }

  if ( sliderTimeout == 0 || abs( (int) outputs[R].vals[0] - pV[R][0] ) > SLIDER_CHANGE_THRESHOLD ) {
  
    lcd.setCursor(3, 1);
    print3digit(outputs[R].vals[0]);
  
  }
  
  if ( sliderTimeout == 0 || abs( (int) outputs[R].vals[1] - pV[R][1] ) > 2 ) {
    
    lcd.setCursor(8, 1);
    print3digit(outputs[R].vals[1]);
    DmxSimple.write(outputs[R].channel + 1, outputs[R].vals[1]);
    
  }
  
  if ( sliderTimeout == 0 || abs( (int) outputs[R].vals[2] - pV[R][2] ) > 2 ) {
    
    lcd.setCursor(13, 1);
    print3digit(outputs[R].vals[2]);
    DmxSimple.write(outputs[R].channel + 2, outputs[R].vals[2]);
    
  }

  /*
  Serial.print(outputs[L].vals[0]);
  Serial.print(",");
  Serial.print(outputs[L].vals[1]);
  Serial.print(",");
  Serial.print(outputs[L].vals[2]);
  Serial.print(",");
  Serial.print(outputs[R].vals[0]);
  Serial.print(",");
  Serial.print(outputs[R].vals[1]);
  Serial.print(",");
  Serial.print(outputs[R].vals[2]);
  Serial.print("\n");
  */

}

// Current IR Photodiode value
uint16_t sensorValue = 0;

// Check IR Photodiodes for inputs and react
void handleIR(void) {

  // save previous value to measure difference
  uint16_t pSensorValue = sensorValue;

  // Read IR value
  sensorValue = analogRead(sensorPin);

  //Serial.println(sensorValue);

  // Trigger on rise over threshold
  if (sensorValue > sensitivityThreshold && pSensorValue < sensitivityThreshold) {

    // Send DMX pulse on main channel
    triggerDMX();

  }

}

// Control loop
// Orchestrates subsystems to check their inputs and react accordingly
void loop(void) {

  handleButtons();

  handleTimeout();

  handleSliders();

  handleIR();

}
