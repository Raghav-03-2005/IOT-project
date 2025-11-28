import serial
import time
import json

# CONFIGURATION
# Replace '/dev/ttyUSB0' with 'COM3' if testing on Windows PC
SERIAL_PORT = '/dev/ttyUSB0' 
BAUD_RATE = 115200

def main():
    print("--- RPi Volcano Manager Started (Crash-Proof Version) ---")
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"Connected to ESP32 on {SERIAL_PORT}")
        ser.reset_input_buffer() # Clear any old garbage
        time.sleep(2) 
    except Exception as e:
        print(f"Error connecting: {e}")
        return

    # Logic State
    safe_counter = 0
    current_mode = "PASSIVE"

    print("\nSystem Ready. Monitoring Data Stream...\n")

    try:
        while True:
            # --- CRASH PREVENTION: BUFFER FLUSH ---
            # If the Pi gets behind and the buffer fills up (> 100 bytes),
            # it will freeze trying to catch up. We flush it to stay real-time.
            if ser.in_waiting > 100:
                ser.reset_input_buffer()
                continue 

            if ser.in_waiting > 0:
                try:
                    # 1. Read line from ESP32
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    # 2. Try to parse JSON
                    if line.startswith("{") and line.endswith("}"):
                        data = json.loads(line)
                        
                        temp = data.get("temp", 0.0)
                        risk = data.get("risk", 0.0)
                        esp_mode = data.get("mode", "UNKNOWN")
                        
                        # Print output (Shortened to save CPU)
                        print(f"Rx: Temp={temp:.1f} | Risk={risk:.4f} | Mode={esp_mode}")

                        # --- 3. DECISION LOGIC ---
                        
                        # CASE A: PASSIVE MODE -> LOOK FOR DANGER
                        if current_mode == "PASSIVE":
                            # CHANGED: Trigger IMMEDIATELY on 1st Eruption Signal
                            if risk > 0.7:
                                print("\n" + "!"*40)
                                print(">>> DANGER DETECTED! IMMEDIATE ESCALATION <<<")
                                print("!"*40 + "\n")
                                
                                ser.write(b"MODE_DISASTER\n")
                                ser.flush() # Force send immediately
                                
                                current_mode = "ACTIVE"

                        # CASE B: ACTIVE MODE -> LOOK FOR SAFETY
                        elif current_mode == "ACTIVE":
                            # We still wait for 10 safe readings to be sure
                            # before cooling down (hysteresis is stable)
                            if risk < 0.3:
                                safe_counter += 1
                                print(f"  [i] Safe Signal detected ({safe_counter}/10)")
                            else:
                                safe_counter = 0 # Reset if danger spike appears
                            
                            # Trigger Switch Back
                            if safe_counter >= 10:
                                print("\n" + "-"*40)
                                print(">>> ALL CLEAR: RETURNING TO SAFE MODE <<<")
                                print("-"*40 + "\n")
                                
                                ser.write(b"MODE_SAFE\n")
                                ser.flush()
                                
                                current_mode = "PASSIVE"
                                safe_counter = 0

                except json.JSONDecodeError:
                    # Ignore garbage lines
                    pass
                except Exception as e:
                    print(f"Error parsing: {e}")

            # Tiny sleep to prevent CPU 100% usage, but fast enough to not lag
            time.sleep(0.05)

    except KeyboardInterrupt:
        print("\nStopping Manager...")
        ser.close()

if __name__ == "__main__":
    main()
