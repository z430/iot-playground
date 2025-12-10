/*
 * Automated Water Cycle System with Enhanced UX/UI
 * * Hardware:
 * - Relay 1 (Pin 8): Fill Pump
 * - Relay 2 (Pin 9): Drain Pump
 * - HC-SR04: Trig (11), Echo (12)
 * - I2C LCD: A4 (SDA), A5 (SCL)
 * - Buttons: Pin 4 (Menu), Pin 5 (Up), Pin 6 (Down), Pin 7 (Start/Stop)
 * - Buzzer: Pin 10
 */

#include <Wire.h> 
#include <LiquidCrystal_I2C.h>

// --- PINS ---
const int btnMenu = 4;
const int btnUp = 5;
const int btnDown = 6;
const int btnStart = 7;

const int relayFill = 8;
const int relayDrain = 9;
const int buzzerPin = 10;
const int trigPin = 11;
const int echoPin = 12;

// --- SETTINGS VARIABLES ---
int setHours = 1;          // Default 6 Hours
int setMinutes = 1;        // Default 0 Minutes
int setHighDist = 10;      // Default 10 cm
int setLowDist = 50;       // Default 50 cm
int setMaxCycles = 1;      // Default 1 cycle

// --- SYSTEM VARIABLES ---
bool isRunning = false;
bool isFinished = false;
int completedCycles = 0;
// Pages: 0:Hours, 1:Mins, 2:High, 3:Low, 4:Cycles
int menuPage = 0; 
const int MAX_PAGES = 4;

// --- OBJECTS ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- CUSTOM ICONS (Bitmaps) ---
byte iconClock[8] = {0x0,0xe,0x15,0x17,0x11,0xe,0x0}; // Clock
byte iconDrop[8]  = {0x4,0x4,0xa,0xa,0x11,0x11,0xe,0x0}; // Water Drop
byte iconPlay[8]  = {0x0,0x8,0xc,0xe,0xc,0x8,0x0,0x0};   // Play Triangle
byte iconCheck[8] = {0x0,0x1,0x3,0x16,0x1c,0x8,0x0,0x0}; // Checkmark

// --- STATES ---
enum SystemState {
  STATE_FILLING,
  STATE_HOLD_HIGH,
  STATE_DRAINING,
  STATE_HOLD_LOW
};

SystemState currentState = STATE_FILLING;
unsigned long stateStartTime = 0;
int currentDistance = 0;

// Button States
bool lastBtnMenu = HIGH;
bool lastBtnStart = HIGH;
bool lastBtnUp = HIGH;
bool lastBtnDown = HIGH;

// Fast Scroll Variables
unsigned long btnPressStartTime = 0;
unsigned long lastRepeatTime = 0;
const int LONG_PRESS_DELAY = 500; 
const int REPEAT_RATE = 100;      

void setup() {
  // Input Pins
  pinMode(btnMenu, INPUT_PULLUP);
  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);
  pinMode(btnStart, INPUT_PULLUP);

  // Output Pins
  pinMode(relayFill, OUTPUT);
  pinMode(relayDrain, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  
  // Relays OFF
  digitalWrite(relayFill, HIGH);
  digitalWrite(relayDrain, HIGH);
  digitalWrite(buzzerPin, LOW);

  Serial.begin(9600);
  
  lcd.init();
  lcd.backlight();

  // Create Custom Characters
  lcd.createChar(0, iconClock);
  lcd.createChar(1, iconDrop);
  lcd.createChar(2, iconPlay);
  lcd.createChar(3, iconCheck);
  
  // Intro Screen
  lcd.setCursor(0,0);
  lcd.print("Water Cycle Sys");
  lcd.setCursor(0,1);
  lcd.print("   v2.0 UI");
  beep(50, 2); 
  delay(1500);
  lcd.clear();
}

void loop() {
  handleButtons();

  if (isRunning) {
    runCycleLogic();
  } else {
    displayMenu();
  }
}

