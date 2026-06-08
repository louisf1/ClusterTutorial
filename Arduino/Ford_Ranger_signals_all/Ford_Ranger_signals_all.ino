// =============================================================================
// Ford Ranger ECU Harness — Arduino Mega 2560 Telemetry
// =============================================================================
// Serial output format (one line per second):
//   SPD:xx.x|RPM:xxxx|TMP:xx.x|LITERS:xx.x|RANGE:xxx
//   |AV2:x.xx|AV3:x.xx|AV4:x.xx|AV5:x.xx|AV6:x.xx|AV7:x.xx
//   |AV8:x.xx|AV9:x.xx|AV10:x.xx|AV11:x.xx|AV12:x.xx|AV13:x.xx
//   |LINDIC:x|RINDIC:x  (1=active, 0=off — latched over the 1s window)
//
// AV values are 0–5V. Each wire uses a 10kΩ series resistor (current limiting
// only — no voltage divider). Signals above 5V clamp at the protection diode.
// Python bridge uses VOLTAGE_SCALE=1.0 (no scaling needed).
// =============================================================================

// =============================================================================
// INDICATORS — D25 (left) / D26 (right) via V508VC voltage regulator
// LIGHTS     — D27 (dim/parking) via V508VC voltage regulator
// All active HIGH; external 10kΩ pull-down holds LOW when off.
// Hazard is derived in Python bridge (left AND right both active).
// State is latched across the 1s report window so fast blinks are never missed.
// =============================================================================
const int leftIndicatorPin  = 25;
const int rightIndicatorPin = 26;
const int LightsDimPin      = 27;
const int LightsBrightPin   = 28;
const int DaytimeLightsPin  = 29;
bool leftIndicatorSeen  = false;
bool rightIndicatorSeen = false;
bool LightsDimSeen      = false;
bool LightsBrightSeen   = false;
bool DaytimeLightsSeen  = false;

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
// Internal PCM temperature — A15
// Alternator controller -A14
// =============================================================================
const int PCM_Temprature_Pin = A15;
const int Alternator_Control = A14;
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
//
//			"red-black",    //: unknown - to be identified :Arduino AnaLog/Digital Pin  5
//			"black-white",    //: ignition power on-off and Timer Control Valve :Arduino Digital Pin 33
//			"black-yellow",    //: Connected to ignition key start barrel :Arduino AnaLog/Digital Pin  13
//			"blue-black",    //: Timer Control Valve :Arduino AnaLog/Digital Pin  2
//			"blue-pink",    //: Crankshaft Position Sensor :Arduino AnaLog/Digital Pin 1
//			"brown",    //: Glow plug relay :AnaLog/Digital Digital Pin 34
//			"brown-red",    //: unknown :Arduino AnaLog/Digital Pin  7
//			"brown-white",    //: Control Lever Position :Arduino AnaLog/Digital Pin  9
//			"brown-yellow",    //: unknown :Arduino AnaLog/Digital Pin  8
//			"green",    //: FICD solenoid Valve no1 :Arduino AnaLog/Digital Pin  11
//			"green-white",    //: Idle control switch :Arduino AnaLog/Digital Pin  4
//			"pink",    //: FICD solenoid Valve no2 :Arduino AnaLog/Digital Pin  12
//			"red",    //: standby power :Arduino Digital Pin 36
//			"red-blue",    //: Control Lever Position :Arduino AnaLog/Digital Pin  6
//			"red-white",    //: unknown :Arduino Digital Pin 35
//			"white-blue",    //: PCM internal control :Arduino AnaLog/Digital Pin  3
//			"yellow"    //: EGR solonoid Valve :Arduino AnaLog/Digital Pin 10
// =============================================================================

// Analog pins — voltage reads only
const int WIRE_A_PINS[13] = {A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13 };

