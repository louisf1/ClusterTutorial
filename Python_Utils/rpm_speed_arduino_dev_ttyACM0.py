import serial
import time
import subprocess
import os
import json

from kuksa_client.grpc import VSSClient, Datapoint

# --- CONFIGURATION ---
SERIAL_PORT = '/dev/ttyACM0'
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
VOLTAGE_SCALE     = 1.0   # no voltage divider — direct 10kΩ series resistor only
ACTIVE_THRESHOLD  = 2.0   # volts (real, after scale) — wire considered "live"
WIRE_STATE_FILE   = '/tmp/ecu_wire_states.json'

def run_master_bridge():
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
        time.sleep(2)  # Give Arduino a moment to reboot after connection
        print(f"✅ Arduino Serial Link Active on {SERIAL_PORT}")
    except Exception as e:
        print(f"❌ Serial Error: {e}. Check your USB connection!")
        return

    try:
        with VSSClient(KUKSA_IP, KUKSA_PORT) as client:
            print("🚀 Kuksa Link Active. Dashboard is now LIVE.")

            while True:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue

                # Format: SPD:xx|RPM:xx|TMP:xx|LITERS:xx|RANGE:xx|AV2:x.xx|...|AV13:x.xx
                if "SPD:" not in line or "|RPM:" not in line:
                    continue

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

                    # Indicators/hazard/lights — latched 1s window on Arduino
                    left_indicator  = False
                    right_indicator = False
                    hazard          = False
                    lights_dim      = False
                    lights_bright   = False
                    for part in parts[5:]:
                        if part.startswith("LINDIC:"):
                            left_indicator  = part.split(':')[1] == '1'
                        elif part.startswith("RINDIC:"):
                            right_indicator = part.split(':')[1] == '1'
                        elif part.startswith("HAZARD:"):
                            hazard          = part.split(':')[1] == '1'
                        elif part.startswith("LIGHTSDIM:"):
                            lights_dim      = part.split(':')[1] == '1'
                        elif part.startswith("LIGHTSBRIGHT:"):
                            lights_bright   = part.split(':')[1] == '1'

                    telemetry = {
                       'Vehicle.Speed':                                               Datapoint(speed_val),
                       'Vehicle.OBD.EngineSpeed':                                     Datapoint(rpm_val),
                       'Vehicle.OBD.FuelLevel':                                       Datapoint(fuel_liters_val),
                       'Vehicle.Powertrain.FuelSystem.Range':                         Datapoint(fuel_range_val),
                       'Vehicle.Powertrain.FuelSystem.IsFuelLevelLow':                Datapoint(IsFuelLevelLow),
                       'Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling':     Datapoint(left_indicator),
                       'Vehicle.Body.Lights.DirectionIndicator.Right.IsSignaling':    Datapoint(right_indicator),
                       'Vehicle.Body.Lights.Hazard.IsSignaling':                      Datapoint(hazard),
                       'Vehicle.Body.Lights.Beam.Low.IsOn':                           Datapoint(lights_dim),
                       'Vehicle.Body.Lights.Beam.High.IsOn':                          Datapoint(lights_bright),
                    }
                    # -100 means thermocouple disconnected — skip temp publish
                    if temp_val != -100.0:
                        telemetry['Vehicle.OBD.CoolantTemperature'] = Datapoint(temp_val)
                    client.set_current_values(telemetry)
                    print(f"TELEMETRY -> Speed: {speed_val} km/h | RPM: {rpm_val} | TEMP: {temp_val} | LITERS: {fuel_liters_val} | RANGE: {fuel_range_val} | L:{int(left_indicator)} R:{int(right_indicator)} H:{int(hazard)} DIM:{int(lights_dim)} BRIGHT:{int(lights_bright)}")

                    # ECU wire voltages (AV2–AV13)
                    wire_states   = [False] * 20
                    wire_voltages = [0.0]   * 20

                    for part in parts[5:]:
                        if not part.startswith("AV"):
                            continue
                        label, _, raw = part.partition(':')
                        pin_num = int(label[2:])
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

    except KeyboardInterrupt:
        print("\n🛑 Shutting down.")
    except Exception as e:
        print(f"❌ Connection lost: {e}")

if __name__ == "__main__":
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
