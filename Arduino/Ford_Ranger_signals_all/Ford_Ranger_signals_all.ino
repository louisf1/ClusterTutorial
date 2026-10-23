// =============================================================================
// Ford Ranger ECU Harness — Arduino Mega 2560 Telemetry
// =============================================================================
// Serial output format (one line per second):
//   SPD:xx.x|RPM:xxxx|TMP:xx.x|LITERS:xx.x|RANGE:xxx
//   |AV2:x.xx|AV3:x.xx|AV4:x.xx|AV5:x.xx|AV6:x.xx
//   |AV8:x.xx|AV9:x.xx|AV10:x.xx|AV11:x.xx|AV12:x.xx|AV13:x.xx
//   |NATIVETEMPV:x.xx (volts, PCM-driven) |NATIVETEMP:xx.x (°C, A7 coolant sender — see below)
//   |LINDIC:x|RINDIC:x  (1=active, 0=off — true within INDICATOR_TIMEOUT_MS of last pulse)
//   |PAGEBTN:x  (1=pressed, 0=not — latched over the 1s window; D19 button to GND)
//   |TRIPRESET:x  (1=pressed, 0=not — latched over the 1s window; D20 button to GND)
//
// A separate FAST line is sent every 100ms, independent of the 1s report:
//   FAST|PAGEBTN:x|LINDIC:x|RINDIC:x
// (PAGEBTN latched over the 100ms window; LINDIC/RINDIC as above)
//
// AV values are 0–5V. Each wire uses a 10kΩ/5.1k volts devider
// Signals above 5V clamp at the protection diode.
// Python bridge uses VOLTAGE_SCALE=1.0 (no scaling needed).
// =============================================================================

// =============================================================================
// INDICATORS — D25 (left) / D26 (right) via 10k ohm/ 5.1 ohm divider
// LIGHTS     — D27 (dim/parking) via 10k ohm/ 5.1 ohm divider
// All active HIGH; external 10kΩ pull-down holds LOW when off.
// Hazard is derived in Python bridge (left AND right both active).
// State is latched across the 1s report window so fast blinks are never missed.
// =============================================================================
const int leftIndicatorPin  = 7;
const int rightIndicatorPin = 5;
const int LightsDimPin      = 9;
const int LightsBrightPin   = 8;
const int DaytimeLightsPin  = 10;
const int ParkingLightsPin  = 14;
const int ParkingBrakePin   = 16;
const int SeatbeltPin       = 19;

// Left/right indicators use a "last pulse seen" timestamp instead of a 1s
// latch — the flasher relay pulses the pin at ~1.5Hz, so "active" is true
// for INDICATOR_TIMEOUT_MS after the most recent pulse. This lets the FAST
// report below reflect indicator on/off within ~100ms instead of up to 2s.
unsigned long lastLeftPulseTime  = 0;
unsigned long lastRightPulseTime = 0;
const unsigned long INDICATOR_TIMEOUT_MS = 700; // > one flasher half-period

bool LightsDimSeen      = false;
bool LightsBrightSeen   = false;
bool DaytimeLightsSeen  = false;
bool ParkingLightsSeen  = false;
bool ParkingBrakeSeen   = false;
bool SeatbeltSeen       = false;

// Spare pins — held LOW (pulled down) by default, briefly switched to
// INPUT each loop to sample for a HIGH pulse, thuen driven LOW again.
// D1 excluded (Serial0 TX, controlled by USART hardware).
// D33 excluded (confirmed dead — stuck at 4.7V even when driven OUTPUT LOW).
// D2-D3 were glow the original spare set; D6,7,8,9,12,13,32,38 were previously
// "reserved" INPUT_PULLUP wire pins that float high with nothing connected —
// moved here so they sit at a known 0V instead.
// D10 removed from this set — now the dedicated parking brake switch input.
// D8 removed from this set — now LightsBrightPin. Leaving it here drove the
// pin OUTPUT LOW every loop, so the bright-lights read was always 0.
// D9 removed from this set — now the dedicated dashboard page-switch button.
// D34 promoted to OilLevelPin (oil level sensor INPUT_PULLUP).
// D35 promoted to DoorPin (door open sensor — active HIGH, 5V = any door open).
const int NUM_SPARE_PINS = 6;
const int SparePins[NUM_SPARE_PINS] = {2, 11, 12, 13, 32, 38};
bool SparePinSeen[NUM_SPARE_PINS]   = {false, false, false,
                                       false, false, false};

