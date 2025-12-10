#include <BleKeyboard.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// --- CONFIGURATION ---
const char keys[] = {'h', 'j', 'k', 'l'}; 
const int numKeys = 4;

// Timeouts
const unsigned long TIMEOUT_DISCONNECTED_MS = 600000; 
const unsigned long TIMEOUT_PAUSED_MS       = 1800000; 

// --- Display Config ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32  // Changed to 32 for your specific OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

BleKeyboard bleKeyboard("ESP32 Random C3", "DIY", 100);

// --- C3 PIN DEFINITIONS ---
// WARNING: Avoid GPIO 0 & 1 (Used for Crystal) and GPIO 2 & 8 & 9 (Strapping Pins)
const int PIN_SPEED_UP   = 3;  // CHANGED: Pin 0 causes conflicts. Used Pin 3.
const int PIN_SPEED_DOWN = 5;  
const int PIN_START_STOP = 6;  

// I2C Pins for OLED (ESP32-C3 Standard)
const int PIN_SDA = 8;
const int PIN_SCL = 9; // Your screen might label this "SCK"

int currentWPM = 60;          
bool isRunning = false;       
unsigned long lastSentTime = 0;
unsigned long sendDelay = 0;
char lastSentKey = ' ';
unsigned long lastActivityTime = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("--- ESP32 Vim Debug Mode Started ---");
  Serial.println("Waiting for button presses...");
  
  pinMode(PIN_SPEED_UP, INPUT_PULLUP);
  pinMode(PIN_SPEED_DOWN, INPUT_PULLUP);
  pinMode(PIN_START_STOP, INPUT_PULLUP);

  // --- C3 SPECIFIC SLEEP SETUP ---
  esp_deep_sleep_enable_gpio_wakeup(1ULL << PIN_START_STOP, ESP_GPIO_WAKEUP_GPIO_LOW);

  // --- FORCE I2C PINS ---
  // We explicitly tell the ESP32 to use Pin 8 for SDA and Pin 9 for SCL/SCK
  Wire.begin(PIN_SDA, PIN_SCL);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  // --- C3 SPECIFIC WAKEUP CHECK ---
  if(esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
    Serial.println("Woke up from Deep Sleep!");
    display.clearDisplay();
    display.setCursor(0, 10); // Adjusted Y for 32px height
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.print("Wake Up!");
    display.display();
    delay(500);
  }
  
  randomSeed(analogRead(0)); 
  bleKeyboard.begin();
  
  calculateDelay();
  lastActivityTime = millis();
  updateScreen();
}

void loop() {
  checkSleepTimer();
  handleControls();

  if(bleKeyboard.isConnected() && isRunning) {
    unsigned long currentMillis = millis();
    if (currentMillis - lastSentTime >= sendDelay) {
      int randomIndex = random(0, numKeys); 
      char keyToSend = keys[randomIndex];
      bleKeyboard.write(keyToSend);
      
      // Debug Key Sent
      Serial.print("Sending Key: ");
      Serial.println(keyToSend);
      
      lastSentKey = keyToSend;
      lastSentTime = currentMillis;
      lastActivityTime = millis(); 
      updateScreen(); 
    }
  } 
  
  static bool wasConnected = false;
  if (bleKeyboard.isConnected() != wasConnected) {
    if (!bleKeyboard.isConnected()) {
       Serial.println("Bluetooth Disconnected.");
       isRunning = false; 
    } else {
       Serial.println("Bluetooth Connected!");
       lastActivityTime = millis(); 
    }
    updateScreen();
    wasConnected = bleKeyboard.isConnected();
  }
}

void checkSleepTimer() {
  unsigned long currentMillis = millis();
  unsigned long timeSinceActivity = currentMillis - lastActivityTime;

  if (!bleKeyboard.isConnected()) {
    if (timeSinceActivity > TIMEOUT_DISCONNECTED_MS) goToSleep("No BT Signal");
  }
  else if (!isRunning) {
    if (timeSinceActivity > TIMEOUT_PAUSED_MS) goToSleep("System Idle");
  }
}

void goToSleep(String reason) {
  Serial.print("Going to sleep because: ");
  Serial.println(reason);
  
  display.clearDisplay();
  
  // Adjusted layout for 128x32
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.println(reason);
  
  display.setCursor(0, 12);
  display.setTextSize(2);
  display.println("Sleep Zzz");
  display.display();
  delay(2000);
  
  display.ssd1306_command(SSD1306_DISPLAYOFF);
  
  // Enter Deep Sleep
  esp_deep_sleep_start();
}

void handleControls() {
  bool changed = false;
  
  // DEBUG: Check pins constantly? No, that floods serial.
  // We only print inside the IF block to see if it triggers.

  if (digitalRead(PIN_START_STOP) == LOW) {
    Serial.println("DEBUG: Start/Stop Button Pressed");
    if (bleKeyboard.isConnected()) {
      isRunning = !isRunning;
      Serial.print("DEBUG: New State: ");
      Serial.println(isRunning ? "RUNNING" : "PAUSED");
      changed = true;
    } else {
      Serial.println("DEBUG: Start/Stop ignored - Not Connected");
    }
    lastActivityTime = millis();
    delay(250); 
  }
  
  if (digitalRead(PIN_SPEED_UP) == LOW) {
    Serial.println("DEBUG: Speed UP Button Pressed");
    currentWPM += 1;
    if (currentWPM > 300) currentWPM = 300;
    
    Serial.print("DEBUG: New WPM: ");
    Serial.println(currentWPM);
    
    calculateDelay();
    changed = true;
    lastActivityTime = millis(); 
    delay(100); 
  }
  
  if (digitalRead(PIN_SPEED_DOWN) == LOW) {
    Serial.println("DEBUG: Speed DOWN Button Pressed");
    currentWPM -= 1;
    if (currentWPM < 5) currentWPM = 5jkkjkhhjlhkklhhlhljlllljklkjklhlhhjjkjlkkkhklhjhljjjhlhllkjhjlhkjklhlhhjkkjklklkjkjkhjlhjhk;
    
    Serial.print("DEBUG: New WPM: ");
    Serial.println(currentWPM);

    calculateDelay();
    changed = true;
    lastActivityTime = millis(); 
    delay(100); 
  }
  
  if (changed) updateScreen();
}

void calculateDelay() {
  if (currentWPM > 0) sendDelay = 60000 / (currentWPM * 5);
}

void updateScreen() {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  
  // --- ROW 1: STATUS BAR (Top) ---
  display.setTextSize(1);
  display.setCursor(0, 0);
  if (!bleKeyboard.isConnected()) display.print("Disconnected");
  else if (isRunning) display.print("RUNNING");
  else display.print("PAUSED");
  
  display.drawLine(0, 9, 128, 9, SSD1306_WHITE);

  // --- ROW 2: DATA (Bottom) ---
  // Layout: "WPM: [60]   Key: [h]"
  
  // WPM Label
  display.setCursor(0, 16);
  display.setTextSize(1);
  display.print("WPM:");
  
  // WPM Value (Big)
  display.setCursor(26, 14); // Adjusted slightly up for Size 2 font alignment
  display.setTextSize(2);
  display.print(currentWPM);

  // Key Label
  display.setCursor(75, 16);
  display.setTextSize(1);
  display.print("Key:");

  // Key Value (Big)
  display.setCursor(102, 14); // Adjusted slightly up
  display.setTextSize(2);
  display.print(isRunning ? lastSentKey : '-');

  display.display();
}
