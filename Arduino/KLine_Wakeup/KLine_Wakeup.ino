// =============================================================================
// K-Line Wakeup Test — ISO 14230 Fast Init + ISO 9141-2 5-baud Init
//
// Physical setup:
//   D3  → K-Line wire (direct or through 10kΩ — script detects which)
//   A3  → K-Line wire (through 10kΩ series — read only, safe at 12V)
//
// The sketch:
//   1. Verifies we can actually pull the line LOW before attempting init
//   2. Tries ISO 14230 Fast Init (25ms LOW + 25ms HIGH + StartComm frame)
//   3. Falls back to ISO 9141-2 5-baud Init (0x33 at 5 baud)
//   4. After any response, dumps all received bytes and identifies protocol
//
// Open Serial Monitor at 115200 baud.
// =============================================================================

const int K_TX   = 3;    // D3 — transmit (drives K-Line LOW)
const int BIT_US = 96;   // 1 000 000 / 10400 = 96.15 µs — rounded

// --- Fast ADC for A3 (monitoring) ---
inline void adcFast() { ADCSRA = (ADCSRA & ~7) | 4; }  // prescaler 16

inline int adcA3() {
  ADMUX  = (1 << REFS0) | 3;
  ADCSRB = 0;
  ADCSRA |= (1 << ADSC);
  while (ADCSRA & (1 << ADSC));
  return ADC;
}

// true = bus HIGH (>~6V displayed), false = bus LOW
bool busHigh() { return adcA3() > 511; }

// --- K-Line drive primitives ---
// We switch D3 between OUTPUT-LOW (dominant) and INPUT (recessive/release).
// No pull-up on INPUT — let the PCM's internal pull-up own the HIGH state.
void kLow()    { pinMode(K_TX, OUTPUT); digitalWrite(K_TX, LOW); }
void kRelease(){ pinMode(K_TX, INPUT); }

// --- Verify we can actually pull the line low ---
// Returns true if bus goes below threshold when we drive D3 LOW.
bool canDriveLow() {
  kLow();
  delayMicroseconds(500);
  bool low = !busHigh();
  kRelease();
  delayMicroseconds(500);
  return low;
}

// --- Send one byte at 10.4 kbaud (LSB first, 1 start + 8 data + 1 stop) ---
void kSendByte(uint8_t b) {
  kLow();                                 // start bit
  delayMicroseconds(BIT_US);
  for (int i = 0; i < 8; i++) {
    if (b & 1) kRelease(); else kLow();
    b >>= 1;
    delayMicroseconds(BIT_US);
  }
  kRelease();                             // stop bit
  delayMicroseconds(BIT_US);
}

// --- Receive one byte at 10.4 kbaud. Returns -1 on timeout (ms). ---
int kRecvByte(unsigned int timeout_ms) {
  unsigned long deadline = millis() + timeout_ms;
  while (busHigh()) {                     // wait for start bit (HIGH → LOW)
    if (millis() > deadline) return -1;
  }
  delayMicroseconds(BIT_US + BIT_US / 2); // skip to centre of first data bit
  uint8_t b = 0;
  for (int i = 0; i < 8; i++) {
    if (busHigh()) b |= (1 << i);
    delayMicroseconds(BIT_US - 13);       // -13µs to account for ADC read time
  }
  delayMicroseconds(BIT_US);             // consume stop bit
  return b;
}

// --- Send one bit at 5 baud (200 ms per bit) ---
void bit5Baud(bool high) {
  if (high) kRelease(); else kLow();
  delay(200);
}

// --- Send address byte at 5 baud: start + 8 bits LSB-first + stop ---
void send5Baud(uint8_t addr) {
  bit5Baud(0);                            // start bit LOW
  for (int i = 0; i < 8; i++) bit5Baud((addr >> i) & 1);
  bit5Baud(1);                            // stop bit HIGH
}

// --- Checksum: sum of all bytes mod 256 ---
uint8_t checksum(const uint8_t* buf, int len) {
  uint16_t s = 0;
  for (int i = 0; i < len; i++) s += buf[i];
  return s & 0xFF;
}