// --- BUTTON LOGIC ---
void handleButtons() {
  bool rMenu = digitalRead(btnMenu);
  bool rUp = digitalRead(btnUp);
  bool rDown = digitalRead(btnDown);
  bool rStart = digitalRead(btnStart);

  // 1. START/STOP
  if (rStart == LOW && lastBtnStart == HIGH) {
    if (!isRunning) {
      // Start
      isRunning = true;
      isFinished = false;
      completedCycles = 0;
      currentState = STATE_FILLING; 
      stateStartTime = millis();
      lcd.clear();
      logStateChange("STARTED");
      beep(200, 1);
    } else {
      // Stop
      stopSystem("STOPPED");
    }
    delay(200); 
  }
  lastBtnStart = rStart;

  if (isRunning) return;

  // 2. MENU (Cycle Pages)
  if (rMenu == LOW && lastBtnMenu == HIGH) {
    menuPage++;
    if (menuPage > MAX_PAGES) menuPage = 0;
    lcd.clear();
    delay(200);
  }
  lastBtnMenu = rMenu;

  // 3. UP BUTTON (Fast Scroll)
  if (rUp == LOW) {
    if (lastBtnUp == HIGH) {
      adjustSetting(1);
      btnPressStartTime = millis();
      lastRepeatTime = millis();
      delay(20);
    } else if (millis() - btnPressStartTime > LONG_PRESS_DELAY) {
      if (millis() - lastRepeatTime > REPEAT_RATE) {
        adjustSetting(1);
        lastRepeatTime = millis();
      }
    }
  }
  lastBtnUp = rUp;

  // 4. DOWN BUTTON (Fast Scroll)
  if (rDown == LOW) {
    if (lastBtnDown == HIGH) {
      adjustSetting(-1);
      btnPressStartTime = millis();
      lastRepeatTime = millis();
      delay(20);
    } else if (millis() - btnPressStartTime > LONG_PRESS_DELAY) {
      if (millis() - lastRepeatTime > REPEAT_RATE) {
        adjustSetting(-1);
        lastRepeatTime = millis();
      }
    }
  }
  lastBtnDown = rDown;
}

void adjustSetting(int direction) {
  switch (menuPage) {
    case 0: // Hours
      setHours += direction;
      if (setHours < 0) setHours = 0;
      if (setHours > 48) setHours = 48; // Max 48 hours
      break;
    case 1: // Minutes
      setMinutes += direction;
      if (setMinutes < 0) setMinutes = 59;
      else if (setMinutes > 59) setMinutes = 0; // Wrap around
      break;
    case 2: // High Dist
      setHighDist += direction;
      if (setHighDist < 2) setHighDist = 2;
      break;
    case 3: // Low Dist
      setLowDist += direction;
      if (setLowDist < setHighDist + 5) setLowDist = setHighDist + 5;
      break;
    case 4: // Cycles
      setMaxCycles += direction;
      if (setMaxCycles < 1) setMaxCycles = 1;
      break;
  }
}

// --- MENU DISPLAY (UX IMPROVED) ---
void displayMenu() {
  if (isFinished) {
    lcd.setCursor(0, 0);
    lcd.write(3); // Check Icon
    lcd.print(" Done! Cyc: ");
    lcd.print(completedCycles);
    lcd.setCursor(0, 1);
    lcd.print("Press Stop->Reset");
    return;
  }

  // Header Line
  lcd.setCursor(0, 0);
  switch (menuPage) {
    case 0: lcd.print("Set Hold Hours:"); break;
    case 1: lcd.print("Set Hold Mins:"); break;
    case 2: lcd.print("Set High Level:"); break;
    case 3: lcd.print("Set Low Level:"); break;
    case 4: lcd.print("Set Total Cycl:"); break;
  }
  
  // Value Line with visual cues
  lcd.setCursor(0, 1);
  lcd.print(" > "); // Cursor indicator
  switch (menuPage) {
    case 0: 
      if(setHours < 10) lcd.print("0");
      lcd.print(setHours); 
      lcd.print(" Hr");
      break;
    case 1: 
      if(setMinutes < 10) lcd.print("0");
      lcd.print(setMinutes); 
      lcd.print(" Min");
      break;
    case 2: 
      lcd.print(setHighDist); 
      lcd.print(" cm");
      break;
    case 3: 
      lcd.print(setLowDist); 
      lcd.print(" cm");
      break;
    case 4: 
      lcd.print(setMaxCycles); 
      lcd.print(" times");
      break;
  }
}

