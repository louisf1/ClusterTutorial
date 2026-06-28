import serial
import time
import subprocess
import os
import json
import csv
import signal
import sys
from datetime import datetime

from kuksa_client.grpc import VSSClient, Datapoint

# --- CONFIGURATION ---
BAUD_RATE = 9600
SERIAL_PORTS = ['/dev/ttyACM0', '/dev/ttyACM1']
KUKSA_IP = '127.0.0.1'
KUKSA_PORT = 55555

# ECU wire harness — maps Arduino analog pin number to wire index (0-19)
# used by the dashboard health page (matches getWireFunction order in App.qml)
ANALOG_TO_WIRE = {
    1:   0,  # A1  Blue/Pink     → index  0 "blue-pink"     (Crankshaft Position Sensor)
    2:   1,  # A2  Blue/Black    → index  1 "blue-black"    (Timer Control Valve)
    3:   2,  # A3  Brown         → index  2 "brown"         (Glow Plug Relay Swicth)
    4:   3,  # A4  Green/White   → index  3 "green-white"   (Idle control switch)
    5:   4,  # A5  Red/Black     → index  4 "red-black"     (unknown - to be identified)
    6:   5,  # A6  Red/Blue      → index  5 "red-blue"      (Control Lever Position)
    7:   6,  # A7  Brown/Red     → index  6 "brown-red"     (unknown)
    8:   7,  # A8  Brown/Yellow  → index  7 "brown-yellow"  (unknown)
    9:   8,  # A9  Brown/White   → index  8 "brown-white"   (Control Lever Position)
    10:  9,  # A10 Yellow        → index  9 "yellow"        (EGR solenoid Valve)
    11: 10,  # A11 Green         → index 10 "green"         (FICD solenoid Valve no1)
    12: 11,  # A12 Pink          → index 11 "pink"          (FICD solenoid Valve no2)
    13: 12,  # A13 Black/Yellow  → index 12 "black-yellow"  (Starter Signal)
    14: 13,  # A14 Black/White   - index 13 "Black/white"   (ECU Power on)
    15: 14,  # A15 Red           - index 14 "Red"           (StandBy Power)
}
DIGITAL_TO_WIRE = {
    'DVGLOWPLUG':     15,  # D11 Brown → index 15 "brown" (Glow Plugs)
    # DVIGNITIONPWR/DVSTANDBYPOWER removed — now read as AV14/AV15 analog (ANALOG_TO_WIRE 14→13, 15→14)
}
VOLTAGE_SCALE     = 1.0   # Arduino applies 10kΩ/5.1kΩ divider scale; value sent is already in automotive volts (0–14.5V)
ACTIVE_THRESHOLD  = 2.0   # volts (real, after scale) — wire considered "live"
WIRE_STATE_FILE   = '/tmp/ecu_wire_states.json'
FAST_STATE_FILE   = '/tmp/ecu_fast_state.json'

def _zero_wire_states_and_exit(signum, frame):
    with open(WIRE_STATE_FILE, 'w') as jf:
        json.dump({'states': [False] * 20, 'voltages': [0.0] * 20}, jf)
    print("🔌 Wire states zeroed — SIGTERM received.")
    os._exit(0)  # bypass context managers that may swallow SystemExit

signal.signal(signal.SIGTERM, _zero_wire_states_and_exit)

# --- KPL (km per litre) rolling log ---
KPL_LOG_FILE    = '/tmp/kpl_log.csv'   # timestamp, liters, odo_km
KPL_MAX_RECORDS = 6                    # 6 × 10 min = 1 hour sliding window
KPL_INTERVAL    = 600                  # seconds between log entries
VSS_BUILD_FILE  = '/home/louis/ClusterWorx/build/VSS_paths.txt'

def _read_persisted_odo():
    """Read totalKms persisted in the 4th pipe field of VSS_paths.txt."""
    try:
        with open(VSS_BUILD_FILE) as f:
            for line in f:
                if 'TraveledDistanceSinceStart' in line and not line.startswith('#'):
                    parts = line.strip().split('|')
                    if len(parts) >= 4 and parts[3]:
                        return float(parts[3])
    except Exception:
        pass
    return 0.0