// =============================================================================
// DOOR OPEN SIGNAL — D17
// Active HIGH: 5V = any door open, 0V = all doors closed.
// Plain INPUT — signal driven externally; no pull-up needed.
// =============================================================================
const int DoorPin = 17;

// =============================================================================
// OIL LEVEL SIGNAL — D18
// Oil level sensor: float switch, closes to GND when oil is LOW.
// INPUT_PULLUP: HIGH = oil normal, LOW = oil low.
// =============================================================================
const int OilLevelPin = 18;

// =============================================================================
// DASHBOARD PAGE-SWITCH BUTTON — D30
// Momentary pushbutton to GND, INPUT_PULLUP (active LOW when pressed).
// Replaces the old spacebar shortcut for cycling dashboard screens.
// PageButtonSeen is latched over the 1s window (main report);
// PageButtonFastSeen is latched over the 100ms FAST report window.
// =============================================================================
const int PageButtonPin = 30;
bool PageButtonSeen     = false;
bool PageButtonFastSeen = false;

// =============================================================================
// TRIP RESET BUTTON — D31
// Momentary pushbutton to GND, INPUT_PULLUP (active LOW when pressed).
// Moved off the SPARE7/D7 spare-pin slot (now reused for the glow plug
// dual-mode read) onto its own dedicated pin. Latched over the 1s window;
// SystemMonitor.cpp edge-detects the rising TRIPRESET flag on the dashboard
// side to fire the actual reset once per press.
// =============================================================================
const int TripResetPin = 31;
bool TripResetSeen      = false;

// =============================================================================
// GLOW PLUG & AIRCON DASH INDICATORS — D20 / D21
// Active HIGH, plain INPUT, read once per 1s report (DVGLOWPLUG/DVAIRCONSIGNAL).
// NOTE: D20/D21 are the Mega's I2C SDA/SCL pins, which have 10kΩ pull-ups to
// 5V on the board — an unconnected pin reads HIGH (= active), and a signal must
// pull the pin below ~1.5V against that pull-up to read LOW.
// =============================================================================
const int GlowPlugPin = 20;
const int AirconPin   = 21;

// DISABLED: D0/D1 are the Mega's Serial RX/TX to the Pi (USB link), so they
// can't be used as signal inputs. Dual-mode read commented out until the
// aircon/glow plug signals are moved to free pins.
// D1 (aircon) and D0 (glow plug) — keep their existing INPUT_PULLUP signal
// reads (DVAIRCONSIGNAL/DVGLOWPLUG) AND, each loop, briefly drive the pin LOW
// then switch back to INPUT_PULLUP to see if an external signal pulls it back
// HIGH on its own. Latched as SPARE5/SPARE11 for comparison/refinement.
//const int DualModePins[2] = {0, 1};
//bool DualModePinSeen[2]   = {false, false};

// =============================================================================
// SPEED & RPM
// =============================================================================
const int speedPin = 24;
const int rpmPin   = 23;

const float wheelCircumference = 2.8274;  // 900mm wheel (π × 0.9m)
const unsigned long debounceTime = 15;    // ms — ignores bounce/noise

volatile unsigned long speedPulses   = 0;
volatile unsigned long rpmPulses     = 0;
volatile unsigned long lastSpeedTime = 0;
volatile unsigned long lastRpmTime   = 0;
bool lastSpeedState = false;
bool lastRpmState   = false;

// =============================================================================
// TEMPERATURE — MAX6675 bit-bang SPI
// NOTE: tempSCK (D47) and tempSO (D46) have been moved away from D12/D13
//       to free D12 and D13 for the ECU wire digital functions below.
// =============================================================================
const int tempCS  = 50;   // Chip select   — any free digital pin
const int tempSO  = 46;   // SPI MISO      — moved from D12
const int tempSCK = 47;   // SPI clock     — moved from D13

// =============================================================================
// ALTERNATOR EXCITATION CONTROL — Arduino-only, no dashboard comms
// Inputs (alternator output voltage, monitored via 7805VC-protected dividers,
// 2 wires for redundancy):
//   A15 = Red wire        (Standby power, P35)
//   A14 = Black/White wire (Ignition power, P39)
// Output:
//   D44 = Excite signal (PWM -> RC filter -> field driver), 0-1.0V max
//
// Closed loop: every ALT_STEP_INTERVAL_MS, if the monitored alternator
// voltage exceeds ALT_TARGET_MAX_V, the excite voltage is reduced by
// ALT_STEP_V; otherwise it is increased by ALT_STEP_V. Output is clamped
// to [0, ALT_OUT_MAX_V] as a hard safety cap.
// =============================================================================
const int AltMonitorRedPin = A15;
const int AltMonitorBWPin  = A14;
const int AltExcitePin     = 44;