// --- LOGIC ---
void runCycleLogic() {
  currentDistance = getDistance();
  unsigned long timeElapsed = millis() - stateStartTime;
  
  // Calculate Total Duration in Milliseconds
  unsigned long durationMillis = ((unsigned long)setHours * 3600000UL) + 
                                 ((unsigned long)setMinutes * 60000UL);

  SystemState oldState = currentState;

  switch (currentState) {
    case STATE_FILLING:
      digitalWrite(relayFill, LOW); 
      digitalWrite(relayDrain, HIGH); 
      if (currentDistance > 0 && currentDistance <= setHighDist) {
        digitalWrite(relayFill, HIGH);
        currentState = STATE_HOLD_HIGH;
      }
      break;

    case STATE_HOLD_HIGH:
      digitalWrite(relayFill, HIGH);
      digitalWrite(relayDrain, HIGH);
      if (timeElapsed >= durationMillis) {
        currentState = STATE_DRAINING;
        stateStartTime = millis(); 
      }
      break;

    case STATE_DRAINING:
      digitalWrite(relayFill, HIGH);
      digitalWrite(relayDrain, LOW); 
      if (currentDistance >= setLowDist) {
        digitalWrite(relayDrain, HIGH);
        currentState = STATE_HOLD_LOW;
      }
      break;

    case STATE_HOLD_LOW:
      digitalWrite(relayFill, HIGH);
      digitalWrite(relayDrain, HIGH);
      if (timeElapsed >= durationMillis) {
        completedCycles++;
        if (completedCycles >= setMaxCycles) {
          stopSystem("DONE");
          isFinished = true;
          beep(500, 3);
        } else {
          currentState = STATE_FILLING;
          stateStartTime = millis();
        }
      }
      break;
  }

  if (currentState != oldState && isRunning) {
    logStatusChange();
  }

  if (isRunning) {
    updateRunningLCD(timeElapsed, durationMillis);
    delay(200); 
  }
}

void stopSystem(String reason) {
  isRunning = false;
  digitalWrite(relayFill, HIGH);
  digitalWrite(relayDrain, HIGH);
  lcd.clear();
  logStateChange(reason);
  beep(500, 1);
}

// --- RUNNING SCREEN (UX IMPROVED) ---
void updateRunningLCD(unsigned long elapsed, unsigned long targetDuration) {
  // Row 0: Play Icon | State | Drop Icon | Distance
  lcd.setCursor(0, 0);
  lcd.write(2); // Play Icon
  lcd.print(" ");
  
  switch (currentState) {
    case STATE_FILLING:   lcd.print("Fill"); break;
    case STATE_HOLD_HIGH: lcd.print("Wait"); break;
    case STATE_DRAINING:  lcd.print("Drain"); break;
    case STATE_HOLD_LOW:  lcd.print("Wait"); break;
  }
  
  lcd.setCursor(9, 0);
  lcd.write(1); // Drop Icon
  lcd.print(currentDistance);
  lcd.print("cm "); // trailing space clear

  // Row 1: Clock Icon | Timer | Cycle Count
  lcd.setCursor(0, 1);
  lcd.write(0); // Clock Icon
  lcd.print(" ");
  
  long remaining = targetDuration - elapsed;
  if (remaining < 0) remaining = 0;
  
  unsigned long totSec = remaining / 1000;
  int h = totSec / 3600;
  int m = (totSec % 3600) / 60;
  int s = totSec % 60;

  if(h<10) lcd.print("0"); lcd.print(h); lcd.print(":");
  if(m<10) lcd.print("0"); lcd.print(m); lcd.print(":");
  if(s<10) lcd.print("0"); lcd.print(s);

  // Cycle Count on the right
  lcd.setCursor(11, 1);
  lcd.print("#");
  lcd.print(completedCycles + 1);
  lcd.print("/");
  lcd.print(setMaxCycles);
}

void beep(int duration, int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(buzzerPin, HIGH);
    delay(duration);
    digitalWrite(buzzerPin, LOW);
    if (count > 1 && i < count - 1) delay(100);
  }
}

void logStatusChange() {
  String s;
  switch(currentState) {
    case STATE_FILLING: s = "FILLING"; break;
    case STATE_HOLD_HIGH: s = "HOLD_HIGH"; break;
    case STATE_DRAINING: s = "DRAINING"; break;
    case STATE_HOLD_LOW: s = "HOLD_LOW"; break;
  }
  logStateChange(s);
  beep(100, 2);
}

void logStateChange(String msg) {
  Serial.print("[");
  Serial.print(millis() / 1000);
  Serial.print("s] ");
  Serial.println(msg);
}

int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  return duration * 0.034 / 2;
}