// Digital pins — reserved for future functions (input_pullup configured in setup)
const int WIRE_D_PINS[18] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 33, 34, 35, 36, 38};

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
// SETUP
// =============================================================================
void setup() {
  Serial.begin(9600);

  // Speed / RPM — D23/D24 are not interrupt-capable on Mega 2560; polled instead.
  // Hall sensors output active-HIGH (5V when magnet passes) so plain INPUT, no pullup.
  pinMode(speedPin, INPUT);
  pinMode(rpmPin,   INPUT);

  // MAX6675 temperature sensor SPI pins
  pinMode(tempCS,  OUTPUT); digitalWrite(tempCS, HIGH);
  pinMode(tempSO,  INPUT);
  pinMode(tempSCK, OUTPUT);

  // ECU wire digital pins — INPUT_PULLUP, functions assigned later
  for (int i = 0; i < 18; i++) {
    pinMode(WIRE_D_PINS[i], INPUT_PULLUP);
  }

  // Indicator and lights inputs via V508VC voltage regulator (active HIGH, external pull-down)
  pinMode(leftIndicatorPin,  INPUT);
  pinMode(rightIndicatorPin, INPUT);
  pinMode(LightsDimPin,      INPUT);
  pinMode(LightsBrightPin,   INPUT);
  pinMode(DaytimeLightsPin,  INPUT);

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
  // Latch indicator state on every fast loop iteration so blinks are never missed
  if (digitalRead(leftIndicatorPin)  == HIGH) leftIndicatorSeen  = true;
  if (digitalRead(rightIndicatorPin) == HIGH) rightIndicatorSeen = true;
  if (digitalRead(LightsDimPin)      == HIGH) LightsDimSeen      = true;
  if (digitalRead(LightsBrightPin)   == HIGH) LightsBrightSeen   = true;
  if (digitalRead(DaytimeLightsPin)  == HIGH) DaytimeLightsSeen  = true;

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

  // ── ECU wire voltages (A1–A13 via 10K ohm resistor) ───────────────────────
  for (int i = 0; i < 13; i++) {
    float v = analogRead(WIRE_A_PINS[i]) * (2.4 * (5.0 / 1023.0));
    Serial.print("|AV"); Serial.print(i + 1);
    Serial.print(":"); Serial.print(v, 2);
  }

  // ── Digital-only ECU wire pins (no ADC — HIGH/LOW reported as 1/0) ─────────
  //Serial.print("|DV33:"); Serial.print(digitalRead(33) == HIGH ? 1 : 0);
  Serial.print("|DVIGNITIONPWR:"); Serial.print(digitalRead(34) == HIGH ? 1 : 0);//Ignition power on/off
  Serial.print("|DVSTANDBYPOWER:"); Serial.print(digitalRead(35) == HIGH ? 1 : 0);//battery standby power on/off 
  Serial.print("|DVGLOWPLUG:"); Serial.print(digitalRead(36) == HIGH ? 1 : 0);//glowplug indicator on dash
  Serial.print("|DVAIRCONSIGNAL:"); Serial.print(digitalRead(38) == HIGH ? 1 : 0);//aircon indicator on/off

  // ── Indicators (latched over the 1s window) ──────────────────────────────
  Serial.print("|LINDIC:");      Serial.print(leftIndicatorSeen  ? 1 : 0);
  Serial.print("|RINDIC:");      Serial.print(rightIndicatorSeen ? 1 : 0);
  Serial.print("|LIGHTSDIM:");     Serial.print(LightsDimSeen      ? 1 : 0);
  Serial.print("|LIGHTSBRIGHT:");  Serial.print(LightsBrightSeen   ? 1 : 0);
  Serial.print("|DAYTIMEDRIVING:"); Serial.print(DaytimeLightsSeen ? 1 : 0);

  leftIndicatorSeen  = false;
  rightIndicatorSeen = false;
  LightsDimSeen      = false;
  LightsBrightSeen   = false;
  DaytimeLightsSeen  = false;

  Serial.println();   // end of line — Python readline() triggers on this
}
