// =============================================================================
// WL93 PCM K-Line KWP2000 Probe
// Ford Ranger T5 2010 (Australia/Africa/Asia) — brown/red wire
//
// Circuit:
//   D2 ──[10kΩ inline]──► BC547 base
//   BC547 collector ──────► K-Line (brown/red)
//   BC547 emitter ────────► GND
//   12V ──[10kΩ]──────────► K-Line (pull-up)
//   K-Line ──[20kΩ]──► midpoint ──[10kΩ]──► GND
//   midpoint ──► D3 (digital RX)   midpoint ──► A14 (analog RX)
//
// Commands:  i=init  n=sniff  m=monitor  v=voltage
// =============================================================================

#define K_TX    2
#define K_RX    3
#define K_RX_A  A14
#define BIT_US  96
#define SCALE   (3.0f * 5.0f / 1023.0f)

// NPN: D2 HIGH = NPN ON = K-Line LOW (dominant)
//      D2 LOW  = NPN OFF = K-Line HIGH (recessive via pull-up)
void kLow()     { digitalWrite(K_TX, HIGH); }
void kRelease() { digitalWrite(K_TX, LOW);  }
bool kRead()    { return digitalRead(K_RX); }
float kVolt()   { return analogRead(K_RX_A) * SCALE; }

// --- TX: byte at 10400 baud ---
void kSend(uint8_t b) {
  kLow(); delayMicroseconds(BIT_US);
  for (int i = 0; i < 8; i++) {
    if ((b >> i) & 1) kRelease(); else kLow();
    delayMicroseconds(BIT_US);
  }
  kRelease(); delayMicroseconds(BIT_US);
}

// --- RX: byte at 10400 baud ---
// Waits for bus to go HIGH first (idle), then waits for falling edge (start bit).
// This prevents false-triggering when K-Line is already LOW at entry.
int kRecv(unsigned int timeout_ms) {
  unsigned long deadline = millis() + timeout_ms;

  // Step 1: wait for K-Line to be HIGH (bus idle)
  while (!kRead()) {
    if (millis() > deadline) return -1;
  }
  // Step 2: wait for falling edge (start bit)
  while (kRead()) {
    if (millis() > deadline) return -1;
  }
  // Step 3: sample 8 data bits
  delayMicroseconds(BIT_US + BIT_US / 2);   // centre of bit 0
  uint8_t b = 0;
  for (int i = 0; i < 8; i++) {
    if (kRead()) b |= (1 << i);
    delayMicroseconds(BIT_US - 14);
  }
  delayMicroseconds(BIT_US);
  return b;
}

// --- TX: byte at 5 baud ---
void k5Baud(uint8_t addr) {
  kLow();   delay(200);
  for (int i = 0; i < 8; i++) {
    if ((addr >> i) & 1) kRelease(); else kLow();
    delay(200);
  }
  kRelease(); delay(200);
}

uint8_t kwpCS(const uint8_t* b, int n) {
  uint16_t s = 0; for (int i=0;i<n;i++) s+=b[i]; return s&0xFF;
}
void ph(int b) { if((byte)b<0x10) Serial.print('0'); Serial.print((byte)b,HEX); }

void kwpSend(const uint8_t* data, int dlen, const char* label) {
  Serial.print(F("\n>> ")); Serial.print(label); Serial.print(F(":  "));
  uint8_t f[20];
  f[0]=0x80; f[1]=0x33; f[2]=0xF1; f[3]=dlen;
  for(int i=0;i<dlen;i++) f[4+i]=data[i];
  int flen=4+dlen; f[flen]=kwpCS(f,flen); flen++;
  for(int i=0;i<flen;i++){ph(f[i]);Serial.print(' ');}
  Serial.println();
  for(int i=0;i<flen;i++) kSend(f[i]);
  Serial.print(F("<< "));
  int n=0; unsigned long t=millis();
  while(millis()-t<2000){int b=kRecv(200);if(b<0)break;ph(b);Serial.print(' ');n++;t=millis();}
  if(!n) Serial.print(F("(no response)"));
  Serial.println();
}

// =============================================================================
// Raw sniff — buffers ALL edges during 2s window (no Serial during capture)
// then prints. Catches 10400 baud transitions that Serial.print would miss.
// =============================================================================
struct Edge { uint32_t us; bool high; };
const int MAX_EDGES = 200;
Edge edgeBuf[MAX_EDGES];

