#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C LCD (16x2)

const int mq2Pin = A0;      // MQ-2 analog output
const int ledPin = 13;      // LED pin
const int buzzerPin = 8;    // Active buzzer pin

int smokeThreshold = 150;   // Default smoke threshold
int smokeLevel = 0;         // Current smoke reading

// Event log array (stores last 10 events)
String eventLog[10];
int logIndex = 0;

// Simulated power consumption in mA
float powerConsumption = 30.0; 

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(mq2Pin, INPUT);

  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("SmokeAlert      ");
  lcd.setCursor(0, 1);
  lcd.print("Initializing... ");
  delay(2000);
  lcd.clear();

  printMenu();
}

void loop() {
  char choice = 0;
  unsigned long startTime = millis();
  const unsigned long timeout = 60000; // 1 minute

  // Display menu header
  lcd.setCursor(0, 0);
  lcd.print("SmokeAlert      ");
  lcd.setCursor(0, 1);
  lcd.print("Choose 1-6     "); // padded

  Serial.println("\nEnter choice (1-6): ");

  // Wait up to 1 minute for user input
  while (millis() - startTime < timeout) {
    if (Serial.available()) {
      choice = Serial.read();
      while (Serial.available() > 0) Serial.read(); // Clear buffer
      break;
    }
  }

  if (choice == 0) {
    Serial.println("No input detected. Returning to menu...");
    printMenu();
    return;
  }

  // Process choice
  switch (choice) {
    case '1':
      viewCurrentSmokeLevel();
      break;
    case '2':
      setSafetyThreshold();
      break;
    case '3':
      runSmokeDetection();
      break;
    case '4':
      viewPowerConsumption();
      break;
    case '5':
      viewEventLog();
      break;
    case '6':
      exitProgram();
      break;
    default:
      Serial.println("Invalid choice. Please select 1-6.");
      break;
  }

  printMenu();
}

// ---------------- Menu Functions ----------------

void printMenu() {
  Serial.println(F("\n--- Smoke Detector Menu ---"));
  Serial.println(F("1. View Current Smoke Level"));
  Serial.println(F("2. Set Safety Threshold"));
  Serial.println(F("3. Run Smoke Detection"));
  Serial.println(F("4. View Power Consumption"));
  Serial.println(F("5. View Event Log"));
  Serial.println(F("6. Exit Program"));
}

// ---------------- Pause for user ----------------

void waitForMenu() {
  lcd.setCursor(0, 1);
  lcd.print("Press 'm'       "); // show prompt

  Serial.println("\nPress 'm' to return to menu...");

  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      while (Serial.available() > 0) Serial.read(); // clear buffer
      if (c == 'm' || c == 'M') break;
    }
  }
  lcd.clear(); // clear after user presses 'm'
}

// ---------------- Functions ----------------

void viewCurrentSmokeLevel() {
  smokeLevel = analogRead(mq2Pin);
  Serial.print("Current Smoke Level: ");
  Serial.println(smokeLevel);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smoke Level:");
  lcd.setCursor(12, 0);
  lcd.print(smokeLevel);   // show value
  lcd.setCursor(0, 1);
  lcd.print("Press 'm'       ");

  waitForMenu();
}

void setSafetyThreshold() {
  Serial.println("Enter new safety threshold (0-1023): ");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Threshold");
  lcd.setCursor(0, 1);
  lcd.print("Type and press");

  unsigned long startTime = millis();
  const unsigned long timeout = 60000; // 1 minute
  String inputString = "";

  while (millis() - startTime < timeout) {
    if (Serial.available()) {
      char c = Serial.read();

      // Only accept digits
      if (c >= '0' && c <= '9') {
        inputString += c;
        Serial.print(c); // echo input
      }

      // Enter pressed (handle both \n and \r)
      if (c == '\n' || c == '\r') {
        if (inputString.length() > 0) {
          int newThreshold = inputString.toInt();
          if (newThreshold >= 0 && newThreshold <= 1023) {
            smokeThreshold = newThreshold;
            Serial.println();
            Serial.print("New threshold set: ");
            Serial.println(smokeThreshold);

            // Display new threshold and prompt
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Thres: ");
            lcd.print(smokeThreshold);
            lcd.print("   "); // pad to clear old digits
            lcd.setCursor(0, 1);
            lcd.print("Press 'm'       ");

            // Wait for user to press 'm'
            waitForMenu();
            lcd.clear();
            return;
          } else {
            Serial.println();
            Serial.println("Invalid value! Enter 0-1023.");
            inputString = "";
          }
        }
      }
    }
  }

  // Timeout
  Serial.println();
  Serial.println("No input detected in 1 minute. Returning to menu...");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("No input detect");
  lcd.setCursor(0, 1);
  lcd.print("Returning...     ");
  waitForMenu();
}

