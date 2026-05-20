import serial
try:
    import parallel
except ImportError:
    parallel = None
import time
import subprocess
import os
import json

from kuksa_client.grpc import VSSClient, Datapoint

# --- CONFIGURATION ---
SERIAL_PORT = '/dev/ttyACM0'  # Your Arduino address
BAUD_RATE = 9600
KUKSA_IP = '127.0.0.1'
KUKSA_PORT = 55555

# ECU wire harness — maps Arduino analog pin number to wire index (0-19)
# used by the dashboard health page (matches getWireFunction order in App.qml)
ANALOG_TO_WIRE = {
    2: 1,   # Blue/Black   A/C Clutch Sense
    3: 5,   # Blue/White   K-Line Diag
    4: 16,  # Green/White  Glow Plug Relay
    5: 19,  # Black/Red    Power Ground
    6: 15,  # Red/Blue     EGR Solenoid
    7: 0,   # Brown/Red    Main Relay Ctrl
    8: 7,   # Brown/Yellow EGR Stepper/Vent
    9: 9,   # Brown/White  Glow Plug Sense
    10: 11, # Yellow       Check Engine
    11: 8,  # Green        A/C Request
    12: 12, # Pink         FICD (Idle Up)
    13: 18, # Black/Yellow Starter Signal
}
VOLTAGE_SCALE     = 3.0   # voltage-divider recovery: 20kΩ+10kΩ → ×3.0
ACTIVE_THRESHOLD  = 2.0   # volts (real, after scale) — wire considered "live"
WIRE_STATE_FILE   = '/tmp/ecu_wire_states.json'

def unblock_serial_port(port_path="/dev/ttyACM0"):
    print(f"🧹 Checking if {port_path} is blocked...")
    try:
        # 'fuser -k -9' finds the process using the port and kills it immediately
        # We redirect output to DEVNULL to keep the console clean
        subprocess.run(
            ["sudo", "fuser", "-k", "-9", port_path],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        # Give the Linux kernel a moment to actually release the hardware
        time.sleep(1) 
        print(f"✅ {port_path} is now clear.")
    except Exception as e:
        print(f"⚠️ Note: Could not run fuser (might already be clear): {e}")

def run_master_bridge():
    # 1. Initialize Parallel Port (for your indicator LEDs/Tripmeter pulses)
    try:
        lpt = parallel.Parallel(0) if parallel else None
        if lpt:
            print("✅ Parallel Port Initialized.")
        else:
            print("⚠️ Parallel Port: module not available (Continuing without LPT)")
    except Exception as e:
        print(f"⚠️ Parallel Port Warning: {e} (Continuing without LPT)")
        lpt = None

    # 2. Initialize Serial Port (for Hall Sensor data)
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        time.sleep(2) # Give Arduino a moment to reboot after connection
        print(f"✅ Arduino Serial Link Active on {SERIAL_PORT}")
    except Exception as e:
        print(f"❌ Serial Error: {e}. Check your USB connection!")
        return

    # 3. Connect to Kuksa Databroker
    try:
        with VSSClient(KUKSA_IP, KUKSA_PORT) as client:
            print("🚀 Kuksa Link Active. Dashboard is now LIVE.")

            while True:
                # --- PART A: READ FROM ARDUINO ---
                if ser.in_waiting > 0:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    
                    # Format: SPD:xx|RPM:xx|TMP:xx|LITERS:xx|RANGE:xx|AV2:x.xx|...|AV13:x.xx
                    if "SPD:" in line and "|RPM:" in line:
                        try:
                            # Trim any junk before SPD: (e.g. two lines joined on startup)
                            line = line[line.index("SPD:"):]
                            parts = line.split('|')
                            speed_val       = float(parts[0].split(':')[1])
                            rpm_val         = float(parts[1].split(':')[1])
                            temp_val        = float(parts[2].split(':')[1])
                            fuel_liters_val = float(parts[3].split(':')[1])
                            fuel_range_val  = float(parts[4].split(':')[1])

                            IsFuelLevelLow = fuel_liters_val < 30

                            # Push telemetry to KUKSA
                            telemetry = {
                               'Vehicle.Speed':                              Datapoint(speed_val),
                               'Vehicle.OBD.EngineSpeed':                   Datapoint(rpm_val),
                               'Vehicle.OBD.FuelLevel':                     Datapoint(fuel_liters_val),
                               'Vehicle.Powertrain.FuelSystem.Range':       Datapoint(fuel_range_val),
                               'Vehicle.Powertrain.FuelSystem.IsFuelLevelLow': Datapoint(IsFuelLevelLow),
                            }
                            # -100 means thermocouple disconnected — skip temp publish
                            if temp_val != -100.0:
                                telemetry['Vehicle.OBD.CoolantTemperature'] = Datapoint(temp_val)
                            client.set_current_values(telemetry)
                            print(f"TELEMETRY -> Speed: {speed_val} km/h | RPM: {rpm_val} | TEMP: {temp_val} | LITERS: {fuel_liters_val} | RANGE: {fuel_range_val}")

                            # --- ECU wire voltages (AV2–AV13) ---
                            wire_states    = [False] * 20
                            wire_voltages  = [0.0]   * 20

                            for part in parts[5:]:
                                if not part.startswith("AV"):
                                    continue
                                label, _, raw = part.partition(':')
                                pin_num = int(label[2:])          # "AV12" → 12
                                raw_v   = float(raw)
                                real_v  = round(raw_v * VOLTAGE_SCALE, 2)
                                idx     = ANALOG_TO_WIRE.get(pin_num)
                                if idx is not None:
                                    wire_voltages[idx] = real_v
                                    wire_states[idx]   = real_v >= ACTIVE_THRESHOLD

                            with open(WIRE_STATE_FILE, 'w') as jf:
                                json.dump({'states': wire_states, 'voltages': wire_voltages}, jf)

                        except (ValueError, IndexError) as e:
                            print(f"⚠️ Malformed data received: {line} ({e})")

                # --- PART B: LPT LOGIC (Pulse for Odometer) ---
                # This keeps your 12.3km tripmeter moving while you drive
                if lpt:
                    lpt.setData(0x01) # Set Pin 2 High (Arduino sees pulse)
                    time.sleep(0.01)  # Tiny pulse width
                    lpt.setData(0x00) # Reset

                # Small delay to prevent CPU pegging
                time.sleep(0.05)

    except KeyboardInterrupt:
        print("\n🛑 Shutting down. Cleaning up...")
        if lpt: lpt.setData(0x00)
    except Exception as e:
        print(f"❌ Connection lost: {e}")

if __name__ == "__main__":
    PORT = "/dev/ttyACM0"

    unblock_serial_port(PORT)
    print("🚀 Starting normal operations...")

    while True:
        try:
            run_master_bridge()
        except KeyboardInterrupt:
            print("\n🛑 Shutting down.")
            break
        except Exception as e:
            print(f"🔄 Bridge dropped ({e}), reconnecting in 3 s...")
            time.sleep(3)
    