void rawSniff(uint8_t addr) {
  Serial.print(F("\n=== Raw sniff  addr=0x")); ph(addr); Serial.println(F(" ==="));
  kRelease(); delay(500);
  Serial.println(F("Sending 5-baud address..."));
  k5Baud(addr);
  kRelease();

  delay(50);   // brief W1 gap then capture

  // --- capture phase: NO Serial calls ---
  int n = 0;
  unsigned long t0 = micros();
  bool last = kRead();

  while (micros() - t0 < 2000000UL && n < MAX_EDGES) {
    bool cur = kRead();
    if (cur != last) {
      edgeBuf[n++] = { (uint32_t)(micros() - t0), cur };
      last = cur;
    }
  }
  bool initHigh = !edgeBuf[0].high;   // first edge is a transition FROM initHigh

  // --- print phase ---
  Serial.print(F("Initial state: ")); Serial.println(initHigh ? F("HIGH") : F("LOW"));
  for (int i = 0; i < n; i++) {
    Serial.print(edgeBuf[i].high ? F("  +") : F("  -"));
    Serial.print(edgeBuf[i].us);
    Serial.print(F("us"));
    if (i > 0) {
      uint32_t dt = edgeBuf[i].us - edgeBuf[i-1].us;
      Serial.print(F("  (dt="));  Serial.print(dt);
      Serial.print(F("us"));
      // baud rate hint
      if (dt < 200)       Serial.print(F(" ~10kbaud"));
      else if (dt < 2000) Serial.print(F(" ~1200baud?"));
      else if (dt < 300000) Serial.print(F(" ~5baud range"));
      Serial.print(F(")"));
    }
    Serial.println();
  }
  if (n == 0) Serial.println(F("  No transitions."));
  Serial.print(F("Total edges: ")); Serial.println(n);
}

void monitorBus(unsigned long ms) {
  Serial.println(F("\n-- Passive monitor --"));
  unsigned long end=millis()+ms;
  bool last=kRead(); unsigned long lt=millis(); int edges=0;
  while(millis()<end){
    bool cur=kRead();
    if(cur!=last){
      Serial.print(cur?F("  H "):F("  L "));
      Serial.print(millis()-lt); Serial.println(F("ms"));
      lt=millis(); last=cur; edges++;
    }
  }
  Serial.print(F("Edges: ")); Serial.println(edges);
}

// =============================================================================
bool initDone = false;

// Wait for K-Line to go HIGH for at least 'ms' consecutive milliseconds.
// Returns true if successful, false on timeout (max_wait_ms total).
bool waitIdleHigh(unsigned int stable_ms, unsigned long max_wait_ms) {
  unsigned long start = millis();
  unsigned long highSince = 0;
  bool wasHigh = false;
  while (millis() - start < max_wait_ms) {
    if (kRead()) {
      if (!wasHigh) { highSince = millis(); wasHigh = true; }
      if (millis() - highSince >= stable_ms) return true;
    } else {
      wasHigh = false;
    }
  }
  return false;
}

