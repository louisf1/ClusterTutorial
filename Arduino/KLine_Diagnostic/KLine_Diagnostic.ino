// =============================================================================
// K-Line Diagnostic — White-Blue Wire on A3
//
// Fast-samples A3 at ~77kHz (ADC prescaler 16) to characterise the PCM signal.
// Reports every 4 seconds:
//   - Current voltage, duty cycle, sample rate
//   - Transition count, min/max pulse width, estimated baud rate
//   - Histogram of pulse widths by decade
//
// K-Line (ISO 9141 / KWP2000) at 10.4 kbaud → bit period ~96 µs
// K-Line slow init (5 baud)               → bit period  200 ms
//
// Open Serial Monitor at 115200 baud after upload.
// =============================================================================

// --- ADC fast mode: prescaler 16 → 1 MHz ADC clock → ~13 µs per sample ---
inline void adcSetFast() {
  ADCSRA = (ADCSRA & ~0x07) | 0x04;
}

// Direct ADC read for A3 (channel 3 on Mega 2560)
inline int adcRead_A3() {
  ADMUX  = (1 << REFS0) | 0x03;  // AVCC reference, channel 3
  ADCSRB = 0;                      // MUX5=0 selects channels 0-7
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC));
  return ADC;
}

// Voltage display scale — same factor as main sketch (10k series, clamp at 5V)
const float SCALE = 2.4f * 5.0f / 1023.0f;

// Threshold: midpoint between low (~225 counts ≈ 2.6V) and high (1023 counts = 12V)
const int THRESH = 511;  // ≈ 6V displayed

// Pulse-width histogram
const int BUCKETS = 6;
const unsigned long LIMITS[BUCKETS] = { 100, 500, 5000, 50000, 500000, 0xFFFFFFFFUL };
const char* LABELS[BUCKETS] = {
  "<100us", "100-500us", "500us-5ms", "5-50ms", "50-500ms", ">500ms"
};
unsigned long hist[BUCKETS];

// Per-window accumulators
unsigned long transitions  = 0;
unsigned long minPulse     = 0xFFFFFFFFUL;
unsigned long maxPulse     = 0;
unsigned long totalSamples = 0;
unsigned long highSamples  = 0;

int           lastState    = -1;
unsigned long lastTransUs  = 0;

const unsigned long REPORT_MS = 4000;
unsigned long lastReport = 0;

// =============================================================================
void setup() {
  Serial.begin(115200);
  adcSetFast();
  memset(hist, 0, sizeof(hist));

  Serial.println(F("=== K-Line Diagnostic (A3 white-blue) ==="));
  Serial.println(F("Fast ADC ~77kHz | 4s windows | 115200 baud"));
  Serial.println(F("K-Line 10.4kbaud = 96us/bit | 5-baud init = 200ms/bit"));
  Serial.println();
}

// =============================================================================
void loop() {
  int raw   = adcRead_A3();
  int state = (raw > THRESH) ? 1 : 0;
  totalSamples++;
  if (state) highSamples++;

  unsigned long now = micros();

  if (lastState < 0) {
    lastState   = state;
    lastTransUs = now;
  } else if (state != lastState) {
    unsigned long pw = now - lastTransUs;
    if (pw < minPulse) minPulse = pw;
    if (pw > maxPulse) maxPulse = pw;
    transitions++;
    for (int b = 0; b < BUCKETS; b++) {
      if (pw <= LIMITS[b]) { hist[b]++; break; }
    }
    lastState   = state;
    lastTransUs = now;
  }

  if (millis() - lastReport < REPORT_MS) return;
  lastReport = millis();

  float vNow = raw * SCALE;
  float duty = totalSamples ? (100.0f * highSamples / totalSamples) : 0.0f;
  float sps  = totalSamples / (REPORT_MS / 1000.0f);

  Serial.print(F("V="));         Serial.print(vNow, 2);
  Serial.print(F("  HIGH%="));   Serial.print(duty, 1);
  Serial.print(F("  SPS="));     Serial.print(sps, 0);
  Serial.print(F("  Trans="));   Serial.println(transitions);

  if (transitions >= 2) {
    Serial.print(F("  MinPulse=")); Serial.print(minPulse);   Serial.print(F("us"));
    Serial.print(F("  MaxPulse=")); Serial.print(maxPulse);   Serial.print(F("us"));
    unsigned long baud = (unsigned long)(1000000.0f / minPulse);
    Serial.print(F("  EstBaud~")); Serial.println(baud);

    Serial.print(F("  Hist: "));
    for (int b = 0; b < BUCKETS; b++) {
      if (hist[b]) {
        Serial.print(LABELS[b]); Serial.print('='); Serial.print(hist[b]); Serial.print(F("  "));
      }
    }
    Serial.println();
  } else {
    Serial.println(F("  (no transitions — line static this window)"));
  }
  Serial.println();

  // Reset accumulators for next window
  transitions  = 0;
  minPulse     = 0xFFFFFFFFUL;
  maxPulse     = 0;
  totalSamples = 0;
  highSamples  = 0;
  memset(hist, 0, sizeof(hist));
}
