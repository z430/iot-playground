/*
 * Automated Water Cycle System with Menu Interface & Fast Scroll
 * * Hardware:
 * - Relay 1 (Pin 8): Fill Pump
 * - Relay 2 (Pin 9): Drain Pump
 * - HC-SR04: Trig (11), Echo (12)
 * - I2C LCD: A4 (SDA), A5 (SCL)
 * - Buttons: Pin 4 (Menu), Pin 5 (Up), Pin 6 (Down), Pin 7 (Start/Stop)
 * - Buzzer: Pin 10 (Active High)
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
const int buzzerPin = 10; // Added Buzzer Pin
const int trigPin = 11;
const int echoPin = 12;

// --- SETTINGS VARIABLES (Adjustable via Menu) ---
int setDurationMins = 360; // Default 6 hours (360 mins)
int setHighDist = 10;      // Default 10 cm
int setLowDist = 50;       // Default 50 cm
int setMaxCycles = 1;      // Default 1 cycle

// --- SYSTEM VARIABLES ---
bool isRunning = false;
bool isFinished = false;
int completedCycles = 0;
int menuPage = 0; // 0: Duration, 1: HighDist, 2: LowDist, 3: MaxCycles

// --- OBJECTS ---
LiquidCrystal_I2C lcd(0x27, 16, 2);

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

// Long Press / Fast Scroll Variables
unsigned long btnPressStartTime = 0;
unsigned long lastRepeatTime = 0;
const int LONG_PRESS_DELAY = 500; // ms to wait before fast scroll starts
const int REPEAT_RATE = 100;      // ms between changes during fast scroll

void setup() {
  // Input Pins (Buttons)
  pinMode(btnMenu, INPUT_PULLUP);
  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);
  pinMode(btnStart, INPUT_PULLUP);

  // Output Pins
  pinMode(relayFill, OUTPUT);
  pinMode(relayDrain, OUTPUT);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT); // Initialize Buzzer
  
  // Relays OFF
  digitalWrite(relayFill, HIGH);
  digitalWrite(relayDrain, HIGH);
  digitalWrite(buzzerPin, LOW);

  Serial.begin(9600);
  
  lcd.init();
  lcd.backlight();
  
  lcd.setCursor(0,0);
  lcd.print("System Ready");
  beep(50, 2); // Init sound
  delay(1000);
  lcd.clear();
}

void loop() {
  // Read Buttons
  handleButtons();

  if (isRunning) {
    runCycleLogic();
  } else {
    displayMenu();
  }
}

// --- BUTTON & MENU LOGIC ---
void handleButtons() {
  bool rMenu = digitalRead(btnMenu);
  bool rUp = digitalRead(btnUp);
  bool rDown = digitalRead(btnDown);
  bool rStart = digitalRead(btnStart);

  // 1. START/STOP BUTTON (Single Click Only)
  if (rStart == LOW && lastBtnStart == HIGH) {
    if (!isRunning) {
      // Start System
      isRunning = true;
      isFinished = false;
      completedCycles = 0;
      currentState = STATE_FILLING; 
      stateStartTime = millis();
      lcd.clear();
      logStateChange("STARTED");
      beep(200, 1); // Start sound
    } else {
      // Stop System
      stopSystem("STOPPED BY USER");
    }
    delay(200); // Debounce
  }
  lastBtnStart = rStart;

  // If Running, ignore setting buttons to prevent accidental changes
  if (isRunning) return;

  // 2. MENU BUTTON (Single Click Only)
  if (rMenu == LOW && lastBtnMenu == HIGH) {
    menuPage++;
    if (menuPage > 3) menuPage = 0;
    lcd.clear();
    delay(200);
  }
  lastBtnMenu = rMenu;

  // 3. UP BUTTON (With Long Press Logic)
  if (rUp == LOW) {
    if (lastBtnUp == HIGH) {
      // Initial Press
      adjustSetting(1);
      btnPressStartTime = millis();
      lastRepeatTime = millis();
      delay(20); // Small debounce
    } else {
      // Holding Down
      if (millis() - btnPressStartTime > LONG_PRESS_DELAY) {
        if (millis() - lastRepeatTime > REPEAT_RATE) {
          adjustSetting(1);
          lastRepeatTime = millis();
        }
      }
    }
  }
  lastBtnUp = rUp;

  // 4. DOWN BUTTON (With Long Press Logic)
  if (rDown == LOW) {
    if (lastBtnDown == HIGH) {
      // Initial Press
      adjustSetting(-1);
      btnPressStartTime = millis();
      lastRepeatTime = millis();
      delay(20); // Small debounce
    } else {
      // Holding Down
      if (millis() - btnPressStartTime > LONG_PRESS_DELAY) {
        if (millis() - lastRepeatTime > REPEAT_RATE) {
          adjustSetting(-1);
          lastRepeatTime = millis();
        }
      }
    }
  }
  lastBtnDown = rDown;
}

void adjustSetting(int direction) {
  switch (menuPage) {
    case 0: // Duration
      setDurationMins += direction;
      if (setDurationMins < 1) setDurationMins = 1;
      break;
    case 1: // High Dist
      setHighDist += direction;
      if (setHighDist < 2) setHighDist = 2; // Min limit
      break;
    case 2: // Low Dist
      setLowDist += direction;
      if (setLowDist < setHighDist + 5) setLowDist = setHighDist + 5; // Safety gap
      break;
    case 3: // Max Cycles
      setMaxCycles += direction;
      if (setMaxCycles < 1) setMaxCycles = 1;
      break;
  }
}

void displayMenu() {
  if (isFinished) {
    lcd.setCursor(0, 0);
    lcd.print("Done! Cycles: ");
    lcd.print(completedCycles);
    lcd.setCursor(0, 1);
    lcd.print("Press Stop->Reset");
    return;
  }

  lcd.setCursor(0, 0);
  switch (menuPage) {
    case 0: lcd.print("Set Hold Time:  "); break;
    case 1: lcd.print("Set High Level: "); break;
    case 2: lcd.print("Set Low Level:  "); break;
    case 3: lcd.print("Set Cycles:     "); break;
  }
  
  lcd.setCursor(0, 1);
  lcd.print("Val: ");
  switch (menuPage) {
    case 0: 
      lcd.print(setDurationMins); 
      lcd.print(" mins   ");
      break;
    case 1: 
      lcd.print(setHighDist); 
      lcd.print(" cm     ");
      break;
    case 2: 
      lcd.print(setLowDist); 
      lcd.print(" cm     ");
      break;
    case 3: 
      lcd.print(setMaxCycles); 
      lcd.print(" times  ");
      break;
  }
}

// --- CYCLE LOGIC ---
void runCycleLogic() {
  currentDistance = getDistance();
  unsigned long timeElapsed = millis() - stateStartTime;
  
  // Convert minutes to millis
  unsigned long durationMillis = (unsigned long)setDurationMins * 60000UL;

  SystemState oldState = currentState;

  switch (currentState) {
    case STATE_FILLING:
      digitalWrite(relayFill, LOW); // ON
      digitalWrite(relayDrain, HIGH); // OFF
      if (currentDistance > 0 && currentDistance <= setHighDist) {
        digitalWrite(relayFill, HIGH);
        currentState = STATE_HOLD_HIGH;
        stateStartTime = millis();
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
      digitalWrite(relayDrain, LOW); // ON
      if (currentDistance >= setLowDist) {
        digitalWrite(relayDrain, HIGH);
        currentState = STATE_HOLD_LOW;
        stateStartTime = millis();
      }
      break;

    case STATE_HOLD_LOW:
      digitalWrite(relayFill, HIGH);
      digitalWrite(relayDrain, HIGH);
      if (timeElapsed >= durationMillis) {
        // One full cycle complete
        completedCycles++;
        if (completedCycles >= setMaxCycles) {
          stopSystem("ALL CYCLES DONE");
          isFinished = true;
          beep(500, 3); // Finished Sound
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

  // Only update LCD if still running
  if (isRunning) {
    updateRunningLCD(timeElapsed, durationMillis);
    delay(200); // UI Refresh Rate
  }
}

void stopSystem(String reason) {
  isRunning = false;
  digitalWrite(relayFill, HIGH);
  digitalWrite(relayDrain, HIGH);
  lcd.clear();
  logStateChange(reason);
  beep(500, 1); // Stop sound
}

// --- HELPERS ---

// Helper: Make Buzzer Sound
void beep(int duration, int count) {
  for (int i = 0; i < count; i++) {
    digitalWrite(buzzerPin, HIGH);
    delay(duration);
    digitalWrite(buzzerPin, LOW);
    if (count > 1 && i < count - 1) delay(100); // Pause between beeps
  }
}

void updateRunningLCD(unsigned long elapsed, unsigned long targetDuration) {
  // Row 0: Status + Distance
  lcd.setCursor(0, 0);
  switch (currentState) {
    case STATE_FILLING:   lcd.print("Fill "); break;
    case STATE_HOLD_HIGH: lcd.print("Wait "); break;
    case STATE_DRAINING:  lcd.print("Drn  "); break;
    case STATE_HOLD_LOW:  lcd.print("Wait "); break;
  }
  lcd.print(currentDistance);
  lcd.print("cm ");
  // Cycle Info
  lcd.print("C:");
  lcd.print(completedCycles + 1);
  lcd.print("/");
  lcd.print(setMaxCycles);

  // Row 1: Timer or Pump Status
  lcd.setCursor(0, 1);
  if (currentState == STATE_FILLING || currentState == STATE_DRAINING) {
    lcd.print("Pumping...      ");
  } else {
    long remaining = targetDuration - elapsed;
    if (remaining < 0) remaining = 0;
    
    // HH:MM:SS format
    unsigned long totSec = remaining / 1000;
    int h = totSec / 3600;
    int m = (totSec % 3600) / 60;
    int s = totSec % 60;

    lcd.print("Rem: ");
    if(h<10) lcd.print("0"); lcd.print(h); lcd.print(":");
    if(m<10) lcd.print("0"); lcd.print(m); lcd.print(":");
    if(s<10) lcd.print("0"); lcd.print(s);
  }
}

void logStatusChange() {
  String s;
  switch(currentState) {
    case STATE_FILLING: s = "FILLING"; break;
    case STATE_HOLD_HIGH: s = "HOLDING HIGH"; break;
    case STATE_DRAINING: s = "DRAINING"; break;
    case STATE_HOLD_LOW: s = "HOLDING LOW"; break;
  }
  logStateChange(s);
  beep(100, 2); // Double beep on state change
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
