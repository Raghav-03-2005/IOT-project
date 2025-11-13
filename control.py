import paho.mqtt.client as mqtt
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS
import datetime

# --- InfluxDB 2.0 Configuration ---
# Make sure you have created this bucket and organization in InfluxDB
INFLUX_URL = "http://localhost:8086"
INFLUX_TOKEN = "YOUR_INFLUX_TOKEN"  # Your InfluxDB token
INFLUX_ORG = "YOUR_ORG"              # Your InfluxDB organization
INFLUX_BUCKET = "geothermal_data"    # The bucket to write to

# --- MQTT Configuration ---
MQTT_BROKER = "localhost"  # Assumes Mosquitto is running on the same Pi
MQTT_PORT = 1883
MQTT_TOPIC = "geodata/+/status"  # Subscribes to all nodes (e.g., geodata/node1/status)

# --- Setup InfluxDB Client ---
# This is the "pen" you will use to write to the database
try:
    influx_client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
    write_api = influx_client.write_api(write_options=SYNCHRONOUS)
    print("InfluxDB Client Initialized.")
except Exception as e:
    print(f"Error connecting to InfluxDB: {e}")
    print("Please check URL, Token, Org, and Bucket details.")
    exit(1)

# --- MQTT Callback Functions ---

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"Connected to MQTT Broker at {MQTT_BROKER}.")
        # Subscribe to the topic(s) once connected
        client.subscribe(MQTT_TOPIC)
        print(f"Subscribed to topic: {MQTT_TOPIC}")
    else:
        print(f"Failed to connect to MQTT Broker, return code {rc}")

def on_message(client, userdata, msg):
    """
    This function is called every time a message is received on the subscribed topic.
    This is where JOB 1 and JOB 2 happen.
    """
    
    # Extract data from the message
    topic = msg.topic
    payload = msg.payload.decode()
    
    try:
        # Example topic: "geodata/node1/status"
        # We split by '/' to get the node_id
        node_id = topic.split('/')[1]
    except Exception as e:
        node_id = "unknown"
        print(f"Warning: Could not parse node_id from topic {topic}: {e}")

    print(f"\n--- Message Received ---")
    print(f"Timestamp: {datetime.datetime.now()}")
    print(f"From Topic: {topic} (Node: {node_id})")
    print(f"Payload: {payload}")

    # --- JOB 1: Data Receiver & Logger ---
    try:
        # Convert payload to an integer
        risk_level = int(payload)
        
        # Create a "Point" for InfluxDB
        # This is the data structure InfluxDB understands
        point = Point("eruption_risk") \
            .tag("node_id", node_id) \
            .field("risk_level", risk_level) \
            .time(datetime.datetime.utcnow(), WritePrecision.NS)

        # Write the point to the database
        write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=point)
        print(f"-> Logged to InfluxDB: Node {node_id}, Risk {risk_level}")

    except Exception as e:
        print(f"Error logging to InfluxDB: {e}")
        return # Don't proceed if logging failed

    # --- JOB 2: Active Alerter ---
    if risk_level == 1:
        # This is a "Warning" state
        print("-> ALERT: 'WARNING' (Level 1) detected. Monitoring.")
        # In a real system, you might log this to a separate file.
        
    elif risk_level == 2:
        # This is a "Danger" state
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
        print("!!!!           DANGER: ERUPTION RISK           !!!!")
        print(f"!!!!  High-risk event (Level 2) detected from {node_id}  !!!!")
        print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!")
        # In a real system, this would call send_email() or send_sms()
        
    else:
        # This is a "Safe" state
        print("-> STATUS: 'SAFE' (Level 0). All normal.")

# --- Main script ---
def main():
    # Create an MQTT client
    mqtt_client = mqtt.Client()
    
    # Assign the callback functions
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    
    # Connect to the broker
    try:
        mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    except Exception as e:
        print(f"Could not connect to MQTT Broker at {MQTT_BROKER}: {e}")
        print("Is Mosquitto running? (sudo systemctl start mosquitto)")
        exit(1)
    
    # Start the network loop. This runs forever, listening for messages.
    # loop_forever() is a blocking call, so it will keep the script alive.
    print("Starting Hub Controller. Listening for messages...")
    mqtt_client.loop_forever()

if __name__ == "__main__":
    main()