// =============================================================================
// K-Line Wakeup — Hardware UARTs, echo-aware
//
// Brown-Red  K-Line: D16=TX2, D17=RX2  → Serial2 at 10400 baud (KWP2000)
// White-Blue L-Line: D18=TX1, D19=RX1  → Serial1 at 1200 baud  (monitoring)
//
// Key improvement over bit-bang: Serial2 gives hardware-accurate 10416 baud
// (0.16% error, well within KWP2000 ±3%). TX bytes echo back on RX (shared
// bus), so we discard the echo before listening for the ECU response.
//
// STI (Yellow/Black on Ford diag plug) grounded throughout.
//
// Wiring Brown-Red:
//   D16 ──[100Ω]──► Brown-Red K-Line
//   Brown-Red ──[100kΩ]──┬──► D17 RX
//                         └──[100kΩ]──► GND
//
// Open Serial Monitor at 115200 baud.
// =============================================================================

#define PIN_K_TX  16   // Serial2 TX
#define PIN_K_RX  17   // Serial2 RX
#define PIN_L_TX  18   // Serial1 TX
#define PIN_L_RX  19   // Serial1 RX

const uint8_t ADDRS[]   = { 0x33, 0x10, 0x01 };
const uint8_t NUM_ADDRS = 3;

// ---- Helpers ----------------------------------------------------------------

void ph(uint8_t b) { if (b < 0x10) Serial.print('0'); Serial.print(b, HEX); }

uint8_t chksum(const uint8_t* buf, int len) {
  uint16_t s = 0;
  for (int i = 0; i < len; i++) s += buf[i];
  return s & 0xFF;
}

// Drive both K and L LOW (Serial2+Serial1 must be ended first).
void busLow()     { pinMode(PIN_K_TX, OUTPUT); digitalWrite(PIN_K_TX, LOW);
                    pinMode(PIN_L_TX, OUTPUT); digitalWrite(PIN_L_TX, LOW); }
void busRelease() { pinMode(PIN_K_TX, INPUT);  pinMode(PIN_L_TX, INPUT); }

// Restart both UARTs after GPIO use.
void uartsBegin() {
  Serial2.begin(10400);
  Serial1.begin(1200);
}

void uartsEnd() {
  Serial2.end();
  Serial1.end();
}

// Discard up to n bytes from Serial2 (TX echo). Returns count discarded.
int discardEcho(int n) {
  unsigned long dl = millis() + 20;   // echo arrives within ~5 ms at 10400 baud
  int got = 0;
  while (got < n && millis() < dl) {
    if (Serial2.available()) { Serial2.read(); got++; }
  }
  return got;
}

// Listen on Serial2 for up to window_ms; print bytes with time-since-TX offset.
int listenK(unsigned long window_ms) {
  unsigned long start = millis();
  unsigned long end   = start + window_ms;
  int n = 0;
  while (millis() < end) {
    // Also drain white-blue to avoid Serial1 buffer overflow
    while (Serial1.available()) {
      uint8_t wb = Serial1.read();
      Serial.print(F("  [WB 0x")); ph(wb); Serial.println(F("]"));
    }
    if (Serial2.available()) {
      uint8_t b = Serial2.read();
      Serial.print(F("    KLine RX +"));
      Serial.print(millis() - start);
      Serial.print(F("ms  0x")); ph(b);
      Serial.print(F("  (")); Serial.print(b); Serial.println(F(")"));
      n++;
    }
  }
  return n;
}

// ---- Fast Init (ISO 14230) --------------------------------------------------

bool tryFastInit(uint8_t target) {
  Serial.print(F("\n  [Fast Init HW] target=0x")); ph(target); Serial.println();

  // Wait for K-Line idle, then enforce 300 ms quiet time
  unsigned long t = millis();
  while (digitalRead(PIN_K_RX) == LOW && millis() - t < 2000);
  delay(300);

  uartsEnd();
  busLow();    delay(25);   // W1 — 25 ms LOW
  busRelease(); delay(25);  // W2 — 25 ms HIGH before first byte
  uartsBegin();

  uint8_t fi[] = { 0xC1, target, 0xF1, 0x81, 0x00 };
  fi[4] = chksum(fi, 4);
  Serial.print(F("    TX: "));
  for (int i = 0; i < 5; i++) { ph(fi[i]); Serial.print(' '); }
  Serial.println();

  Serial2.write(fi, 5);
  Serial2.flush();          // wait until all TX bits are out
  int disc = discardEcho(5);
  Serial.print(F("    (echo discarded: ")); Serial.print(disc); Serial.println(F(" bytes)"));

  int n = listenK(1500);
  if (!n) Serial.println(F("    No response."));
  return n > 0;
}

// ---- 5-baud Init (ISO 9141-2) -----------------------------------------------

