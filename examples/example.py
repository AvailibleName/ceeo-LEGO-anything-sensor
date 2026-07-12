import time
import legoeducation as le

def connect_device():
    print("Initializing LEGO device...")
    # NOTE: In the original library you might initialize le.ColorSensor() or similar.
    # In this custom repo, initialize the class for the specific sensor you are building.
    # We will use the base Oled class as a placeholder example.
    device = le.Oled()

    print("Searching for LEGO Smart Sensor Emulator...")
    try:
        # You can specify card_color and card_serial if you want to connect to a specific tag.
        # Leave them blank to connect to the first available emulator.
        device.connect()
    except Exception as e:
        print(f"Failed to connect due to exception: {e}")
        return

    if not device.connected:
        print("Failed to find or connect to the device.")
        return

    print("Connected successfully!")
    
    # ---------------------------------------------------------
    # YOUR CUSTOM SENSOR CODE GOES HERE
    # e.g., reading values or sending commands to the device
    # ---------------------------------------------------------
    
    # Keep script alive to receive notifications (if applicable)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass

    print("Disconnecting...")
    device.disconnect()
    print("Disconnected.")

if __name__ == '__main__':
    connect_device()