// ---------------- Smoke Detection ----------------

void runSmokeDetection() {
  Serial.println("Running smoke detection... Press 'q' to stop.");

  bool ledState = false;
  unsigned long lastBlink = 0;
  const unsigned long blinkInterval = 500;

  String lastStatus = ""; // track previous status

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("SmokeDetect     ");
  lcd.setCursor(0, 1);
  lcd.print("Initializing... ");
  delay(500);

  while (true) {
    smokeLevel = analogRead(mq2Pin);

    // Determine current status
    String currentStatus = (smokeLevel > smokeThreshold) ? "ALERT" : "SAFE";

    // Only add to event log if status changed
    if (currentStatus != lastStatus) {
      addEventLog(smokeLevel, currentStatus);
      lastStatus = currentStatus;
    }

    // Update LCD
    lcd.setCursor(0, 0);
    lcd.print("Lvl:");
    lcd.print(smokeLevel);
    lcd.print("   ");  // pad leftover digits
    lcd.setCursor(8, 0);
    lcd.print("Thr:");
    lcd.print(smokeThreshold);
    lcd.print("   ");

    lcd.setCursor(0, 1);
    if (currentStatus == "ALERT") {
      lcd.print("ALERT! Press m "); // pad to clear previous SAFE
      // Blink LED and buzzer
      if (millis() - lastBlink >= blinkInterval) {
        ledState = !ledState;
        digitalWrite(ledPin, ledState);
        digitalWrite(buzzerPin, ledState);
        lastBlink = millis();
      }
    } else {
      lcd.print("SAFE  Press m  "); // pad to clear previous ALERT
      digitalWrite(ledPin, LOW);
      digitalWrite(buzzerPin, LOW);
    }

    // Print to Serial
    Serial.print("Smoke Level: ");
    Serial.print(smokeLevel);
    Serial.print(" Status: ");
    Serial.println(currentStatus);

    // Exit detection if 'q' is pressed
    if (Serial.available()) {
      char c = Serial.read();
      while (Serial.available() > 0) Serial.read();
      if (c == 'q' || c == 'Q') {
        digitalWrite(ledPin, LOW);
        digitalWrite(buzzerPin, LOW);
        Serial.println("Smoke detection stopped.");
        break;
      }
    }

    delay(200);
  }

  // Wait for user to press 'm' before returning to menu
  lcd.setCursor(0, 1);
  lcd.print("Press 'm' to menu");
  waitForMenu();
  lcd.clear();
}

// ---------------- Power & Event Log ----------------

void viewPowerConsumption() {
  Serial.print("Estimated power consumption: ");
  Serial.print(powerConsumption);
  Serial.println(" mA");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Power:");
  lcd.setCursor(7, 0);
  lcd.print(powerConsumption);
  lcd.print("mA");
  lcd.setCursor(0, 1);
  lcd.print("Press 'm'       ");

  waitForMenu();
}

void viewEventLog() {
  Serial.println("--- Event Log ---");
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Check Serial    ");
  lcd.setCursor(0, 1);
  lcd.print("Press 'm'       ");

  for (int i = 0; i < 10; i++) {
    if (eventLog[i] != "") Serial.println(eventLog[i]);
  }

  waitForMenu();
}

// ---------------- Exit Program ----------------

void exitProgram() {
  digitalWrite(ledPin, LOW);
  digitalWrite(buzzerPin, LOW);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Thank you for   ");
  lcd.setCursor(0, 1);
  lcd.print("using SmokeAlert");

  Serial.println("Program exited. Goodbye!");
  while (true) {}
}

// ---------------- Helper Functions ----------------

void addEventLog(int level, String status) {
  unsigned long totalSeconds = millis() / 1000;
  unsigned int hours = totalSeconds / 3600;
  unsigned int minutes = (totalSeconds % 3600) / 60;
  unsigned int seconds = totalSeconds % 60;

  char timeStamp[9];
  sprintf(timeStamp, "%02u:%02u:%02u", hours, minutes, seconds);

  String event = String(timeStamp) + " Level:" + String(level) + " Status:" + status;
  eventLog[logIndex] = event;
  logIndex = (logIndex + 1) % 10;
}
