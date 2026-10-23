// =============================================================================
// K-Line KWP2000 Probe — Grey (D16/D17) and White-Blue (D18/D19)
// Ford Ranger T5 WL93 PCM
//
// Wiring (per channel):
//   TX pin ──────────────────────────────► K-Line wire  (direct — OUTPUT LOW / INPUT)
//   K-Line wire ──[10kΩ]──┬──────────►  RX pin
//                          └──[5.1kΩ]──► GND
//   (node = 4.0V when bus is 12V idle → safe on 5V pin)
//
//   Grey      → TX=D16  RX=D17
//   White-Blue → TX=D18  RX=D19
//
// Commands (send via Serial Monitor):
//   A / B  — select channel (Grey / White-Blue)
//   i      — full init sequence (Fast Init + 5-baud fallback)
//   n      — raw edge sniff after 5-baud address
//   x      — raw Fast Init capture
//   r      — ReadECUIdent (0x1A/0x9B)
//   f      — ReadDTCsByStatus
//   m      — passive monitor 10s
//   v      — show bus state
// =============================================================================

struct KCh {
  const char* name;
  uint8_t tx;
  uint8_t rx;
};

const KCh CH_A = { "GREY",        16, 17 };
const KCh CH_B = { "WHITE-BLUE",  18, 19 };

const KCh* active = &CH_A;

const uint16_t BIT_US = 96;

void kLow    () { pinMode(active->tx, OUTPUT); digitalWrite(active->tx, LOW); }
void kRelease() { pinMode(active->tx, INPUT); }
bool kRead   () { return digitalRead(active->rx) == HIGH; }

// =============================================================================
// Bit-bang TX / RX at 10400 baud
// =============================================================================
void kSend(uint8_t b) {
  kLow(); delayMicroseconds(BIT_US);
  for (int i = 0; i < 8; i++) {
    if ((b >> i) & 1) kRelease(); else kLow();
    delayMicroseconds(BIT_US);
  }
  kRelease(); delayMicroseconds(BIT_US);
}

int kRecv(uint16_t timeout_ms) {
  unsigned long deadline = millis() + timeout_ms;
  while (kRead())  { if (millis() > deadline) return -1; }  // wait for start bit
  delayMicroseconds(BIT_US + BIT_US / 2);
  uint8_t b = 0;
  for (int i = 0; i < 8; i++) {
    if (kRead()) b |= (1 << i);
    delayMicroseconds(BIT_US - 14);
  }
  delayMicroseconds(BIT_US);
  return b;
}

void k5Baud(uint8_t addr) {
  kLow(); delay(200);
  for (int i = 0; i < 8; i++) {
    if ((addr >> i) & 1) kRelease(); else kLow();
    delay(200);
  }
  kRelease(); delay(200);
}

// =============================================================================
// Helpers
// =============================================================================
void ph(int b) { if ((byte)b < 0x10) Serial.print('0'); Serial.print((byte)b, HEX); }

uint8_t kwpCS(const uint8_t* b, int n) {
  uint16_t s = 0; for (int i = 0; i < n; i++) s += b[i]; return s & 0xFF;
}

bool waitIdle(unsigned int stable_ms, unsigned long max_ms) {
  unsigned long start = millis();
  unsigned long highSince = 0;
  bool wasHigh = false;
  while (millis() - start < max_ms) {
    if (kRead()) {
      if (!wasHigh) { highSince = millis(); wasHigh = true; }
      if (millis() - highSince >= stable_ms) return true;
    } else {
      wasHigh = false;
    }
  }
  return false;
}

void kwpSend(const uint8_t* data, int dlen, const char* label) {
  Serial.print(F("\n>> ")); Serial.print(label); Serial.print(F(":  "));
  uint8_t f[20];
  f[0] = 0x80; f[1] = 0x33; f[2] = 0xF1; f[3] = dlen;
  for (int i = 0; i < dlen; i++) f[4 + i] = data[i];
  int flen = 4 + dlen; f[flen] = kwpCS(f, flen); flen++;
  for (int i = 0; i < flen; i++) { ph(f[i]); Serial.print(' '); }
  Serial.println();
  for (int i = 0; i < flen; i++) kSend(f[i]);
  Serial.print(F("<< "));
  int n = 0; unsigned long t = millis();
  while (millis() - t < 2000) { int b = kRecv(200); if (b < 0) break; ph(b); Serial.print(' '); n++; t = millis(); }
  if (!n) Serial.print(F("(no response)"));
  Serial.println();
}

