// =============================================================================
// PC_Wake_Leonardo — USB-HID wake device for ClusterWorx ignition-based sleep
//
// Why this board exists: the Mega 2560's only USB port runs through the 16U2
// as USB-serial (the telemetry link the Python bridge depends on), which has
// no native HID capability. A confirmed-working wake path on this PC is a
// plain USB keyboard event (see project_pc_sleep_wake memory — the Mega's
// D40/BC547 pulse onto the PC's power-switch header does NOT wake it, only a
// literal keypress does). This Leonardo (ATmega32U4, native USB) is wired on
// its OWN separate USB port to the PC and does nothing but watch the ignition
// signal and fire a keystroke when it comes on.
//
// Wiring: Mega's A14 (Black/White ignition wire, already divided down to a
// 0-5V logic-level signal by the existing resistor divider — see
// hardware_notes_voltage_dividers.txt) is tapped in parallel into this
// board's D9. Same physical signal, read independently — no connection to
// the Mega itself.
//
// IMPORTANT bench-test dependency: this board must stay powered and running
// while the PC is suspended in order to ever see the ignition-on transition
// and fire the wake keystroke. That depends on the PC's USB port keeping
// power during S3 suspend (BIOS setting, sometimes called "USB power in
// sleep" / disabling ErP). Confirm this is enabled before end-to-end testing
// — otherwise this board also loses power the moment the PC sleeps.
// =============================================================================

#include <Keyboard.h>

const int IgnitionPin = 9;

// Same debounce shape as the Mega's D40 pulse logic (project_pc_sleep_wake
// memory) — confirm the signal has been stable for N consecutive samples
// before acting, so ADC/wiring noise on the shared ignition line can't fire
// spurious wake keystrokes.
const int SAMPLE_INTERVAL_MS = 1000;
const int DEBOUNCE_COUNT     = 5;

bool ignitionOn      = true;  // assume ON at boot — board only powers up when it has power to read
int  highCount       = 0;
int  lowCount        = 0;
unsigned long lastSampleTime = 0;

void setup() {
  pinMode(IgnitionPin, INPUT);
  Keyboard.begin();
}

void loop() {
  unsigned long now = millis();
  if (now - lastSampleTime < SAMPLE_INTERVAL_MS) return;
  lastSampleTime = now;

  if (digitalRead(IgnitionPin) == HIGH) {
    highCount++;
    lowCount = 0;
  } else {
    lowCount++;
    highCount = 0;
  }

  // Ignition-on transition, confirmed — send one wake keystroke.
  // Left Ctrl alone: enough HID activity to wake the PC, but harmless if it
  // ever lands focus in a text field (no visible character typed).
  if (!ignitionOn && highCount >= DEBOUNCE_COUNT) {
    ignitionOn = true;
    Keyboard.press(KEY_LEFT_CTRL);
    delay(20);
    Keyboard.releaseAll();
  }

  // Ignition-off transition, confirmed — just re-arm for the next wake.
  if (ignitionOn && lowCount >= DEBOUNCE_COUNT) {
    ignitionOn = false;
  }
}