// D40 — PC power-switch-header relay output (BC547 low-side switch), pulsed
// briefly on confirmed ignition-off (sleep) and ignition-on (wake) transitions.
// Debounced against AV14 (Black/White wire, same real-volts scale as the AV14
// telemetry field) so the same 1.5V/5s logic used by the Python bridge's
// software suspend also drives this independent hardware path — this one
// still works even if the bridge/KUKSA/systemd chain is down, and it's the
// only path that can wake the PC once suspended (nothing runs during suspend
// to catch a serial command). See hardware_notes_voltage_dividers.txt and
// [[project_pc_sleep_wake]] memory for the full design.
const int PcPowerPulsePin  = 40;
const float IGNITION_SLEEP_THRESHOLD_V = 1.5;
const int   IGNITION_DEBOUNCE_COUNT    = 5;      // consecutive 1s samples before confirming a transition
const unsigned long PULSE_DURATION_MS  = 400;    // mimics a momentary power-button press

bool ignitionConfirmedOn = true;   // assume ON at boot — Arduino is USB-powered from the PC
int  ignitionAboveCount  = 0;
int  ignitionBelowCount  = 0;
bool pcPulseActive        = false;
unsigned long pcPulseStartTime = 0;

// Calibrated 2026-06-10: 12V applied to red wire -> RAWRED read 0.36V,
// giving scale 12 / 0.36 = 33.33. Assumed same for black-white (separate
// 7805VC unit, same circuit) until verified independently.
const float ALT_MONITOR_SCALE = 33.33;

const float ALT_TARGET_MAX_V = 14.5;
const float ALT_STEP_V       = 0.01;
const float ALT_OUT_MIN_V    = 0.0;
const float ALT_OUT_MAX_V    = 1.0;   // safety cap
const float ALT_OUT_SUPPLY_V = 5.0;   // PWM/RC-filter reference voltage
const unsigned long ALT_STEP_INTERVAL_MS = 10;

float altExciteVoltage   = 0.0;
unsigned long altLastStepTime = 0;

// Latest readings, for debug telemetry only
float altDebugVRed   = 0.0;
float altDebugVBW    = 0.0;
float altDebugRawRed = 0.0;  // raw pin voltage, 0-5V, before ALT_MONITOR_SCALE
float altDebugRawBW  = 0.0;  // raw pin voltage, 0-5V, before ALT_MONITOR_SCALE
// =============================================================================
// FUEL GAUGE — A0
// =============================================================================
const int fuelPin    = A0;
const int numReadings = 20;
int  readings[numReadings];
int  readIndex = 0;
long total     = 0;
int  average   = 0;

const float V_EMPTY       = 1.248;
const float V_FULL        = 0.109;
const float TANK_CAPACITY = 76.03012;
float avgKmPerLiter       = 10.0;     // update to your real-world average

// =============================================================================
// ECU WIRE HARNESS — Analog voltage reads (A1–A13)
// Each wire uses a 10kΩ series resistor (current limiting only, no voltage divider).
// Signals above 5V clamp at the Arduino protection diode — sufficient for
// active/inactive detection. Python bridge uses VOLTAGE_SCALE=1.0.
//	"black-white",    //: ignition power on-off and Timer Control Valve :Arduin
//	"black-yellow",    //: Connected to ignition key start barrel :Arduino AnaLog Pin  13
//	"blue-black",    //: Timer Control Valve :Arduino AnaLog Pin  2
//	"blue-pink",    //: Crankshaft Position Sensor :Arduino AnaLog Pin 1
//	"brown",    //: Glow plug relay :Arduino Analog Pin 3 
//	"brown-red",    //: unknown :Arduino AnaLog Pin  7
//	"brown-white",    //: Control Lever Position :Arduino AnaLog Pin  9
//	"brown-yellow",    //: unknown :Arduino AnaLog Pin  8
//	"green",    //: FICD solenoid Valve no1 :Arduino AnaLog Pin  11
//	"green-white",    //: Idle control switch :Arduino AnaLog Pin  4
//	"pink",    //: FICD solenoid Valve no2 :Arduino AnaLog Pin  12
//	"red",    //: standby power :Arduino Analog pin 15 
//	"red-blue",    //: Control Lever Position :Arduino AnaLog Pin  6
//	"white-blue",    //: PCM internal control :Arduino AnaLog Pin  3
//	"yellow"    //: EGR solonoid Valve :Arduino AnaLog Pin 10
// =============================================================================