// =============================================================================
// Edge sniff buffer
// =============================================================================
struct Edge { uint32_t us; bool high; };
const int MAX_EDGES = 200;
Edge edgeBuf[MAX_EDGES];

void printEdges(int n) {
  for (int i = 0; i < n; i++) {
    Serial.print(edgeBuf[i].high ? F("  +") : F("  -"));
    Serial.print(edgeBuf[i].us); Serial.print(F("us"));
    if (i > 0) {
      uint32_t dt = edgeBuf[i].us - edgeBuf[i-1].us;
      Serial.print(F("  dt=")); Serial.print(dt);
      if      (dt < 200)    Serial.print(F("us ~10kbaud"));
      else if (dt < 2000)   Serial.print(F("us ~1200baud?"));
      else if (dt < 300000) Serial.print(F("us ~5baud range"));
    }
    Serial.println();
  }
  if (!n) Serial.println(F("  No transitions."));
  Serial.print(F("Total edges: ")); Serial.println(n);
}

void rawSniff(uint8_t addr) {
  Serial.print(F("\n=== Raw sniff  addr=0x")); ph(addr); Serial.println(F(" ==="));
  kRelease(); delay(500);
  Serial.println(F("Sending 5-baud address..."));
  k5Baud(addr); kRelease(); delay(50);
  int n = 0;
  unsigned long t0 = micros(); bool last = kRead();
  while (micros() - t0 < 2000000UL && n < MAX_EDGES) {
    bool cur = kRead();
    if (cur != last) { edgeBuf[n++] = { (uint32_t)(micros() - t0), cur }; last = cur; }
  }
  printEdges(n);
}

void rawFastInit() {
  Serial.println(F("\n=== Raw Fast Init capture ==="));
  kRelease();
  Serial.print(F("Waiting for idle HIGH... "));
  if (!waitIdle(100, 6000)) { Serial.println(F("TIMEOUT")); return; }
  Serial.println(F("OK"));
  kLow(); delay(25); kRelease(); delay(25);
  uint8_t fi[] = { 0xC1, 0x33, 0xF1, 0x81, 0x00 }; fi[4] = kwpCS(fi, 4);
  Serial.print(F("TX: "));
  for (int i = 0; i < 5; i++) { ph(fi[i]); Serial.print(' '); } Serial.println();
  for (int i = 0; i < 5; i++) kSend(fi[i]);
  int n = 0;
  unsigned long t0 = micros(); bool last = kRead();
  while (micros() - t0 < 2000000UL && n < MAX_EDGES) {
    bool cur = kRead();
    if (cur != last) { edgeBuf[n++] = { (uint32_t)(micros() - t0), cur }; last = cur; }
  }
  printEdges(n);
}

