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
    1:   0,  # A1  Blue/Pink     → index  0 "blue-pink"     (Crankshaft Position Sensor)
    2:   1,  # A2  Blue/Black    → index  1 "blue-black"    (Timer Control Valve)
    3:   2,  # A3  White/Blue    → index  2 "white-blue"    (PCM internal control)
    4:   3,  # A4  Green/White   → index  3 "green-white"   (Idle control switch)
    5:   4,  # A5  Red/Black      → index  4 "red-black"     (unknown - to be identified)
    6:   5,  # A6  Red/Blue      → index  5 "red-blue"      (Control Lever Position)
    7:   6,  # A7  Brown/Red     → index  6 "brown-red"     (unknown)
    8:   7,  # A8  Brown/Yellow  → index  7 "brown-yellow"  (unknown)
    9:   8,  # A9  Brown/White   → index  8 "brown-white"   (Control Lever Position)
    10:  9,  # A10 Yellow        → index  9 "yellow"        (EGR solenoid Valve)
    11: 10,  # A11 Green         → index 10 "green"         (FICD solenoid Valve no1)
    12: 11,  # A12 Pink          → index 11 "pink"          (FICD solenoid Valve no2)
    13: 12,  # A13 Black/Yellow  → index 12 "black-yellow"  (Starter Signal)
}
DIGITAL_TO_WIRE = {
    'DVIGNITIONPWR':  13,  # D14 Black/White → index 13 "black-white" (ignition power)
    'DVSTANDBYPOWER': 14,  # D35 red         → index 14 "red" (standby power)
    'DVGLOWPLUG':     15,  # D11 Red         → index 15 "red" (Glow Plugs)
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

            # Clear any stale values left in the broker from a previous run
            client.set_current_values({
                'Vehicle.Speed':                                               Datapoint(0.0),
                'Vehicle.OBD.EngineSpeed':                                     Datapoint(0.0),
                'Vehicle.OBD.FuelLevel':                                       Datapoint(0.0),
                'Vehicle.OBD.CoolantTemperature':                              Datapoint(0.0),
                'Vehicle.Powertrain.FuelSystem.Range':                         Datapoint(0.0),
                'Vehicle.Powertrain.FuelSystem.IsFuelLevelLow':                Datapoint(False),
                'Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling':     Datapoint(False),
                'Vehicle.Body.Lights.DirectionIndicator.Right.IsSignaling':    Datapoint(False),
                'Vehicle.Body.Lights.Hazard.IsSignaling':                      Datapoint(False),
                'Vehicle.Body.Lights.Beam.Low.IsOn':                           Datapoint(False),
                'Vehicle.Body.Lights.Beam.High.IsOn':                          Datapoint(False),
                'Vehicle.Body.Lights.Running.IsOn':                            Datapoint(False),
                'Vehicle.Powertrain.IsAutoPowerOptimize':                      Datapoint(False),
                'Vehicle.Cabin.HVAC.IsAirConditioningActive':                  Datapoint(False),
                'Vehicle.ADAS.DMS.IsError':                                    Datapoint(False),
                'Vehicle.Body.Lights.Parking.IsOn':                            Datapoint(False),
            })
            with open(WIRE_STATE_FILE, 'w') as jf:
                json.dump({'states': [False] * 20, 'voltages': [0.0] * 20}, jf)
            print("🧹 Stale KUKSA values cleared.")

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

                    # Indicators/lights — latched 1s window on Arduino
                    # Hazard derived here: both left AND right active = hazard
                    left_indicator  = False
                    right_indicator = False
                    lights_dim      = False
                    lights_bright   = False
                    daytime_lights  = False
                    parking_lights  = False
                    glow_plug_relay = False
                    ac_active       = False
                    for part in parts[5:]:
                        if part.startswith("LINDIC:"):
                            left_indicator  = part.split(':')[1] == '1'
                        elif part.startswith("RINDIC:"):
                            right_indicator = part.split(':')[1] == '1'
                        elif part.startswith("LIGHTSDIM:"):
                            lights_dim      = part.split(':')[1] == '1'
                        elif part.startswith("LIGHTSBRIGHT:"):
                            lights_bright   = part.split(':')[1] == '1'
                        elif part.startswith("DAYTIMEDRIVING:"):
                            daytime_lights  = part.split(':')[1] == '1'
                        elif part.startswith("PARKINGLIGHTS:"):
                            parking_lights  = part.split(':')[1] == '1'
                        elif part.startswith("DVGLOWPLUG:"):
                            glow_plug_relay = part.split(':')[1] == '1'
                        elif part.startswith("DVIGNITIONPWR:"):
                            ignition_power = part.split(':')[1] == '1'
                        elif part.startswith("DVSTANDBYPOWER:"):
                            stand_by_power = part.split(':')[1] == '1'
                        elif part.startswith("DVAIRCONSIGNAL:"):
                            ac_active = part.split(':')[1] == '1'                                                                                    

                    hazard = left_indicator and right_indicator

                    # ECU wire voltages (AV1–AV13 analog, DVIGNITIONPWR/DVSTANDBYPOWER/DVGLOWPLUG digital)
                    wire_states   = [False] * 20
                    wire_voltages = [0.0]   * 20

                    for part in parts[5:]:
                        if part.startswith("AV"):
                            label, _, raw = part.partition(':')
                            pin_num = int(label[2:])
                            real_v  = round(float(raw) * VOLTAGE_SCALE, 2)
                            idx     = ANALOG_TO_WIRE.get(pin_num)
                            if idx is not None:
                                wire_voltages[idx] = real_v
                                wire_states[idx]   = real_v >= ACTIVE_THRESHOLD
                        elif part.startswith("DV"):
                            label, _, raw = part.partition(':')
                            is_high = raw.strip() == '1'
                            idx     = DIGITAL_TO_WIRE.get(label)
                            if idx is not None:
                                wire_voltages[idx] = 12.0 if is_high else 0.0
                                wire_states[idx]   = is_high

                    with open(WIRE_STATE_FILE, 'w') as jf:
                        json.dump({'states': wire_states, 'voltages': wire_voltages}, jf)

                    # ECU STATUS screen shows wire indices 0-14 (AV1-AV13 + ignition/standby power).
                    # If any of those reads 0V, light the ECU warning icon.
                    ecu_error = any(v <= 0.01 for v in wire_voltages[:15])

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
                       'Vehicle.Body.Lights.Running.IsOn':                            Datapoint(daytime_lights),
                       'Vehicle.Powertrain.IsAutoPowerOptimize':                      Datapoint(glow_plug_relay),
                       'Vehicle.Cabin.HVAC.IsAirConditioningActive':                  Datapoint(ac_active),
                       'Vehicle.ADAS.DMS.IsError':                                    Datapoint(ecu_error),
                       'Vehicle.Body.Lights.Parking.IsOn':                            Datapoint(parking_lights),
                    }
                    # -100 means thermocouple disconnected — skip temp publish
                    if temp_val != -100.0:
                        telemetry['Vehicle.OBD.CoolantTemperature'] = Datapoint(temp_val)
                    client.set_current_values(telemetry)
                    print(f"TELEMETRY -> Speed: {speed_val} km/h | RPM: {rpm_val} | TEMP: {temp_val} | LITERS: {fuel_liters_val} | RANGE: {fuel_range_val} | L:{int(left_indicator)} R:{int(right_indicator)} H:{int(hazard)} DIM:{int(lights_dim)} BRIGHT:{int(lights_bright)} DT:{int(daytime_lights)} PARK:{int(parking_lights)} GLOWPLUGS:{int(glow_plug_relay)} AC:{int(ac_active)} ECUERR:{int(ecu_error)}")

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
