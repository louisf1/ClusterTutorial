// =============================================================================
// K-Line Diagnostic — Grey (D17) and White-Blue (D19) passive bus monitor
//
// Wiring (per channel):
//   K-Line wire ──[10kΩ]──┬──────────► RX pin (D17 or D19)
//                          └──[5.1kΩ]──► GND
//   (node = 4.0V when bus is 12V idle → safe on 5V pin; 0V when LOW)
//
// Reports every 4 seconds per channel:
//   - Bus state, transition count, min/max pulse width, estimated baud rate
//   - Histogram of pulse widths (helps identify protocol/baud)
//
// K-Line 10.4 kbaud → bit period ~96 µs
// K-Line 5-baud init → bit period 200 ms
//
// Open Serial Monitor at 115200 baud.
// =============================================================================

struct KCh {
  const char* name;
  uint8_t rx;
};

const KCh CH_A = { "GREY",       17 };
const KCh CH_B = { "WHITE-BLUE", 19 };

const unsigned long REPORT_MS = 4000;

// Pulse-width histogram buckets
const int BUCKETS = 6;
const unsigned long LIMITS[BUCKETS] = { 100, 500, 5000, 50000, 500000, 0xFFFFFFFFUL };
const char* LABELS[BUCKETS] = { "<100us", "100-500us", "500us-5ms", "5-50ms", "50-500ms", ">500ms" };

struct ChanState {
  unsigned long transitions;
  unsigned long minPulse;
  unsigned long maxPulse;
  unsigned long totalSamples;
  unsigned long highSamples;
  unsigned long hist[BUCKETS];
  int           lastState;
  unsigned long lastTransUs;
  unsigned long lastReport;
};

ChanState stA, stB;

void resetState(ChanState& s) {
  s.transitions  = 0;
  s.minPulse     = 0xFFFFFFFFUL;
  s.maxPulse     = 0;
  s.totalSamples = 0;
  s.highSamples  = 0;
  s.lastState    = -1;
  s.lastTransUs  = 0;
  memset(s.hist, 0, sizeof(s.hist));
}

void sampleChannel(const KCh& c, ChanState& s) {
  int state = digitalRead(c.rx);
  s.totalSamples++;
  if (state == HIGH) s.highSamples++;

  unsigned long now = micros();
  if (s.lastState < 0) {
    s.lastState   = state;
    s.lastTransUs = now;
    return;
  }
  if (state != s.lastState) {
    unsigned long pw = now - s.lastTransUs;
    if (pw < s.minPulse) s.minPulse = pw;
    if (pw > s.maxPulse) s.maxPulse = pw;
    s.transitions++;
    for (int b = 0; b < BUCKETS; b++) {
      if (pw <= LIMITS[b]) { s.hist[b]++; break; }
    }
    s.lastState   = state;
    s.lastTransUs = now;
  }
}

void printReport(const KCh& c, ChanState& s) {
  if (millis() - s.lastReport < REPORT_MS) return;
  s.lastReport = millis();

  float duty = s.totalSamples ? (100.0f * s.highSamples / s.totalSamples) : 0.0f;
  unsigned long sps = s.totalSamples / (REPORT_MS / 1000UL);

  Serial.print(F("[")); Serial.print(c.name); Serial.print(F("]  "));
  Serial.print(digitalRead(c.rx) == HIGH ? F("HIGH") : F("LOW "));
  Serial.print(F("  HIGH%=")); Serial.print(duty, 1);
  Serial.print(F("  SPS~")); Serial.print(sps);
  Serial.print(F("  Trans=")); Serial.println(s.transitions);

  if (s.transitions >= 2) {
    Serial.print(F("    MinPulse=")); Serial.print(s.minPulse); Serial.print(F("us"));
    Serial.print(F("  MaxPulse=")); Serial.print(s.maxPulse); Serial.print(F("us"));
    unsigned long baud = 1000000UL / s.minPulse;
    Serial.print(F("  EstBaud~")); Serial.println(baud);
    Serial.print(F("    Hist: "));
    for (int b = 0; b < BUCKETS; b++) {
      if (s.hist[b]) {
        Serial.print(LABELS[b]); Serial.print('='); Serial.print(s.hist[b]); Serial.print(F("  "));
      }
    }
    Serial.println();
  } else {
    Serial.println(F("    (no transitions this window — line static)"));
  }

  resetState(s);
  s.lastReport = millis();  // set after reset so timer restarts cleanly
}

// =============================================================================
void setup() {
  Serial.begin(115200);
  pinMode(CH_A.rx, INPUT);
  pinMode(CH_B.rx, INPUT);
  resetState(stA);
  resetState(stB);
  stA.lastReport = millis();
  stB.lastReport = millis();
  Serial.println(F("=== K-Line Diagnostic ==="));
  Serial.print(F("GREY D17 idle: "));  Serial.println(digitalRead(CH_A.rx) == HIGH ? F("HIGH") : F("LOW"));
  Serial.print(F("WHITE-BLUE D19 idle: ")); Serial.println(digitalRead(CH_B.rx) == HIGH ? F("HIGH") : F("LOW"));
  Serial.println(F("Sampling both channels — report every 4s"));
  Serial.println();
}

void loop() {
  sampleChannel(CH_A, stA);
  sampleChannel(CH_B, stB);
  printReport(CH_A, stA);
  printReport(CH_B, stB);
}