def _load_kpl_records():
    records = []
    try:
        with open(KPL_LOG_FILE, newline='') as f:
            for row in csv.reader(f):
                if len(row) == 3:
                    records.append((row[0], float(row[1]), float(row[2])))
    except FileNotFoundError:
        pass
    return records

def _save_kpl_records(records):
    with open(KPL_LOG_FILE, 'w', newline='') as f:
        w = csv.writer(f)
        for r in records:
            w.writerow(r)

def _calc_kpl(records):
    """KPL from first→last record: km driven / litres consumed."""
    if len(records) < 2:
        return 0.0
    fuel_used = records[0][1] - records[-1][1]   # litres consumed (positive)
    km_driven = records[-1][2] - records[0][2]   # km gained
    if fuel_used <= 0 or km_driven <= 0:
        return 0.0
    return round(km_driven / fuel_used, 2)

def _init_kpl_log_if_missing():
    """Seed the log with 6 dummy records so the file exists from day one."""
    if os.path.exists(KPL_LOG_FILE):
        return
    now = time.time()
    records = []
    for i in range(KPL_MAX_RECORDS):
        t   = now - (KPL_MAX_RECORDS - 1 - i) * KPL_INTERVAL
        ts  = datetime.fromtimestamp(t).strftime('%Y-%m-%d %H:%M:%S')
        lit = round(65.0 - i * 1.0, 2)        # 65→60 L  (1 L / 10 min)
        odo = round(160000.0 + i * 10.0, 1)   # 160000→160050 km (10 km / 10 min)
        records.append((ts, lit, odo))
    _save_kpl_records(records)
    print(f"KPL log seeded with {KPL_MAX_RECORDS} dummy records → {KPL_LOG_FILE}")

def update_kpl_log(fuel_liters):
    """Append entry, trim to KPL_MAX_RECORDS, return current KPL average."""
    odo     = _read_persisted_odo()
    records = _load_kpl_records()
    ts      = datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    records.append((ts, round(fuel_liters, 2), round(odo, 1)))
    if len(records) > KPL_MAX_RECORDS:
        records = records[-KPL_MAX_RECORDS:]
    _save_kpl_records(records)
    return _calc_kpl(records)