// =============================================================================
void setup() {
  Serial.begin(115200);
  adcFast();
  kRelease();
  delay(1000);

  Serial.println(F("\n=== K-Line Wakeup Test ==="));
  float v = adcA3() * (2.4f * 5.0f / 1023.0f);
  Serial.print(F("Bus at start: ")); Serial.print(v, 2); Serial.print(F("V  "));
  Serial.println(busHigh() ? F("(HIGH — idle, good)") : F("(LOW — PCM driving low now)"));

  // ── Step 1: Verify we can drive the line low ────────────────────────────────
  Serial.println(F("\n[1] Drive check: pulling D3 LOW..."));
  bool canDrive = canDriveLow();
  if (canDrive) {
    Serial.println(F("    OK — bus went LOW. D3 can influence K-Line."));
  } else {
    Serial.println(F("    WARNING — bus stayed HIGH. 10k resistor may be too large."));
    Serial.println(F("    PCM may not see our LOW bits. Attempting anyway..."));
  }

  // ── Step 2: ISO 14230 Fast Init ─────────────────────────────────────────────
  Serial.println(F("\n[2] ISO 14230 Fast Init"));

  // Wait for bus to be free (idle HIGH) for at least 300ms
  Serial.print(F("    Waiting for idle bus..."));
  unsigned long t = millis();
  while (!busHigh() && millis() - t < 2000);
  delay(300);
  Serial.println(busHigh() ? F(" OK") : F(" TIMEOUT (bus stuck LOW)"));

  Serial.println(F("    Wake-up: 25ms LOW"));
  kLow(); delay(25);
  Serial.println(F("    Wake-up: 25ms HIGH"));
  kRelease(); delay(25);

  // StartCommunication request: C1 33 F1 81 [CS]
  // Format=0xC1 (physical, 1 data byte), Target=0x33 (OBD broadcast),
  // Source=0xF1 (tester), SID=0x81 (StartCommunication)
  uint8_t frame[] = { 0xC1, 0x33, 0xF1, 0x81, 0x00 };
  frame[4] = checksum(frame, 4);
  Serial.print(F("    Sending frame: "));
  for (int i = 0; i < 5; i++) {
    if (frame[i] < 0x10) Serial.print('0');
    Serial.print(frame[i], HEX); Serial.print(' ');
  }
  Serial.println();

  for (int i = 0; i < 5; i++) kSendByte(frame[i]);

  Serial.println(F("    Listening for response (1000ms)..."));
  int rxCount = 0;
  t = millis();
  while (millis() - t < 1000) {
    int b = kRecvByte(100);
    if (b >= 0) {
      Serial.print(F("    RX[")); Serial.print(rxCount++); Serial.print(F("]: 0x"));
      if (b < 0x10) Serial.print('0');
      Serial.println(b, HEX);
    }
  }
  if (rxCount == 0) Serial.println(F("    No response to fast init."));

  // ── Step 3: ISO 9141-2 5-baud Init ──────────────────────────────────────────
  Serial.println(F("\n[3] ISO 9141-2 5-baud Init (address 0x33 — ~2s)"));
  delay(300);

  Serial.print(F("    Waiting for idle bus..."));
  t = millis();
  while (!busHigh() && millis() - t < 4000);
  delay(300);
  Serial.println(busHigh() ? F(" OK") : F(" TIMEOUT"));

  Serial.println(F("    Sending 0x33 at 5 baud..."));
  send5Baud(0x33);
  kRelease();

  Serial.println(F("    Waiting for sync 0x55 (300ms)..."));
  int sync = kRecvByte(300);
  if (sync < 0) {
    Serial.println(F("    No sync byte. Trying 400ms window..."));
    sync = kRecvByte(400);
  }

  if (sync < 0) {
    Serial.println(F("    No response to 5-baud init."));
  } else {
    Serial.print(F("    Sync: 0x")); Serial.println(sync, HEX);
    if (sync == 0x55) {
      Serial.println(F("    *** SYNC 0x55 received — ECU is responding! ***"));
      int kw1 = kRecvByte(200);
      int kw2 = kRecvByte(200);
      Serial.print(F("    KW1=0x")); Serial.print(kw1 < 0 ? 0xFF : kw1, HEX);
      Serial.print(F("  KW2=0x")); Serial.println(kw2 < 0 ? 0xFF : kw2, HEX);
      if (kw2 >= 0) {
        delay(25);
        kSendByte((uint8_t)(~kw2));     // respond with inverted KW2
        int ack = kRecvByte(300);
        Serial.print(F("    ECU ACK: 0x")); Serial.println(ack < 0 ? 0xFF : ack, HEX);
        if (ack == 0xCC) {
          Serial.println(F("\n    *** K-LINE INIT COMPLETE (ISO 9141-2) ***"));
          Serial.println(F("    ECU ready for service requests."));
        }
      }
    } else {
      Serial.print(F("    Unexpected sync value: 0x")); Serial.println(sync, HEX);
      Serial.println(F("    (may be wrong baud rate, wrong protocol, or noise)"));
    }
  }

  Serial.println(F("\n=== Init attempts complete. Monitoring bus... ===\n"));
}

// =============================================================================
void loop() {
  // Passively monitor — show voltage every 500ms and flag any transitions
  static bool lastState = true;
  static unsigned long lastPrint = 0;
  bool cur = busHigh();

  if (cur != lastState) {
    float v = adcA3() * (2.4f * 5.0f / 1023.0f);
    Serial.print(millis()); Serial.print(F("ms  EDGE: "));
    Serial.print(lastState ? F("HIGH→LOW  ") : F("LOW→HIGH  "));
    Serial.print(F("V=")); Serial.println(v, 2);
    lastState = cur;
  }

  if (millis() - lastPrint >= 2000) {
    lastPrint = millis();
    float v = adcA3() * (2.4f * 5.0f / 1023.0f);
    Serial.print(F("Bus: ")); Serial.print(v, 2); Serial.println(F("V"));
  }
}
