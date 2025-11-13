#include <TensorFlowLite.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/system_setup.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "model.h" // <-- YOUR MODEL.h FILE, CONTAINS 'g_model'
#include <LoRa.h>
#include <DallasTemperature.h>

// --- TFLite Globals ---
const tflite::Model* model = nullptr;
tflite::MicroInterpreter* interpreter = nullptr;
TfLiteTensor* model_input = nullptr;
TfLiteTensor* model_output = nullptr;
constexpr int kTensorArenaSize = 2048; // May need to be larger!
uint8_t tensor_arena[kTensorArenaSize];

// --- Sensor Globals ---
DallasTemperature temp_sensor;

// --- Helper function (from training script) ---
//These are just example values
const float FEATURE_MEANS[] = {55.0, 8.5, 1700.0, 1650.0};
const float FEATURE_STDS[] = {20.0, 5.0, 1200.0, 1150.0};

void setup() {
    Serial.begin(115200);

    // --- 1. Load the TFLite Model ---
    model = tflite::GetModel(g_model); // g_model comes from model.h

    // --- 2. Build the Interpreter ---
    static tflite::MicroMutableOpResolver<4> resolver; // Adjust size
    resolver.AddFullyConnected();
    resolver.AddRelu();
    resolver.AddLogistic(); // Useing Logistic for Sigmoid
    
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize);
    interpreter = &static_interpreter;

    // --- 3. Allocat Tensors ---
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("!!! Failed to allocate tensors!");
        return;
    }
    model_input = interpreter->input(0);
    model_output = interpreter->output(0);
    
    // --- 4. Setup LoRa, Sensors, etc. ---
    if (!LoRa.begin(915E6)) { // Set your frequency
        Serial.println("!!! LoRa init failed!");
    }
    temp_sensor.begin();
    
    Serial.println("Node setup complete. Starting loop...");
}

void loop() {
    // --- 1. Read all sensors ---
    temp_sensor.requestTemperatures();
    float temp_c = temp_sensor.getTempCByIndex(0);
    float flow_lpm = 10.5; // read_flow_sensor();
    float gas_so2 = (float)analogRead(A0);
    float gas_h2s = (float)analogRead(A1);

    // --- 2. Load data into the model's input tensor ---
    // we must keep the data scaling same as python
    model_input->data.f[0] = (temp_c - FEATURE_MEANS[0]) / FEATURE_STDS[0];
    model_input->data.f[1] = (flow_lpm - FEATURE_MEANS[1]) / FEATURE_STDS[1];
    model_input->data.f[2] = (gas_so2 - FEATURE_MEANS[2]) / FEATURE_STDS[2];
    model_input->data.f[3] = (gas_h2s - FEATURE_MEANS[3]) / FEATURE_STDS[3];

    // --- 3. Run Inference ---
    if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("!!! Invoke failed!");
        return;
    }

    // --- 4. Get the Output (NEW BINARY LOGIC) ---
    // The model output is a single float between 0.0 (No) and 1.0 (Yes)
    float eruption_probability = model_output->data.f[0];

    // 50% threshold to decide the class
    int predicted_risk = 0;
    if (eruption_probability > 0.5) {
        predicted_risk = 1;
    }

    Serial.printf("Risk Level: %d (Probability: %.2f)\n", predicted_risk, eruption_probability);

    // --- 5. Send Result via LoRa ---
    LoRa.beginPacket();
    LoRa.print(predicted_risk); // Send the single number (0 or 1)
    LoRa.endPacket();

    delay(30000); // Wait 30 seconds
}