def run_master_bridge():
    # Zero wire states immediately — if bridge restarts or is searching for Arduino,
    # the dashboard should show all red rather than stale values
    with open(WIRE_STATE_FILE, 'w') as jf:
        json.dump({'states': [False] * 20, 'voltages': [0.0] * 20}, jf)

    _init_kpl_log_if_missing()
    last_kpl_time = 0   # log first entry immediately on first valid telemetry

    ser = None
    for port in SERIAL_PORTS:
        try:
            ser = serial.Serial(port, BAUD_RATE, timeout=0.1)
            time.sleep(2)  # Give Arduino a moment to reboot after connection
            print(f"✅ Arduino Serial Link Active on {port}")
            break
        except Exception as e:
            print(f"⚠️  Could not open {port}: {e}")
    if ser is None:
        print(f"❌ Serial Error: no Arduino found on {SERIAL_PORTS}. Check your USB connection!")
        with open(WIRE_STATE_FILE, 'w') as jf:
            json.dump({'states': [False] * 20, 'voltages': [0.0] * 20}, jf)
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
                'Vehicle.Powertrain.CombustionEngine.EngineOil.Level':        Datapoint("NORMAL"),
                'Vehicle.Cabin.Door.Row1.DriverSide.IsOpen':                  Datapoint(False),
                'Vehicle.Cabin.Door.Row1.PassengerSide.IsOpen':               Datapoint(False),
                'Vehicle.Cabin.Door.Row2.DriverSide.IsOpen':                  Datapoint(False),
                'Vehicle.Cabin.Door.Row2.PassengerSide.IsOpen':               Datapoint(False),
            })
            with open(WIRE_STATE_FILE, 'w') as jf:
                json.dump({'states': [False] * 20, 'voltages': [0.0] * 20}, jf)
            with open(FAST_STATE_FILE, 'w') as jf:
                json.dump({'pageButton': False, 'tripReset': False}, jf)
            print("🧹 Stale KUKSA values cleared.")

            last_trip_reset  = False
            last_page_button = False

            # Debounce for ECU wire states — require DEBOUNCE_COUNT consecutive
            # readings above/below ACTIVE_THRESHOLD before changing confirmed state.
            # Prevents floating/noisy pins from showing false greens.
            DEBOUNCE_COUNT      = 3
            wire_above_count    = [0] * 20   # consecutive readings above threshold
            wire_below_count    = [0] * 20   # consecutive readings below threshold
            wire_confirmed      = [False] * 20  # debounced state written to JSON

            while True:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                if not line:
                    continue

                # FAST line — page button + indicators, sent every 100ms,
                # independent of the 1s telemetry line below.
                if line.startswith("FAST|"):
                    page_button     = False
                    left_indicator  = False
                    right_indicator = False
                    for part in line.split('|')[1:]:
                        if part.startswith("PAGEBTN:"):
                            page_button     = part.split(':')[1] == '1'
                        elif part.startswith("LINDIC:"):
                            left_indicator  = part.split(':')[1] == '1'
                        elif part.startswith("RINDIC:"):
                            right_indicator = part.split(':')[1] == '1'

                    client.set_current_values({
                        'Vehicle.Body.Lights.DirectionIndicator.Left.IsSignaling':  Datapoint(left_indicator),
                        'Vehicle.Body.Lights.DirectionIndicator.Right.IsSignaling': Datapoint(right_indicator),
                        'Vehicle.Body.Lights.Hazard.IsSignaling':                   Datapoint(left_indicator and right_indicator),
                    })
                    last_page_button = page_button
                    with open(FAST_STATE_FILE, 'w') as jf:
                        json.dump({'pageButton': page_button, 'tripReset': last_trip_reset}, jf)
                    continue

                # Format: SPD:xx|RPM:xx|TMP:xx|LITERS:xx|RANGE:xx|AV1:x.xx|...|AV15:x.xx
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
                    parking_brake   = False
                    seatbelt        = False
                    glow_plug_relay = False
                    ac_active       = False
                    oil_signal      = True   # default: oil normal until read
                    door_open       = False
                    trip_reset      = False
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
                        elif part.startswith("PARKINGBRAKE:"):
                            parking_brake   = part.split(':')[1] == '1'
                        elif part.startswith("SEATBELT:"):
                            seatbelt        = part.split(':')[1] == '1'
                        elif part.startswith("DVGLOWPLUG:"):
                            glow_plug_relay = part.split(':')[1] == '1'
                        elif part.startswith("DVAIRCONSIGNAL:"):
                            ac_active = part.split(':')[1] == '1'
                        elif part.startswith("DVDOORSIGNAL:"):
                            door_open = part.split(':')[1] == '1'   # 1=HIGH=door open, 0=closed
                        elif part.startswith("DVOILSIGNAL:"):
                            oil_signal = part.split(':')[1] == '1'  # 1=HIGH=normal, 0=LOW=low
                        elif part.startswith("SPARE7:"):
                            trip_reset = part.split(':')[1] == '1'

                    last_trip_reset = trip_reset
                    with open(FAST_STATE_FILE, 'w') as jf:
                        json.dump({'pageButton': last_page_button, 'tripReset': trip_reset}, jf)

                    hazard = left_indicator and right_indicator

                    # ECU wire voltages (AV1–AV15 analog; AV14=ignition power, AV15=standby power)
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

                    # Apply debounce: only flip confirmed state after DEBOUNCE_COUNT
                    # consecutive readings on the same side of ACTIVE_THRESHOLD.
                    for i in range(20):
                        if wire_states[i]:
                            wire_above_count[i] += 1
                            wire_below_count[i]  = 0
                            if wire_above_count[i] >= DEBOUNCE_COUNT:
                                wire_confirmed[i] = True
                        else:
                            wire_below_count[i] += 1
                            wire_above_count[i]  = 0
                            if wire_below_count[i] >= DEBOUNCE_COUNT:
                                wire_confirmed[i] = False

                    with open(WIRE_STATE_FILE, 'w') as jf:
                        json.dump({'states': wire_confirmed, 'voltages': wire_voltages}, jf)

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
                       'Vehicle.Chassis.ParkingBrake.IsEngaged':                      Datapoint(parking_brake),
                       'Vehicle.Cabin.Seat.Row1.DriverSide.IsBelted':                 Datapoint(seatbelt),
                       'Vehicle.Cabin.Seat.Row1.Middle.IsBelted':                     Datapoint(seatbelt),
                       'Vehicle.Cabin.Seat.Row1.PassengerSide.IsBelted':              Datapoint(seatbelt),
                       'Vehicle.Cabin.Seat.Row2.DriverSide.IsBelted':                 Datapoint(seatbelt),
                       'Vehicle.Cabin.Seat.Row2.Middle.IsBelted':                     Datapoint(seatbelt),
                       'Vehicle.Cabin.Seat.Row2.PassengerSide.IsBelted':              Datapoint(seatbelt),
                       'Vehicle.Powertrain.CombustionEngine.EngineOil.Level':         Datapoint("NORMAL" if oil_signal else "LOW"),
                       'Vehicle.Cabin.Door.Row1.DriverSide.IsOpen':                   Datapoint(door_open),
                       'Vehicle.Cabin.Door.Row1.PassengerSide.IsOpen':                Datapoint(door_open),
                       'Vehicle.Cabin.Door.Row2.DriverSide.IsOpen':                   Datapoint(door_open),
                       'Vehicle.Cabin.Door.Row2.PassengerSide.IsOpen':                Datapoint(door_open),
                    }
                    # -100 means thermocouple disconnected — skip temp publish
                    if temp_val != -100.0:
                        telemetry['Vehicle.OBD.CoolantTemperature'] = Datapoint(temp_val)
                    client.set_current_values(telemetry)

                    # KPL rolling log — update every 10 minutes
                    now = time.time()
                    if now - last_kpl_time >= KPL_INTERVAL:
                        kpl = update_kpl_log(fuel_liters_val)
                        last_kpl_time = now
                        if kpl > 0:
                            client.set_current_values({
                                'Vehicle.Powertrain.FuelSystem.AverageConsumption':
                                    Datapoint(float(kpl))
                            })
                        print(f"KPL log entry written → {kpl} km/L  (file: {KPL_LOG_FILE})")
                    print(f"TELEMETRY -> Speed: {speed_val} km/h | RPM: {rpm_val} | TEMP: {temp_val} | LITERS: {fuel_liters_val} | RANGE: {fuel_range_val} | L:{int(left_indicator)} R:{int(right_indicator)} H:{int(hazard)} DIM:{int(lights_dim)} BRIGHT:{int(lights_bright)} DT:{int(daytime_lights)} PARK:{int(parking_lights)} PBRAKE:{int(parking_brake)} SEATBELT:{int(seatbelt)} GLOWPLUGS:{int(glow_plug_relay)} AC:{int(ac_active)} ECUERR:{int(ecu_error)} TRIPRESET:{int(trip_reset)} OIL:{'normal' if oil_signal else 'low'} DOOR:{'open' if door_open else 'closed'}")

                except (ValueError, IndexError) as e:
                    print(f"⚠️ Malformed data received: {line} ({e})")

    except KeyboardInterrupt:
        print("\n🛑 Shutting down.")
    except Exception as e:
        print(f"❌ Connection lost: {e}")
    finally:
        # Zero out wire states so ECU STATUS shows all red when bridge is not running
        with open(WIRE_STATE_FILE, 'w') as jf:
            json.dump({'states': [False] * 20, 'voltages': [0.0] * 20}, jf)
        if ser and ser.is_open:
            ser.close()
        print("🔌 Wire states zeroed — bridge exiting.")

if __name__ == "__main__":
    print("🚀 Starting normal operations...")

    while True:
        try:
            run_master_bridge()
        except KeyboardInterrupt:
            print("\n🛑 Shutting down.")
            break
        except SystemExit:
            break
        except Exception as e:
            print(f"🔄 Bridge dropped ({e}), reconnecting in 3 s...")
            time.sleep(3)
        else:
            # run_master_bridge returned normally (no Arduino found) — wait before retry
            time.sleep(3)
