// =============================================================================
// K-Line Diagnostic — passive bus monitor
//
// CH_A (Grey/Brown-Red)  — D17, software bit-bang sampler, reports every 4s
// CH_B (White-Blue)      — D19 = Serial1 RX, hardware UART at 1200 baud
//                          Prints every received byte as hex + timestamp
//
// White-Blue shows ~1200 baud activity (not KWP2000).
// Hardware UART captures frames without bit-timing jitter.
//
// D19 connects directly to White-Blue — no divider needed if ECU drives
// to 5V-compatible levels. If idle is >5V, add 10kΩ+5.1kΩ divider first.
//
// Open Serial Monitor at 115200 baud.
// =============================================================================

// --- Software sampler for CH_A (D17) ---

struct KCh {
  const char* name;
  uint8_t rx;
};

const KCh CH_A = { "BROWN-RED", 17 };

const unsigned long REPORT_MS = 4000;
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

ChanState stA;

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
  if (s.lastState < 0) { s.lastState = state; s.lastTransUs = now; return; }
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
  Serial.print(F("[")); Serial.print(c.name); Serial.print(F("]  "));
  Serial.print(digitalRead(c.rx) == HIGH ? F("HIGH") : F("LOW "));
  Serial.print(F("  HIGH%=")); Serial.print(duty, 1);
  Serial.print(F("  Trans=")); Serial.println(s.transitions);
  if (s.transitions >= 2) {
    Serial.print(F("    MinPulse=")); Serial.print(s.minPulse); Serial.print(F("us"));
    Serial.print(F("  MaxPulse=")); Serial.print(s.maxPulse); Serial.print(F("us"));
    Serial.print(F("  EstBaud~")); Serial.println(1000000UL / s.minPulse);
    Serial.print(F("    Hist: "));
    for (int b = 0; b < BUCKETS; b++) {
      if (s.hist[b]) { Serial.print(LABELS[b]); Serial.print('='); Serial.print(s.hist[b]); Serial.print(F("  ")); }
    }
    Serial.println();
  } else {
    Serial.println(F("    (no transitions — line static)"));
  }
  resetState(s);
  s.lastReport = millis();
}

// =============================================================================
void setup() {
  Serial.begin(115200);
  Serial1.begin(1200);        // D19 = RX1 — White-Blue at 1200 baud
  pinMode(CH_A.rx, INPUT);
  resetState(stA);
  stA.lastReport = millis();

  Serial.println(F("=== K-Line Diagnostic ==="));
  Serial.print(F("BROWN-RED D17 idle: "));
  Serial.println(digitalRead(CH_A.rx) == HIGH ? F("HIGH") : F("LOW"));
  Serial.print(F("WHITE-BLUE D19 idle (Serial1 1200 baud): "));
  Serial.println(digitalRead(19) == HIGH ? F("HIGH") : F("LOW"));
  Serial.println(F("Sampling — BROWN-RED report every 4s, WHITE-BLUE bytes printed live"));
  Serial.println();
}

void loop() {
  // Capture Serial1 bytes (White-Blue, 1200 baud) and echo back
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    Serial.print(F("WB RX  t=")); Serial.print(millis());
    Serial.print(F("ms  0x"));
    if (b < 0x10) Serial.print('0');
    Serial.print(b, HEX);
    Serial.print(F("  (")); Serial.print(b); Serial.println(F(")"));
    Serial1.write(0x10);
    Serial.println(F("WB TX  0x10"));
  }

  // Software sampler for Brown-Red
  sampleChannel(CH_A, stA);
  printReport(CH_A, stA);
}