// Analog pins — voltage reads only (A14=ignition power, A15=standby power)
// A7 ("brown-red", never identified) is repurposed as the native coolant
// sender input — see NATIVE COOLANT TEMPERATURE SENDER. It's PCM-powered
// (no external pull-up needed) and carries the same 10k/5.1k + TVS + cap
// protection as every other pin here, so it's read through this same loop
// like all the rest (reported as AV7).
const int WIRE_A_PINS[15] = {A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15};

// =============================================================================
// NATIVE COOLANT TEMPERATURE SENDER — A7 (yellow-black wire into the engine
// block; repurposed from the "brown-red" ECU slot, which was never identified
// as a live signal).
//
// This wire is PCM-powered, not an unpowered single-wire dash-gauge sender —
// the PCM supplies its own internal reference voltage and pull-up to the
// sensor and reads the divided voltage back on this same wire (a standard
// 2-wire ECT sensor circuit). We're just passively tapping that PCM-driven
// voltage the same way every other AV1-13 ECU wire is read, through A7's
// existing 10k/5.1k + TVS + cap protection — no external pull-up needed,
// wire connects straight through.
//
// Because the PCM's internal reference voltage and pull-up resistor aren't
// known, the sender's resistance can't be back-calculated the way a
// self-excited sender could — the calibration table below maps the
// *recovered wire voltage* (AV7, already scaled by the standard 10k/5.1k
// divider formula above) directly to temperature instead.
//
// CALIBRATION REQUIRED: the table below is a placeholder, not a verified
// factory curve. Read NATIVETEMPV (volts) off the serial/bridge log at two
// known temperatures — engine cold at ambient, and warmed up to normal
// operating temp (OEM gauge needle at its usual running position) — and
// replace the two points below. Add a third point if the curve is noticeably
// non-linear between them.
//
// Assumed direction: voltage falls as temperature rises (typical for an NTC
// sender behind a PCM pull-up). If your calibration readings go the other
// way, flip the sort order of the table and the comparisons below.
// =============================================================================
struct TempCalPoint { float voltage; float tempC; };
// Placeholder points — replace with real measurements (see comment above).
// Must stay sorted by descending voltage.
const TempCalPoint NativeTempCalTable[] = {
  { 4.0,  20.0 },   // placeholder: cold/ambient
  { 0.5,  90.0 },   // placeholder: warmed up
};
const int NATIVE_TEMP_CAL_POINTS = sizeof(NativeTempCalTable) / sizeof(NativeTempCalTable[0]);

// Converts the AV7 wire voltage to °C via linear interpolation across
// NativeTempCalTable. Returns -100.0 (matches the MAX6675 sentinel) if the
// reading is outside a sane 0-5V range.
float nativeVoltageToTempC(float v) {
  if (v <= 0.0 || v > 5.5) return -100.0;

  if (v >= NativeTempCalTable[0].voltage) return NativeTempCalTable[0].tempC;
  for (int i = 0; i < NATIVE_TEMP_CAL_POINTS - 1; i++) {
    float vHi = NativeTempCalTable[i].voltage;
    float vLo = NativeTempCalTable[i + 1].voltage;
    if (v <= vHi && v >= vLo) {
      float tHi  = NativeTempCalTable[i].tempC;
      float tLo  = NativeTempCalTable[i + 1].tempC;
      float frac = (vHi - v) / (vHi - vLo);
      return tHi + frac * (tLo - tHi);
    }
  }
  return NativeTempCalTable[NATIVE_TEMP_CAL_POINTS - 1].tempC;  // below lowest cal point
}

// NOT USED  Digital pins actively used for ECU wire signals (input_pullup configured in setup)
//const int WIRE_D_PINS[4] = { 6, 12, 14, 35 };

// =============================================================================
// TEMPERATURE READ
// =============================================================================
float readTemperature() {
  uint16_t v;

  digitalWrite(tempCS, LOW);
  delay(1);
  v  = shiftIn(tempSO, tempSCK, MSBFIRST) << 8;
  v |= shiftIn(tempSO, tempSCK, MSBFIRST);
  digitalWrite(tempCS, HIGH);

  if (v & 0x4) {
    return -100.0;   // thermocouple open / disconnected
  }
  return (v >> 3) * 0.25;
}