bool try5BaudInit(uint8_t addr) {
  Serial.print(F("\n  [5-baud HW] addr=0x")); ph(addr); Serial.println();

  unsigned long t = millis();
  while (digitalRead(PIN_K_RX) == LOW && millis() - t < 4000);
  delay(300);

  uartsEnd();
  // Send address at 5 baud on K+L simultaneously
  busLow(); delay(200);
  for (int i = 0; i < 8; i++) {
    if ((addr >> i) & 1) busRelease(); else busLow();
    delay(200);
  }
  busRelease(); delay(200);
  uartsBegin();

  // Wait for 0x55 sync (ECU responds at 10400 baud on K-Line)
  unsigned long dl = millis() + 1500;
  while (millis() < dl) {
    while (Serial1.available()) {
      uint8_t wb = Serial1.read();
      Serial.print(F("  [WB 0x")); ph(wb); Serial.println(F("]"));
    }
    if (Serial2.available()) {
      uint8_t sync = Serial2.read();
      Serial.print(F("    Sync: 0x")); ph(sync); Serial.println();
      if (sync != 0x55) {
        Serial.println(F("    Not 0x55 — unexpected."));
        listenK(300);
        return false;
      }
      Serial.println(F("    *** 0x55 — ECU responding! ***"));
      int kw1 = -1, kw2 = -1;
      unsigned long kwDl = millis() + 400;
      while (millis() < kwDl) {
        if (Serial2.available()) {
          uint8_t kw = Serial2.read();
          Serial.print(F("    KW: 0x")); ph(kw); Serial.println();
          if (kw1 < 0) kw1 = kw; else if (kw2 < 0) { kw2 = kw; break; }
          kwDl = millis() + 200;
        }
      }
      if (kw2 >= 0) {
        delay(25);
        uint8_t inv = ~(uint8_t)kw2;
        Serial2.write(inv);
        Serial2.flush();
        discardEcho(1);
        Serial.print(F("    TX ~KW2=0x")); ph(inv); Serial.println();
        int ack = -1;
        unsigned long ackDl = millis() + 400;
        while (millis() < ackDl) {
          if (Serial2.available()) { ack = Serial2.read(); break; }
        }
        if (ack >= 0) {
          Serial.print(F("    ACK: 0x")); ph(ack); Serial.println();
          if (ack == 0xCC) Serial.println(F("    *** INIT COMPLETE ***"));
        }
      }
      return true;
    }
  }
  Serial.println(F("    No response."));
  return false;
}

// =============================================================================

void setup() {
  Serial.begin(115200);
  pinMode(PIN_K_RX, INPUT);
  pinMode(PIN_L_RX, INPUT);
  busRelease();
  uartsBegin();

  Serial.println(F("\n=== K-Line Wakeup (HW Serial2 10400 / Serial1 1200) ==="));
  Serial.println(F("STI must be grounded."));
  Serial.print(F("Brown-Red  D17: ")); Serial.println(digitalRead(PIN_K_RX) ? F("HIGH") : F("LOW"));
  Serial.print(F("White-Blue D19: ")); Serial.println(digitalRead(PIN_L_RX) ? F("HIGH") : F("LOW"));

  // 2 s passive baseline — any spontaneous traffic on either line
  Serial.println(F("\n--- Passive 2s ---"));
  unsigned long t = millis();
  while (millis() - t < 2000) {
    if (Serial2.available()) {
      uint8_t b = Serial2.read();
      Serial.print(F("  KLine 0x")); ph(b); Serial.println();
    }
    if (Serial1.available()) {
      uint8_t b = Serial1.read();
      Serial.print(F("  WB    0x")); ph(b); Serial.println();
    }
  }

  // Fast Init
  Serial.println(F("\n--- Fast Init (K+L, Serial2 10400 baud) ---"));
  for (uint8_t i = 0; i < NUM_ADDRS; i++) {
    if (tryFastInit(ADDRS[i])) break;
  }

  // 5-baud Init
  Serial.println(F("\n--- 5-baud Init (K+L, Serial2 10400 baud) ---"));
  for (uint8_t i = 0; i < NUM_ADDRS; i++) {
    if (try5BaudInit(ADDRS[i])) break;
  }

  uartsBegin();
  Serial.println(F("\n=== Monitoring (KLine=Serial2, WB=Serial1) ==="));
}

void loop() {
  static unsigned long lastPrint = 0;

  while (Serial2.available()) {
    uint8_t b = Serial2.read();
    Serial.print(F("KLine t=")); Serial.print(millis());
    Serial.print(F("ms 0x")); ph(b); Serial.println();
  }
  while (Serial1.available()) {
    uint8_t b = Serial1.read();
    Serial.print(F("WB    t=")); Serial.print(millis());
    Serial.print(F("ms 0x")); ph(b); Serial.println();
  }
  if (millis() - lastPrint >= 5000) {
    lastPrint = millis();
    Serial.print(F("K:")); Serial.print(digitalRead(PIN_K_RX) ? F("H") : F("L"));
    Serial.print(F(" L:")); Serial.println(digitalRead(PIN_L_RX) ? F("H") : F("L"));
  }
}