// =============================================================================
void runInit() {
  bool initDone = false;

  Serial.print(F("\n[1] Drive check  idle="));
  Serial.print(kRead() ? F("HIGH") : F("LOW")); Serial.print(F("  "));
  kLow(); delay(5);
  bool driven = !kRead();
  kRelease(); delay(50);
  Serial.println(driven ? F("bus went LOW — OK") : F("bus stayed HIGH — PCM pull-up too strong; add transistor on TX"));
  if (!driven) return;

  // Fast Init
  Serial.println(F("[2] ISO 14230 Fast Init"));
  Serial.print(F("    Waiting for idle... "));
  if (!waitIdle(100, 6000)) { Serial.println(F("TIMEOUT")); }
  else Serial.println(F("OK"));
  kLow(); delay(25); kRelease(); delay(25);
  uint8_t fi[] = { 0xC1, 0x33, 0xF1, 0x81, 0x00 }; fi[4] = kwpCS(fi, 4);
  Serial.print(F("    TX: "));
  for (int i = 0; i < 5; i++) { ph(fi[i]); Serial.print(' '); } Serial.println();
  for (int i = 0; i < 5; i++) kSend(fi[i]);
  Serial.print(F("    RX: "));
  int fn = 0; unsigned long t = millis();
  while (millis() - t < 1500) { int b = kRecv(200); if (b < 0) break; ph(b); Serial.print(' '); fn++; t = millis(); }
  if (fn) { Serial.println(); initDone = true; Serial.println(F("    Fast init response received.")); }
  else      Serial.println(F("(no response)"));

  // 5-baud init — four addresses
  uint8_t addrs[] = { 0x33, 0xD3, 0x01, 0x10 };
  const char* names[] = { "0x33 (ISO)", "0xD3 (Mazda alt)", "0x01 (Mazda)", "0x10 (Ford)" };
  for (int a = 0; a < 4 && !initDone; a++) {
    uint8_t addr = addrs[a];
    Serial.print(F("\n[3.")); Serial.print(a + 1); Serial.print(F("] 5-baud ")); Serial.println(names[a]);
    Serial.print(F("    Waiting for long idle (>1.2s)... "));
    if (!waitIdle(1200, 10000)) { Serial.println(F("TIMEOUT")); continue; }
    Serial.println(F("GO"));
    Serial.println(F("    Sending addr (~2s)..."));
    k5Baud(addr); kRelease();
    delay(50);
    Serial.print(F("    Bytes: "));
    int nb = 0; int bytes[8];
    for (int j = 0; j < 8; j++) {
      int b = kRecv(j == 0 ? 600 : 300);
      if (b < 0) break;
      ph(b); Serial.print(' ');
      bytes[nb++] = b;
    }
    if (!nb) { Serial.println(F("TIMEOUT")); continue; }
    Serial.println();
    int sync = bytes[0];
    if (sync == 0x55 && nb >= 3) {
      Serial.println(F("    *** 0x55 — KWP2000 handshake! ***"));
      int kb1 = bytes[1], kb2 = bytes[2];
      Serial.print(F("    KB1=0x")); ph(kb1); Serial.print(F("  KB2=0x")); ph(kb2); Serial.println();
      delay(35);
      uint8_t inv = ~(uint8_t)kb2;
      Serial.print(F("    TX ~KB2=0x")); ph(inv); Serial.println();
      kSend(inv);
      int ack = kRecv(400);
      Serial.print(F("    ACK=0x")); ph(ack < 0 ? 0xFF : ack); Serial.println();
      if ((uint8_t)ack == (uint8_t)~addr) {
        Serial.println(F("\n    *** INIT COMPLETE ***"));
        initDone = true;
      }
    } else {
      Serial.print(F("    Sync=0x")); ph(sync); Serial.println(F(" (not 0x55)"));
    }
  }

  if (!initDone) { Serial.println(F("\nNo successful init.")); return; }

  delay(55);
  uint8_t ident[] = { 0x1A, 0x9B };
  kwpSend(ident, 2, "ReadECUIdent 0x1A/0x9B");
  delay(55);
  uint8_t dtcs[] = { 0x18, 0x02, 0xFF, 0x00 };
  kwpSend(dtcs, 4, "ReadDTCsByStatus");
}

void monitorBus(unsigned long ms) {
  Serial.println(F("\n-- Passive monitor --"));
  unsigned long end = millis() + ms;
  bool last = kRead(); unsigned long lt = millis(); int edges = 0;
  while (millis() < end) {
    bool cur = kRead();
    if (cur != last) {
      Serial.print(cur ? F("  H ") : F("  L ")); Serial.print(millis() - lt); Serial.println(F("ms"));
      lt = millis(); last = cur; edges++;
    }
  }
  Serial.print(F("Edges: ")); Serial.println(edges);
}

// =============================================================================
void setup() {
  Serial.begin(115200);
  pinMode(CH_A.rx, INPUT);
  pinMode(CH_B.rx, INPUT);
  kRelease();
  delay(500);
  Serial.println(F("\n=== K-Line KWP2000 Probe ==="));
  Serial.print(F("Active channel: ")); Serial.println(active->name);
  Serial.print(F("GREY D17: "));       Serial.println(digitalRead(CH_A.rx) == HIGH ? F("HIGH") : F("LOW"));
  Serial.print(F("WHITE-BLUE D19: ")); Serial.println(digitalRead(CH_B.rx) == HIGH ? F("HIGH") : F("LOW"));
  Serial.println(F("Commands:  A/B=channel  i=init  n=sniff  x=fastinit  r=ident  f=faults  m=monitor  v=state"));
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch (c) {
    case 'A': active = &CH_A; Serial.print(F("Channel: ")); Serial.println(active->name); kRelease(); break;
    case 'B': active = &CH_B; Serial.print(F("Channel: ")); Serial.println(active->name); kRelease(); break;
    case 'i': runInit(); break;
    case 'n': rawSniff(0x33); break;
    case 'x': rawFastInit(); break;
    case 'r': { uint8_t d[] = { 0x1A, 0x9B }; kwpSend(d, 2, "ReadECUIdent"); break; }
    case 'f': { uint8_t d[] = { 0x18, 0x02, 0xFF, 0x00 }; kwpSend(d, 4, "ReadFaults"); break; }
    case 'm': monitorBus(10000); break;
    case 'v': Serial.print(F("Bus: ")); Serial.println(kRead() ? F("HIGH") : F("LOW")); break;
  }
}