void runInit() {
  initDone = false;

  Serial.print(F("\n[1] Drive test  idle="));
  Serial.print(kVolt(),2); Serial.print(F("V  "));
  kLow(); delay(5);
  float vDriven = kVolt();
  kRelease(); delay(5);
  Serial.print(F("driven="));  Serial.print(vDriven,2); Serial.print(F("V  "));
  kRelease(); delay(50);
  unsigned long tw = millis();
  while (!kRead() && millis()-tw < 500);
  Serial.println(kRead() ? F("OK (recovered HIGH)") : F("WARN (K-Line not recovering HIGH — check pull-up)"));

  // ── Wait for idle HIGH window before Fast Init ─────────────────────────────
  Serial.print(F("\n[2] Waiting for idle HIGH window..."));
  if (!waitIdleHigh(100, 6000)) { Serial.println(F("TIMEOUT — bus stuck LOW")); }
  else Serial.println(F("  OK"));

  // ── ISO 14230 Fast Init ───────────────────────────────────────────────────
  Serial.println(F("[2] ISO 14230 Fast Init"));
  kLow(); delay(25); kRelease(); delay(25);
  uint8_t fi[]={0xC1,0x33,0xF1,0x81,0x00};
  fi[4]=kwpCS(fi,4);
  Serial.print(F("    TX: "));
  for(int i=0;i<5;i++){ph(fi[i]);Serial.print(' ');}Serial.println();
  for(int i=0;i<5;i++) kSend(fi[i]);
  Serial.print(F("    RX: "));
  int fn=0; unsigned long t=millis();
  while(millis()-t<1500){int b=kRecv(200);if(b<0)break;ph(b);Serial.print(' ');fn++;t=millis();}
  if(fn){Serial.println();initDone=true;Serial.println(F("    Fast init response received."));}
  else  {Serial.println(F("(no response)"));}

  // ── 5-baud init — four addresses ─────────────────────────────────────────
  uint8_t addrs[]={0x33,0xD3,0x01,0x10};
  const char* names[]={"0x33 (ISO)","0xD3 (Mazda alt)","0x01 (Mazda)","0x10 (Ford)"};

  for(int a=0;a<4&&!initDone;a++){
    uint8_t addr=addrs[a];
    Serial.print(F("\n[3.")); Serial.print(a+1);
    Serial.print(F("] 5-baud  ")); Serial.println(names[a]);

    kRelease();
    Serial.print(F("    Waiting for long idle HIGH (>1.2s)..."));
    if (!waitIdleHigh(1200, 10000)) { Serial.println(F("TIMEOUT")); continue; }
    Serial.println(F("  GO"));
    Serial.println(F("    Sending address (~2s)..."));
    k5Baud(addr); kRelease();

    delay(50);    // brief W1 — PCM responds ~400ms after stop bit (measured)

    Serial.print(F("    Bytes: "));
    int nb=0; int bytes[8];
    for(int j=0;j<8;j++){
      int b=kRecv(j==0 ? 600 : 300);   // first byte: longer wait (PCM at ~414ms)
      if(b<0) break;
      ph(b); Serial.print(' ');
      bytes[nb++]=b;
    }
    if(!nb){Serial.println(F("TIMEOUT"));continue;}
    Serial.println();

    int sync=bytes[0];
    if(sync==0x55 && nb>=3){
      Serial.println(F("    *** 0x55 — KWP2000 handshake! ***"));
      int kb1=bytes[1], kb2=bytes[2];
      Serial.print(F("    KB1=0x"));ph(kb1);
      Serial.print(F("  KB2=0x"));ph(kb2);Serial.println();
      delay(35);
      uint8_t inv=~(uint8_t)kb2;
      Serial.print(F("    TX ~KB2=0x"));ph(inv);Serial.println();
      kSend(inv);
      int ack=kRecv(400);
      Serial.print(F("    ACK=0x"));ph(ack<0?0xFF:ack);Serial.println();
      if((uint8_t)ack==(uint8_t)~addr){
        Serial.println(F("\n    *** INIT COMPLETE ***"));
        initDone=true;
      }
    } else {
      Serial.print(F("    Sync=0x"));ph(sync);
      Serial.println(F(" (not 0x55) — send 'n' for raw edge sniff"));
    }
  }

  if(!initDone){
    Serial.println(F("\nNo successful init."));
    Serial.println(F("  'n' = raw edge sniff  'm' = monitor  'v' = voltage  'i' = retry"));
    return;
  }

  delay(55);
  uint8_t ident[]={0x1A,0x9B};
  kwpSend(ident,2,"ReadECUIdent 0x1A/0x9B");
  delay(55);
  uint8_t dtcs[]={0x18,0x02,0xFF,0x00};
  kwpSend(dtcs,4,"ReadDTCsByStatus");
  Serial.println(F("\nCommands: i r f m n v"));
}

// =============================================================================
void setup() {
  Serial.begin(115200);
  pinMode(K_TX, OUTPUT);
  kRelease();
  delay(500);
  Serial.println(F("\n=== WL93 K-Line Probe ==="));
  Serial.print(F("K-Line: ")); Serial.print(kVolt(),2);
  Serial.println(kRead() ? F("V  HIGH") : F("V  LOW"));
  Serial.println(F("Commands: i=init  n=sniff  x=fastinit  r=ident  f=faults  m=monitor  v=voltage  z=pintest"));
}