// =============================================================================
// ALTERNATOR EXCITATION CONTROL — high-rate, runs every loop() iteration
// =============================================================================
void updateAlternatorExcitation() {
  float rawRed = analogRead(AltMonitorRedPin) * (ALT_OUT_SUPPLY_V / 1023.0);
  float rawBW  = analogRead(AltMonitorBWPin)  * (ALT_OUT_SUPPLY_V / 1023.0);

  float vRed = rawRed * ALT_MONITOR_SCALE;
  float vBW  = rawBW  * ALT_MONITOR_SCALE;

  // Redundant readings — use the higher of the two (clamp early, err safe)
  float vAlternator = max(vRed, vBW);

  altDebugVRed    = vRed;
  altDebugVBW     = vBW;
  altDebugRawRed  = rawRed;
  altDebugRawBW   = rawBW;

  unsigned long now = millis();
  if (now - altLastStepTime < ALT_STEP_INTERVAL_MS) return;
  altLastStepTime = now;

  if (vAlternator > ALT_TARGET_MAX_V) {
    altExciteVoltage -= ALT_STEP_V;
  } else {
    altExciteVoltage += ALT_STEP_V;
  }
  altExciteVoltage = constrain(altExciteVoltage, ALT_OUT_MIN_V, ALT_OUT_MAX_V);

  int pwmValue = (int)((altExciteVoltage / ALT_OUT_SUPPLY_V) * 255.0 + 0.5);
  analogWrite(AltExcitePin, pwmValue);
}

// =============================================================================
// SETUP
// =============================================================================
void setup() {
  Serial.begin(9600);

  // Start-up pull-down: drive every digital pin D2-D53 LOW for a moment so
  // floating lines and stray charge start at a known 0V, then the per-pin
  // setup below reconfigures each one. D0/D1 are skipped (Serial RX/TX to the
  // Pi). The ATmega2560 has no internal pull-downs, so pins left as plain
  // INPUT can float again afterwards; only external resistors keep them at 0V.
  for (int pin = 2; pin <= 53; pin++) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }
  delay(10);
  for (int pin = 2; pin <= 53; pin++) {
    pinMode(pin, INPUT);
  }

  // Alternator excitation output — start at 0V, ramps up via control loop
  pinMode(AltExcitePin, OUTPUT);
  analogWrite(AltExcitePin, 0);

  // PC power-switch relay pin — reserved, held LOW (idle) until wiring + logic added
  pinMode(PcPowerPulsePin, OUTPUT);
  digitalWrite(PcPowerPulsePin, LOW);

  // Speed / RPM — D23/D24 are not interrupt-capable on Mega 2560; polled instead.
  // Hall sensors output active-HIGH (5V when magnet passes) so plain INPUT, no pullup.
  pinMode(speedPin, INPUT);
  pinMode(rpmPin,   INPUT);

  // MAX6675 temperature sensor SPI pins
  pinMode(tempCS,  OUTPUT); digitalWrite(tempCS, HIGH);
  pinMode(tempSO,  INPUT);
  pinMode(tempSCK, OUTPUT);

  // NOT USED: ECU wire digital pins — INPUT_PULLUP, actively assigned signals
  //for (int i = 0; i < 4; i++) {
  //  pinMode(WIRE_D_PINS[i], INPUT_PULLUP);
  //}

  // Door open sensor — active HIGH from external signal, plain INPUT
  pinMode(DoorPin, INPUT);

  // Glow plug / aircon dash indicators — active HIGH, plain INPUT
  pinMode(GlowPlugPin, INPUT);
  pinMode(AirconPin,   INPUT);

  // Spare pins — held LOW (pulled down) by default
  for (int i = 0; i < NUM_SPARE_PINS; i++) {
    pinMode(SparePins[i], OUTPUT);
    digitalWrite(SparePins[i], LOW);
  }

  // Oil level sensor — INPUT_PULLUP, float switch to GND when oil is low
  pinMode(OilLevelPin, INPUT_PULLUP);

  // Dashboard page-switch button — internal pull-up, button pulls to GND
  pinMode(PageButtonPin, INPUT_PULLUP);

  // Trip reset button — internal pull-up, button pulls to GND
  pinMode(TripResetPin, INPUT_PULLUP);

  // Indicator and lights inputs via V508VC voltage regulator (active HIGH, external pull-down)
  pinMode(leftIndicatorPin,  INPUT);
  pinMode(rightIndicatorPin, INPUT);
  pinMode(LightsDimPin,      INPUT);
  pinMode(LightsBrightPin,   INPUT);
  pinMode(DaytimeLightsPin,  INPUT);
  pinMode(ParkingLightsPin,  INPUT);
  pinMode(ParkingBrakePin,   INPUT);
  pinMode(SeatbeltPin,       INPUT);

  // Fuel gauge — prime smoothing buffer with a real reading so the average
  // starts correct immediately, instead of ramping up from zero over ~20s
  int fuelSeed = analogRead(fuelPin);
  for (int i = 0; i < numReadings; i++) {
    readings[i] = fuelSeed;
  }
  total = (long)fuelSeed * numReadings;
}

