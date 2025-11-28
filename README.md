Smart Edge IoT System for Adaptive Environmental Anomaly Detection

This repository contains the source code for a decentralized Edge AI system designed to monitor geothermal and volcanic activity.

The system uses an ESP32 microcontroller to run TinyML models locally for real-time anomaly detection. It communicates with a Raspberry Pi Hub, which acts as a central manager to dynamically switch the ESP32 between "Passive" (power-saving) and "Active" (disaster response) modes based on the threat level.

📂 File Structure & Explanations

Here is a breakdown of the files included in this repository and their role in the system:

1. Model Training & Generation

ESP32 Notebook.ipynb:

Purpose: The "Brain Builder." This Jupyter Notebook runs on your PC or Google Colab.

Function: It loads the dataset, trains two TensorFlow Neural Networks (Passive & Active), converts them to TensorFlow Lite (.tflite) format, and finally generates the C header files (.h) needed for the ESP32.

data.csv:

Purpose: The training dataset.

Content: Contains historical or synthetic geothermal sensor data (Water Temperature, Flow Rate, SO2, H2S) labeled with "Safe" or "Eruption" status. Used by the notebook to train the models.

2. ESP32 Firmware (The Edge Node)

esp32.c (or .ino):

Purpose: The main firmware for the ESP32 microcontroller.

Function:

Loads the ML models from memory.

Simulates sensor data (using biased random generation).

Runs inference to calculate a "Risk Score."

Sends data via Serial (JSON format) to the Raspberry Pi.

Listens for commands (MODE_DISASTER) to switch models and sampling rates.

model.h:

Purpose: The AI model stored as a C byte array.

Function: This file allows the C++ firmware to load the trained neural network directly from flash memory without a file system. (Note: In the final dual-model version, you may have passive_model.h and active_model.h).

geothermal_eruption_model.tflite:

Purpose: The binary model file.

Function: A raw TFLite model file. Useful for debugging or loading into other interpreters (like Python) to verify predictions off-chip.

3. Raspberry Pi Manager (The Hub)

raspi_manager.py:

Purpose: The "Boss" script running on the Raspberry Pi.

Function:

Connects to the ESP32 via USB Serial.

Reads the JSON data stream.

Analyzes risk trends (e.g., "3 danger signals in a row?").

Sends commands to the ESP32 to switch between Passive and Active modes.

Prevents serial buffer overflows to keep the system crash-proof.

control.py:

Purpose: (Legacy/Helper) Likely an earlier iteration of the control logic or a testing script for verifying serial communication without the full manager logic.

How to Run

Step 1: Train the Models

Open ESP32 Notebook.ipynb.

Upload data.csv.

Run all cells to generate your model.h files.

Step 2: Flash the ESP32

Copy the code from esp32.c into your Arduino IDE.

Ensure model.h is in the same folder.

Upload to your ESP32 board(you can use LittleFS or SPIFFS filesystem of ESP32).

Step 3: Start the Hub

Connect the ESP32 to the Raspberry Pi via USB.

Run the manager script:

python3 raspi_manager.py


Watch the terminal as the system autonomously detects danger and switches modes!

Dependencies

ESP32: EloquentTinyML library.

Python (Pi): pyserial, json.

Python (Training): tensorflow, pandas, scikit-learn.


Future Extensions

While the current system utilizes a USB connection for robustness, scaling to a field-deployable network will require wireless communication.

Wireless MQTT Bridge: Future iterations will replace the USB-Serial link with MQTT over Wi-Fi (for short-range prototyping) or LoRaWAN (for long-range field deployment). The Raspberry Pi would act as an MQTT Broker (using Mosquitto), and the ESP32 would publish JSON payloads to topics like volcano/sensor1/data and subscribe to volcano/sensor1/control for mode-switching commands.

Energy Harvesting: Integration of the planned 1W solar panel and BMS to test true autonomous operation in the field.

Advanced Sensors: Upgrading to industrial-grade electrochemical gas sensors for higher precision in sulfur detection.

Cloud Dashboard: Connecting the Raspberry Pi to a cloud dashboard (e.g., Grafana/InfluxDB) for long-term historical analysis and remote visualization.
