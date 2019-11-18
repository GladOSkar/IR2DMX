#include <DmxSimple.h>
#include <LiquidCrystal.h>

#include "IR2DMX.h"

/* TODO:
 *  - Remove Magic numbers
 *  - Comment
 *  - Structure
 */

const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

const int ldButton = 7, luButton = 8, rdButton = 9, ruButton = 10;

const int sensorPin = A0;

void setup(void) {

  lcd.begin(16, 2);

  lcd.setCursor(0, 0);
  lcd.print("L A*** B*** C***");
  lcd.setCursor(0, 1);
  lcd.print("R A*** B*** C***");


  //Serial.begin(115200);
  DmxSimple.usePin(6);
  
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

void print4digit(uint16_t nr) {

  if (nr < 1000) lcd.print("0");
  if (nr < 100) lcd.print("0");
  if (nr < 10) lcd.print("0");

  lcd.print(nr, DEC);

}

void print3digit(uint8_t nr) {

  if (nr < 100) lcd.print("0");
  if (nr < 10) lcd.print("0");

  lcd.print(nr, DEC);

}

output_t outputs[] = {
  { .channel = 300, .vals = {255, 0, 0} },
  { .channel = 400, .vals = {255, 0, 0} }
};

unsigned int displayTimeoutCounter = 0;

#define DEFAULT_DISPLAY_TIMEOUT 8000; // loops

void updateDisplay(void) {

  if (displayTimeoutCounter > 1)
    return;

  lcd.setCursor(0, 0);
  lcd.print("L A");
  print3digit(outputs[L].vals[0]);
  lcd.print(" B");
  print3digit(outputs[L].vals[1]);
  lcd.print(" C");
  print3digit(outputs[L].vals[2]);

  lcd.setCursor(0, 1);
  lcd.print("R A");
  print3digit(outputs[R].vals[0]);
  lcd.print(" B");
  print3digit(outputs[R].vals[1]);
  lcd.print(" C");
  print3digit(outputs[R].vals[2]);

}

void handleTimeout(void) {

  if (displayTimeoutCounter == 1)
    updateDisplay();

  if (displayTimeoutCounter > 0)
    displayTimeoutCounter--;

}

void clearDMX(void) {

  DmxSimple.write(outputs[L].channel + 0, 0);
  DmxSimple.write(outputs[L].channel + 1, 0);
  DmxSimple.write(outputs[L].channel + 2, 0);

  DmxSimple.write(outputs[R].channel + 0, 0);
  DmxSimple.write(outputs[R].channel + 1, 0);
  DmxSimple.write(outputs[R].channel + 2, 0);

}

void writeDMX(void) {

  DmxSimple.write(outputs[L].channel + 0, outputs[L].vals[0]);
  DmxSimple.write(outputs[L].channel + 1, outputs[L].vals[1]);
  DmxSimple.write(outputs[L].channel + 2, outputs[L].vals[2]);

  DmxSimple.write(outputs[R].channel + 0, outputs[L].vals[0]);
  DmxSimple.write(outputs[R].channel + 1, outputs[L].vals[1]);
  DmxSimple.write(outputs[R].channel + 2, outputs[L].vals[2]);

}

uint16_t pulseLength = 2000; // ms

void triggerDMX(void) {

  DmxSimple.write(outputs[L].channel, outputs[L].vals[0]);
  DmxSimple.write(outputs[R].channel, outputs[R].vals[0]);

  lcd.setCursor(1, 0);
  lcd.print("!");
  lcd.setCursor(1, 1);
  lcd.print("!");

  delay(pulseLength);

  DmxSimple.write(outputs[L].channel, 0);
  DmxSimple.write(outputs[R].channel, 0);

  lcd.setCursor(1, 0);
  lcd.print(" ");
  lcd.setCursor(1, 1);
  lcd.print(" ");
  
}

void updateChannel(lr_t lr) {

  lcd.setCursor(2, lr);
  lcd.print("CH:           ");
  lcd.setCursor(6, lr);
  lcd.print(outputs[lr].channel, DEC);

  displayTimeoutCounter = DEFAULT_DISPLAY_TIMEOUT;

}

uint8_t sensitivityThreshold = 50;

bool settingsMode = false;

void paintSettingsValues(void) {

  lcd.setCursor(3, 1);
  print3digit(sensitivityThreshold);
  lcd.setCursor(10, 1);
  print4digit(pulseLength);

}

void changeSettingsMode(void) {

  settingsMode = !settingsMode;

  if (settingsMode) {

    lcd.setCursor(0, 0);
    lcd.print("  Sensi: PlsLen:");
    lcd.setCursor(0, 1);
    lcd.print("                ");
    
    paintSettingsValues();
    
  } else {
    
    updateDisplay();

  }

}

bool preventNext = false;

void changeVal(lr_t lr, int8_t delta) {

  if (preventNext) {
    preventNext = false;
    return;
  }

  if (settingsMode) {

    if (lr == L) {

      if (sensitivityThreshold <= 10 && delta < 0)
        return;

      if (sensitivityThreshold >= 200 && delta > 0)
        return;
      
      sensitivityThreshold += 10 * delta;
      
    } else if (lr == R) {

      if (pulseLength <= 100 && delta < 0)
        return;

      if (pulseLength >= 8000 && delta > 0)
        return;
      
      pulseLength += 100 * delta;
      
    }

    paintSettingsValues();
    
    return;

  }

  if (outputs[lr].channel == 1 && delta < 0)
    return;

  if (outputs[lr].channel == 512 && delta > 0)
    return;

  clearDMX();

  outputs[lr].channel += delta;
  updateChannel(lr);

  writeDMX();

  // Limit "Press & Hold" speed
  delay(16);

}

uint16_t  cld = 0,
          clu = 0,
          crd = 0,
          cru = 0;

void handleButtons(void) {

  bool  ld = (digitalRead(ldButton) == HIGH),
        lu = (digitalRead(luButton) == HIGH),
        rd = (digitalRead(rdButton) == HIGH),
        ru = (digitalRead(ruButton) == HIGH);

  if ((!ld || !lu) && (cld && clu)) {
    
    changeSettingsMode();
    preventNext = true;
    
  }

  else if ((!rd || !ru) && (crd && cru)) {
    
    triggerDMX();
    preventNext = true;
    
  }

  else {

    #define PRESS_HOLD_TIMEOUT 3600 // loops

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

uint16_t sliderTimeout = 255;
uint8_t sampleTimeout = 255;
uint8_t pV[2][3];

void handleSliders(void) {

  if (displayTimeoutCounter > 0)
    return;

  if (settingsMode)
    return;

  if (sampleTimeout == 0) {
    memcpy(pV[L], outputs[L].vals, 3);
    memcpy(pV[R], outputs[R].vals, 3);
  }

  outputs[L].vals[0] = analogRead(A1) / 4;
  outputs[L].vals[1] = analogRead(A2) / 4;
  outputs[L].vals[2] = analogRead(A3) / 4;
  outputs[R].vals[0] = analogRead(A4) / 4;
  outputs[R].vals[1] = analogRead(A5) / 4;
  outputs[R].vals[2] = analogRead(A6) / 4;

  sliderTimeout = (sliderTimeout + 1) & 0b0000011111111111;
  sampleTimeout = (sampleTimeout + 1) &         0b01111111;

  if ( sliderTimeout == 0 || abs( (int) outputs[L].vals[0] - pV[L][0] ) > 2 ) {
  
    lcd.setCursor(3, 0);
    print3digit(outputs[L].vals[0]);
  
  }
  
  if ( sliderTimeout == 0 || abs( (int) outputs[L].vals[1] - pV[L][1] ) > 2 ) {
    
    lcd.setCursor(8, 0);
    print3digit(outputs[L].vals[1]);
    DmxSimple.write(outputs[L].channel + 1, outputs[L].vals[1]);
    
  }
  
  if ( sliderTimeout == 0 || abs( (int) outputs[L].vals[2] - pV[L][2] ) > 2 ) {
    
    lcd.setCursor(13, 0);
    print3digit(outputs[L].vals[2]);
    DmxSimple.write(outputs[L].channel + 2, outputs[L].vals[2]);
    
  }



  if ( sliderTimeout == 0 || abs( (int) outputs[R].vals[0] - pV[R][0] ) > 2 ) {
  
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


uint16_t sensorValue = 0;

void handleIR(void) {

  uint16_t pSensorValue = sensorValue;
  sensorValue = analogRead(sensorPin);

  //Serial.println(sensorValue);

  if (sensorValue > sensitivityThreshold && pSensorValue < sensitivityThreshold) {

    triggerDMX();

  }

}


void loop(void) {

  handleButtons();

  handleTimeout();

  handleSliders();

  handleIR();

}