// Fast Init then raw-capture (no Serial during capture) to see 10400baud response
void rawFastInit() {
  Serial.println(F("\n=== Raw Fast Init capture ==="));
  kRelease();
  Serial.print(F("Waiting for idle HIGH..."));
  if (!waitIdleHigh(100, 6000)) { Serial.println(F("TIMEOUT")); return; }
  Serial.println(F("  OK"));

  kLow(); delay(25); kRelease(); delay(25);
  uint8_t fi[]={0xC1,0x33,0xF1,0x81,0x00}; fi[4]=kwpCS(fi,4);
  Serial.print(F("TX: "));
  for(int i=0;i<5;i++){ph(fi[i]);Serial.print(' ');}Serial.println();
  for(int i=0;i<5;i++) kSend(fi[i]);

  // --- buffered capture (no Serial) ---
  int n = 0;
  unsigned long t0 = micros();
  bool last = kRead();
  while (micros() - t0 < 2000000UL && n < MAX_EDGES) {
    bool cur = kRead();
    if (cur != last) { edgeBuf[n++] = {(uint32_t)(micros()-t0), cur}; last = cur; }
  }

  // --- print ---
  Serial.print(F("Init state: ")); Serial.println(last ? F("HIGH") : F("LOW"));
  for (int i = 0; i < n; i++) {
    Serial.print(edgeBuf[i].high ? F("  +") : F("  -"));
    Serial.print(edgeBuf[i].us); Serial.print(F("us"));
    if (i > 0) {
      uint32_t dt = edgeBuf[i].us - edgeBuf[i-1].us;
      Serial.print(F("  dt=")); Serial.print(dt);
      if (dt < 200)       Serial.print(F("us ~10kbaud"));
      else if (dt < 1000) Serial.print(F("us ~1kbaud?"));
      else if (dt < 300000) Serial.print(F("us ~5baud"));
    }
    Serial.println();
  }
  if (!n) Serial.println(F("  No transitions."));
  Serial.print(F("Edges: ")); Serial.println(n);
}

// D2 pin drive test — toggle NPN ON/OFF, read K-Line voltage both ways
void pinTest() {
  Serial.println(F("\n=== D2 / NPN drive test ==="));

  kRelease();  // D2=LOW → NPN OFF → K-Line should float HIGH via pull-up
  delay(50);
  float vOff = kVolt();
  bool  dOff = kRead();
  Serial.print(F("  NPN OFF (D2=LOW):  ")); Serial.print(vOff,2);
  Serial.println(dOff ? F("V  HIGH  (pull-up working)") : F("V  LOW   (pull-up missing or NPN stuck)"));

  kLow();   // D2=HIGH → NPN ON → K-Line should be pulled to GND (~0V)
  delay(50);
  float vOn = kVolt();
  bool  dOn = kRead();
  Serial.print(F("  NPN ON  (D2=HIGH): ")); Serial.print(vOn,2);
  Serial.println(dOn ? F("V  HIGH  (NPN not switching — base open?)") : F("V  LOW   (NPN switching correctly)"));

  kRelease();  // leave idle
  Serial.println(F("  D2 restored LOW (NPN OFF)."));

  if (vOff > 8.0f && vOn < 2.0f)
    Serial.println(F("  RESULT: D2 + NPN both functional. Pull-up on collector confirmed."));
  else if (vOff < 2.0f && vOn < 2.0f)
    Serial.println(F("  RESULT: K-Line stuck LOW. Pull-up likely still on base (not collector)."));
  else if (vOff > 8.0f && vOn > 8.0f)
    Serial.println(F("  RESULT: NPN not switching. Check base connection from D2."));
  else
    Serial.println(F("  RESULT: Partial — check wiring."));
}

// Hold D2 HIGH (NPN ON) for 10s — measure base and D2 pin with multimeter
void holdHigh() {
  Serial.println(F("\n=== HOLD: D2=HIGH (NPN ON) for 10s ==="));
  Serial.println(F("  Measure brown/red (K-Line) with multimeter — expect ~0V."));
  Serial.println(F("  D3=HIGH means K-Line not pulled LOW. D3=LOW means NPN working."));
  kLow();
  for (int i = 10; i > 0; i--) {
    Serial.print(F("  ")); Serial.print(i);
    Serial.print(F("s  D3=")); Serial.print(kRead() ? F("HIGH") : F("LOW"));
    Serial.print(F("  K-Line=")); Serial.print(kVolt(), 2); Serial.println(F("V"));
    delay(1000);
  }
  kRelease();
  Serial.println(F("  Released. D2=LOW, NPN OFF."));
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();
  switch(c){
    case 'i': runInit(); break;
    case 'n': rawSniff(0x33); break;
    case 'x': rawFastInit(); break;
    case 'r': {uint8_t d[]={0x1A,0x9B};kwpSend(d,2,"ReadECUIdent");break;}
    case 'f': {uint8_t d[]={0x18,0x02,0xFF,0x00};kwpSend(d,4,"ReadFaults");break;}
    case 'm': monitorBus(10000); break;
    case 'M': monitorBus(20000); break;
    case 'v': Serial.print(F("K-Line: "));Serial.print(kVolt(),2);Serial.println(F("V")); break;
    case 'z': pinTest(); break;
    case 'h': holdHigh(); break;
  }
}
