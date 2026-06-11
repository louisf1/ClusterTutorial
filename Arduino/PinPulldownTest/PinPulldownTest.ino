// Diagnostic sketch — pulls D5-D13 and D30-D41 LOW (driven OUTPUT) and does nothing else.
// Used to bench-test floating voltage on these pins with a multimeter.
//
// Results 2026-06-11 (nothing connected to any of these pins):
//   D5-D13, D32, D34-D41: pulled to 0V correctly — pins OK
//   D30, D31, D33: stuck at 4.7V despite OUTPUT LOW — DEAD, do not use

const int pins[] = {5, 6, 7, 8, 9, 10, 11, 12, 13,
                     30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41};
const int numPins = sizeof(pins) / sizeof(pins[0]);

void setup() {
  for (int i = 0; i < numPins; i++) {
    pinMode(pins[i], OUTPUT);
    digitalWrite(pins[i], LOW);
  }
}

void loop() {
  // nothing — pins held LOW
}