// =============================================================================
// MAIN LOOP
// =============================================================================
void loop() {
  // Alternator excitation control — runs every iteration, independent of
  // the 1s telemetry report below.
  updateAlternatorExcitation();

  // Release the PC power-switch pulse after PULSE_DURATION_MS — non-blocking
  // so it doesn't stall speed/RPM pulse counting elsewhere in loop().
  if (pcPulseActive && millis() - pcPulseStartTime >= PULSE_DURATION_MS) {
    digitalWrite(PcPowerPulsePin, LOW);
    pcPulseActive = false;
  }

  // Record the most recent flasher pulse on every fast loop iteration —
  // see INDICATOR_TIMEOUT_MS comment above for why.
  if (digitalRead(leftIndicatorPin)  == HIGH) lastLeftPulseTime  = millis();
  if (digitalRead(rightIndicatorPin) == HIGH) lastRightPulseTime = millis();
  if (digitalRead(LightsDimPin)      == HIGH) LightsDimSeen      = true;
  if (digitalRead(LightsBrightPin)   == HIGH) LightsBrightSeen   = true;
  if (digitalRead(DaytimeLightsPin)  == HIGH) DaytimeLightsSeen  = true;
  if (digitalRead(ParkingLightsPin)  == HIGH) ParkingLightsSeen  = true;
  if (digitalRead(ParkingBrakePin)   == HIGH) ParkingBrakeSeen   = true;
  if (digitalRead(SeatbeltPin)       == HIGH) SeatbeltSeen       = true;

  // Spare pins: briefly switch to INPUT to sample for a HIGH pulse,
  // then drive back LOW (kept pulled down by default)
  for (int i = 0; i < NUM_SPARE_PINS; i++) {
    pinMode(SparePins[i], INPUT);
    if (digitalRead(SparePins[i]) == HIGH) SparePinSeen[i] = true;
    pinMode(SparePins[i], OUTPUT);
    digitalWrite(SparePins[i], LOW);
  }

  // Dashboard page-switch button — active LOW (INPUT_PULLUP, button to GND)
  if (digitalRead(PageButtonPin) == LOW) {
    PageButtonSeen     = true;
    PageButtonFastSeen = true;
  }

  // Trip reset button — active LOW (INPUT_PULLUP, button to GND)
  if (digitalRead(TripResetPin) == LOW) {
    TripResetSeen = true;
  }

  // DISABLED (D0/D1 = Serial RX/TX): briefly drive LOW, then switch back to
  // INPUT_PULLUP — if the signal pulls the pin back HIGH on its own, latch
  // SPARE5/SPARE11.
  //for (int i = 0; i < 2; i++) {
  //  pinMode(DualModePins[i], OUTPUT);
  //  digitalWrite(DualModePins[i], LOW);
  //  pinMode(DualModePins[i], INPUT_PULLUP);
  //  if (digitalRead(DualModePins[i]) == HIGH) DualModePinSeen[i] = true;
  //}

  // Poll speed / RPM — detect RISING edge from active-HIGH hall sensors
  bool curSpeed = digitalRead(speedPin) == HIGH;
  bool curRpm   = digitalRead(rpmPin)   == HIGH;
  unsigned long now = millis();
  if (curSpeed && !lastSpeedState) {
    if (now - lastSpeedTime > debounceTime) { speedPulses++; lastSpeedTime = now; }
  }
  if (curRpm && !lastRpmState) {
    if (now - lastRpmTime > debounceTime) { rpmPulses++; lastRpmTime = now; }
  }
  lastSpeedState = curSpeed;
  lastRpmState   = curRpm;

  // ── FAST report — page button + indicators, every 100ms ──────────────────
  // Decoupled from the 1s telemetry report below so screen-switching and
  // indicator/hazard updates reach the dashboard with low latency.
  static unsigned long lastFastReport = 0;
  if (millis() - lastFastReport >= 100) {
    lastFastReport = millis();
    bool leftActive  = (millis() - lastLeftPulseTime)  < INDICATOR_TIMEOUT_MS;
    bool rightActive = (millis() - lastRightPulseTime) < INDICATOR_TIMEOUT_MS;
    Serial.print("FAST|PAGEBTN:"); Serial.print(PageButtonFastSeen ? 1 : 0);
    Serial.print("|LINDIC:");      Serial.print(leftActive  ? 1 : 0);
    Serial.print("|RINDIC:");      Serial.print(rightActive ? 1 : 0);
    Serial.println();
    PageButtonFastSeen = false;
  }

  static unsigned long lastReport = 0;
  if (millis() - lastReport < 1000) return;
  lastReport = millis();

  // ── Speed ────────────────────────────────────────────────────────────────
  float kmh = (speedPulses * wheelCircumference * 3.6) / 3.0;
  speedPulses = 0;

  // ── RPM ──────────────────────────────────────────────────────────────────
  unsigned long currentRPM = rpmPulses * 60;
  if (currentRPM > 10000) currentRPM = 0;   // sanity — reject noise
  currentRPM /= 10;
  rpmPulses = 0;

  // ── Temperature ──────────────────────────────────────────────────────────
  float currentTemp = readTemperature();

  // ── Fuel gauge (rolling average) ─────────────────────────────────────────
  total -= readings[readIndex];
  readings[readIndex] = analogRead(fuelPin);
  total += readings[readIndex];
  readIndex = (readIndex + 1) % numReadings;
  average = total / numReadings;

  float fuelVoltage = average * (5.0 / 1023.0);
  float litersLeft  = (fuelVoltage - V_EMPTY) * (TANK_CAPACITY / (V_FULL - V_EMPTY));
  litersLeft = constrain(litersLeft, 0.0, TANK_CAPACITY);
  float rangeKm = litersLeft * avgKmPerLiter;

  // ── Serial output — main telemetry ───────────────────────────────────────
  Serial.print("SPD:");    Serial.print(kmh, 1);
  Serial.print("|RPM:");   Serial.print(currentRPM);
  Serial.print("|TMP:");   Serial.print(currentTemp, 1);
  Serial.print("|LITERS:"); Serial.print(litersLeft, 1);
  Serial.print("|RANGE:"); Serial.print(rangeKm, 0);

  // ── ECU wire voltages (A1–A15) ────────────────────────────────────────────
  // Voltage divider: 10kΩ series + 5.1kΩ to GND maps 0–14.5V automotive to 0–5V ADC.
  // Scale factor: 0.25V automotive = 0.0845V ADC → multiply by (0.25/0.0845) = 2.9586
  float ignitionVoltage   = 0.0;
  float nativeWireVoltage = 0.0;
  for (int i = 0; i < 15; i++) {
    float v = analogRead(WIRE_A_PINS[i]) * (5.0 / 1023.0) * (0.25 / 0.0845);
    Serial.print("|AV"); Serial.print(i + 1);
    Serial.print(":"); Serial.print(v, 2);
    if (i == 13) ignitionVoltage   = v;   // AV14 = A14 = Black/White ignition wire
    if (i == 6)  nativeWireVoltage = v;   // AV7  = A7  = yellow-black coolant sender
  }

  // ── Native coolant temp sender (A7) — raw volts + calibrated °C ──────────
  // NATIVETEMPV is what you read off the log to build/verify the
  // calibration table above; NATIVETEMP is the result of that table.
  Serial.print("|NATIVETEMPV:"); Serial.print(nativeWireVoltage, 2);
  Serial.print("|NATIVETEMP:");  Serial.print(nativeVoltageToTempC(nativeWireVoltage), 1);

  // ── PC power-switch pulse — ignition-off (sleep) / ignition-on (wake) ────
  // Mirrors the Python bridge's 1.5V / 5s debounce so both paths agree on
  // what counts as "ignition off", independently of each other.
  if (ignitionVoltage < IGNITION_SLEEP_THRESHOLD_V) {
    ignitionBelowCount++;
    ignitionAboveCount = 0;
  } else {
    ignitionAboveCount++;
    ignitionBelowCount = 0;
  }

  if (ignitionConfirmedOn && ignitionBelowCount >= IGNITION_DEBOUNCE_COUNT) {
    ignitionConfirmedOn = false;
    digitalWrite(PcPowerPulsePin, HIGH);
    pcPulseActive     = true;
    pcPulseStartTime  = millis();
  } else if (!ignitionConfirmedOn && ignitionAboveCount >= IGNITION_DEBOUNCE_COUNT) {
    ignitionConfirmedOn = true;
    digitalWrite(PcPowerPulsePin, HIGH);
    pcPulseActive     = true;
    pcPulseStartTime  = millis();
  }

  // ── Digital-only ECU wire pins (no ADC — HIGH/LOW reported as 1/0) ─────────
  //Serial.print("|DV33:"); Serial.print(digitalRead(33) == HIGH ? 1 : 0);
  // DVIGNITIONPWR (was digitalRead(14)=TX3, wrong pin) — now covered by AV14 analog
  // DVSTANDBYPOWER (was digitalRead(35), wrong pin)    — now covered by AV15 analog
  Serial.print("|DVGLOWPLUG:"); Serial.print(digitalRead(GlowPlugPin) == HIGH ? 1 : 0);//glowplug indicator on dash (HIGH=active)
  Serial.print("|DVAIRCONSIGNAL:"); Serial.print(digitalRead(AirconPin) == HIGH ? 1 : 0);//aircon indicator on/off

  // ── Alternator excitation debug ──────────────────────────────────────────
  Serial.print("|RAWRED:");  Serial.print(altDebugRawRed, 3);
  Serial.print("|RAWBW:");   Serial.print(altDebugRawBW, 3);
  Serial.print("|ALTRED:");  Serial.print(altDebugVRed, 2);
  Serial.print("|ALTBW:");   Serial.print(altDebugVBW, 2);
  Serial.print("|ALTOUT:");  Serial.print(altExciteVoltage, 2);

  // ── Indicators (same "last pulse" timestamps used by the FAST report) ────
  Serial.print("|LINDIC:");      Serial.print((millis() - lastLeftPulseTime)  < INDICATOR_TIMEOUT_MS ? 1 : 0);
  Serial.print("|RINDIC:");      Serial.print((millis() - lastRightPulseTime) < INDICATOR_TIMEOUT_MS ? 1 : 0);
  Serial.print("|LIGHTSDIM:");     Serial.print(LightsDimSeen      ? 1 : 0);
  Serial.print("|LIGHTSBRIGHT:");  Serial.print(LightsBrightSeen   ? 1 : 0);
  Serial.print("|DAYTIMEDRIVING:"); Serial.print(DaytimeLightsSeen ? 1 : 0);
  Serial.print("|PARKINGLIGHTS:");  Serial.print(ParkingLightsSeen ? 1 : 0);
  Serial.print("|PARKINGBRAKE:");   Serial.print(ParkingBrakeSeen  ? 1 : 0);
  Serial.print("|SEATBELT:");       Serial.print(SeatbeltSeen      ? 1 : 0);

  // ── Spare pins (latched HIGH-pulse detection) ─────────────────────────────
  Serial.print("|SPARE2:");  Serial.print(SparePinSeen[0]  ? 1 : 0);
  Serial.print("|DVDOORSIGNAL:"); Serial.print(digitalRead(DoorPin) == HIGH ? 1 : 0);//door open (HIGH=open, LOW=closed)
  Serial.print("|DVOILSIGNAL:"); Serial.print(digitalRead(OilLevelPin) == HIGH ? 1 : 0);//oil level (HIGH=normal, LOW=low)
//  Serial.print("|SPARE11:"); Serial.print(SparePinSeen[1]  ? 1 : 0);
  Serial.print("|SPARE12:"); Serial.print(SparePinSeen[2]  ? 1 : 0);
  Serial.print("|SPARE13:"); Serial.print(SparePinSeen[3]  ? 1 : 0);
  Serial.print("|SPARE32:"); Serial.print(SparePinSeen[4]  ? 1 : 0);
  Serial.print("|SPARE38:"); Serial.print(SparePinSeen[5]  ? 1 : 0);
  //Serial.print("|SPARE5:");  Serial.print(DualModePinSeen[0] ? 1 : 0);  // DISABLED: D0/D1
  //Serial.print("|SPARE11:"); Serial.print(DualModePinSeen[1] ? 1 : 0);

  // ── Dashboard page-switch button (latched over the 1s window) ────────────
  Serial.print("|PAGEBTN:"); Serial.print(PageButtonSeen ? 1 : 0);

  // ── Trip reset button (latched over the 1s window) ────────────────────────
  Serial.print("|TRIPRESET:"); Serial.print(TripResetSeen ? 1 : 0);

  LightsDimSeen      = false;
  LightsBrightSeen   = false;
  DaytimeLightsSeen  = false;
  ParkingLightsSeen  = false;
  ParkingBrakeSeen   = false;
  SeatbeltSeen       = false;
  for (int i = 0; i < NUM_SPARE_PINS; i++) SparePinSeen[i] = false;
  //DualModePinSeen[0] = false;  // DISABLED: D0/D1
  //DualModePinSeen[1] = false;
  PageButtonSeen     = false;
  TripResetSeen      = false;

  Serial.println();   // end of line — Python readline() triggers on this
}
