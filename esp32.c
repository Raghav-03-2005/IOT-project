#include "model.h"
#include <EloquentTinyML.h>
#include <FS.h>
#include <LittleFS.h>

// --- 1. SETTINGS ---
#define NUMBER_OF_INPUTS 4
#define NUMBER_OF_OUTPUTS 1
#define TENSOR_ARENA_SIZE 16384
#define LOG_FILENAME "/logs.csv" // Name of the log file

// --- 2. CALIBRATION VALUES (Updated with your data) ---
const float FEATURE_MEANS[] = {46.1278309654112, 3.9463028781206266,
                               0.40330585073749886, 3.5402061864530037};
const float FEATURE_STDS[] = {8.117822112957583, 1.5429810468282663,
                              0.5938779722107673, 3.2940159341811683};

// --- Objects ---
Eloquent::TinyML::TfLite<NUMBER_OF_INPUTS, NUMBER_OF_OUTPUTS, TENSOR_ARENA_SIZE>
    ml;

// Helper to generate a float between min and max
float get_random_value(float minVal, float maxVal) {
  return minVal + (float)random(0, 1000) / 1000.0 * (maxVal - minVal);
}

void setup() {
  Serial.begin(115200);

  // --- THE "AUTO-WIPER" FIX ---
  Serial.write(27);    // ESC command
  Serial.print("[2J"); // clear screen command
  Serial.write(27);
  Serial.print("[H"); // cursor to home command

  Serial.println("\n\n");
  Serial.println("--- SYSTEM STARTING ---");

  delay(2000);

  // --- RANDOMNESS FIX ---
  // analogRead(0) is often constant on ESP32 (Boot button).
  // esp_random() uses the chip's internal thermal noise generator (True RNG).
  randomSeed(esp_random());

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed!");
    return;
  }
  Serial.println("Filesystem Mounted.");

  // --- CHECK / CREATE LOG FILE ---
  if (!LittleFS.exists(LOG_FILENAME)) {
    Serial.println("Creating new log file...");
    File f = LittleFS.open(LOG_FILENAME, "w");
    if (f) {
      f.println("Water_Temp,Flow_Rate,SO2,H2S,Risk_Score,Alert_Status");
      f.close();
    }
  }

  if (!ml.begin(model_tflite)) {
    Serial.println("ERROR: Failed to initialize TFLite interpreter!");
    while (1)
      ;
  }

  Serial.println("\n=== VIRTUAL VOLCANO MONITOR & LOGGER STARTED ===");
  Serial.println("Commands:");
  Serial.println("  'dump' -> Print all saved CSV logs to Serial");
  Serial.println("  'stop' -> Halt the system");
}

void loop() {
  // 0. CHECK FOR SERIAL COMMANDS
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();

    // --- COMMAND: STOP ---
    if (input.equalsIgnoreCase("stop")) {
      Serial.println("\n!!! STOP COMMAND RECEIVED. SYSTEM HALTED. !!!");
      while (1)
        ;
    }

    // --- COMMAND: DUMP LOGS ---
    if (input.equalsIgnoreCase("dump")) {
      Serial.println("\n\n=== BEGIN CSV DUMP ===");
      File f = LittleFS.open(LOG_FILENAME, "r");
      if (f) {
        while (f.available()) {
          Serial.write(f.read());
        }
        f.close();
      } else {
        Serial.println("Error reading log file!");
      }
      Serial.println("\n=== END CSV DUMP ===\n");
      Serial.println("Resuming simulation in 5 seconds...");
      delay(5000);
      return; // Skip the prediction this loop
    }
  }

  // 1. GENERATE FAKE SENSOR DATA
  float rawInputs[4];
  rawInputs[0] = get_random_value(30.0, 100.0); // Temp
  rawInputs[1] = get_random_value(0.0, 10.0);   // Flow
  rawInputs[2] = get_random_value(0.0, 5.0);    // SO2
  rawInputs[3] = get_random_value(0.0, 25.0);   // H2S

  // 2. NORMALIZE
  float scaledInputs[4];
  for (int i = 0; i < 4; i++) {
    scaledInputs[i] = (rawInputs[i] - FEATURE_MEANS[i]) / FEATURE_STDS[i];
  }

  // 3. PREDICT
  float prediction = ml.predict(scaledInputs);

  // Determine Status
  String status = "Safe";
  if (prediction > 0.5)
    status = "ERUPTION";

  // 4. OUTPUT TO SERIAL
  Serial.println("------------------------------------------------");
  Serial.print("Sensors:    [Temp: ");
  Serial.print(rawInputs[0], 1);
  Serial.print(", Flow: ");
  Serial.print(rawInputs[1], 1);
  Serial.print(", SO2: ");
  Serial.print(rawInputs[2], 2);
  Serial.print(", H2S: ");
  Serial.print(rawInputs[3], 2);
  Serial.println("]");

  Serial.print("Risk Score: ");
  Serial.print(prediction, 4);
  Serial.println(" [" + status + "]");

  // 5. LOG TO CSV
  File logFile = LittleFS.open(LOG_FILENAME, "a");
  if (logFile) {
    logFile.print(rawInputs[0], 2);
    logFile.print(",");
    logFile.print(rawInputs[1], 2);
    logFile.print(",");
    logFile.print(rawInputs[2], 3);
    logFile.print(",");
    logFile.print(rawInputs[3], 3);
    logFile.print(",");
    logFile.print(prediction, 4);
    logFile.print(",");
    logFile.println(status);
    logFile.close();
    Serial.println(" -> Logged to CSV");
  }

  delay(2000);
}