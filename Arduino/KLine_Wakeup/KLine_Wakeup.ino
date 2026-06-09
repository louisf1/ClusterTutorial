// =============================================================================
// K-Line Wakeup — ISO 14230 Fast Init + ISO 9141-2 5-baud Init
// Two channels: Grey (D16 TX / D17 RX) and White-Blue (D18 TX / D19 RX)
//
// Wiring (per channel):
//   TX pin ──────────────────────────────► K-Line wire  (direct — OUTPUT LOW to drive, INPUT to release)
//   K-Line wire ──[10kΩ]──┬──────────►  RX pin
//                          └──[5.1kΩ]──► GND
//   (node = 4.0V when bus is 12V idle → safe on 5V pin; 0V when LOW)
//
//   If the drive-check reports the bus won't go LOW, the PCM pull-up is too
//   strong for direct drive — add a BC547 on TX.
//
// Open Serial Monitor at 115200 baud.
// =============================================================================

struct KCh {
  const char* name;
  uint8_t tx;
  uint8_t rx;
};

const KCh CH_A = { "GREY",       16, 17 };
const KCh CH_B = { "WHITE-BLUE", 18, 19 };

const uint16_t BIT_US = 96;   // 1 000 000 / 10400 = 96.15 µs

void kLow    (const KCh& c) { pinMode(c.tx, OUTPUT); digitalWrite(c.tx, LOW); }
void kRelease(const KCh& c) { pinMode(c.tx, INPUT); }
bool busHigh (const KCh& c) { return digitalRead(c.rx) == HIGH; }

void kSendByte(const KCh& c, uint8_t b) {
  kLow(c); delayMicroseconds(BIT_US);
  for (int i = 0; i < 8; i++) {
    if (b & 1) kRelease(c); else kLow(c);
    b >>= 1;
    delayMicroseconds(BIT_US);
  }
  kRelease(c); delayMicroseconds(BIT_US);
}

int kRecvByte(const KCh& c, uint16_t timeout_ms) {
  unsigned long deadline = millis() + timeout_ms;
  while (busHigh(c)) { if (millis() > deadline) return -1; }
  delayMicroseconds(BIT_US + BIT_US / 2);
  uint8_t b = 0;
  for (int i = 0; i < 8; i++) {
    if (busHigh(c)) b |= (1 << i);
    delayMicroseconds(BIT_US - 13);
  }
  delayMicroseconds(BIT_US);
  return b;
}

void k5Baud(const KCh& c, uint8_t addr) {
  kLow(c); delay(200);
  for (int i = 0; i < 8; i++) {
    if ((addr >> i) & 1) kRelease(c); else kLow(c);
    delay(200);
  }
  kRelease(c); delay(200);
}

uint8_t chksum(const uint8_t* buf, int len) {
  uint16_t s = 0;
  for (int i = 0; i < len; i++) s += buf[i];
  return s & 0xFF;
}

void ph(uint8_t b) { if (b < 0x10) Serial.print('0'); Serial.print(b, HEX); }

void testChannel(const KCh& c) {
  Serial.print(F("\n=== ")); Serial.print(c.name);
  Serial.print(F("  TX=D")); Serial.print(c.tx);
  Serial.print(F("  RX=D")); Serial.print(c.rx); Serial.println(F(" ==="));

  Serial.print(F("Bus idle: ")); Serial.println(busHigh(c) ? F("HIGH (good)") : F("LOW — check wiring"));

  // Drive check — can we actually pull the bus LOW?
  kLow(c); delayMicroseconds(500);
  bool canDrive = !busHigh(c);
  kRelease(c); delayMicroseconds(500);
  Serial.print(F("Drive check: "));
  Serial.println(canDrive ? F("OK (bus went LOW)") : F("FAIL — bus stayed HIGH; PCM pull-up too strong, add transistor on TX"));
  if (!canDrive) return;

  // ISO 14230 Fast Init
  Serial.println(F("[1] ISO 14230 Fast Init"));
  unsigned long t = millis();
  while (!busHigh(c) && millis() - t < 2000);
  delay(300);
  kLow(c); delay(25);
  kRelease(c); delay(25);
  uint8_t fi[] = { 0xC1, 0x33, 0xF1, 0x81, 0x00 };
  fi[4] = chksum(fi, 4);
  Serial.print(F("    TX: "));
  for (int i = 0; i < 5; i++) { ph(fi[i]); Serial.print(' '); }
  Serial.println();
  for (int i = 0; i < 5; i++) kSendByte(c, fi[i]);
  int n = 0;
  t = millis();
  while (millis() - t < 1000) {
    int b = kRecvByte(c, 100);
    if (b >= 0) { Serial.print(F("    RX: 0x")); ph(b); Serial.println(); n++; }
  }
  if (!n) Serial.println(F("    No response."));

  // ISO 9141-2 5-baud Init
  Serial.println(F("[2] ISO 9141-2 5-baud Init (addr 0x33)"));
  delay(300);
  t = millis();
  while (!busHigh(c) && millis() - t < 4000);
  delay(300);
  k5Baud(c, 0x33);
  int sync = kRecvByte(c, 500);
  if (sync < 0) sync = kRecvByte(c, 500);
  if (sync < 0) {
    Serial.println(F("    No response."));
    return;
  }
  Serial.print(F("    Sync: 0x")); ph(sync); Serial.println();
  if (sync == 0x55) {
    Serial.println(F("    *** 0x55 — ECU responding! ***"));
    int kw1 = kRecvByte(c, 200);
    int kw2 = kRecvByte(c, 200);
    Serial.print(F("    KW1=0x")); ph(kw1 < 0 ? 0xFF : kw1);
    Serial.print(F("  KW2=0x")); ph(kw2 < 0 ? 0xFF : kw2); Serial.println();
    if (kw2 >= 0) {
      delay(25);
      kSendByte(c, (uint8_t)(~kw2));
      int ack = kRecvByte(c, 300);
      Serial.print(F("    ACK: 0x")); ph(ack < 0 ? 0xFF : ack); Serial.println();
      if (ack == 0xCC) Serial.println(F("    *** INIT COMPLETE (ISO 9141-2) ***"));
    }
  } else {
    Serial.print(F("    Unexpected sync (not 0x55). Raw byte: 0x")); ph(sync); Serial.println();
  }
}

// =============================================================================
void setup() {
  Serial.begin(115200);
  pinMode(CH_A.rx, INPUT);
  pinMode(CH_B.rx, INPUT);
  kRelease(CH_A);
  kRelease(CH_B);
  delay(1000);
  Serial.println(F("\n=== K-Line Wakeup ==="));
  testChannel(CH_A);
  testChannel(CH_B);
  Serial.println(F("\n=== Done. Watching bus edges... ==="));
}

void loop() {
  static bool lastA = true, lastB = true;
  static unsigned long lastPrint = 0;
  bool curA = busHigh(CH_A);
  bool curB = busHigh(CH_B);
  if (curA != lastA) {
    Serial.print(F("GREY       ")); Serial.println(curA ? F("HIGH") : F("LOW"));
    lastA = curA;
  }
  if (curB != lastB) {
    Serial.print(F("WHITE-BLUE ")); Serial.println(curB ? F("HIGH") : F("LOW"));
    lastB = curB;
  }
  if (millis() - lastPrint >= 3000) {
    lastPrint = millis();
    Serial.print(F("GREY:")); Serial.print(busHigh(CH_A) ? F("H ") : F("L "));
    Serial.print(F("WHITE-BLUE:")); Serial.println(busHigh(CH_B) ? F("H") : F("L"));
  }
}
