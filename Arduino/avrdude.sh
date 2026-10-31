#!/bin/bash
# Hold the Mega (ATmega2560) in programming mode via the UNO running ArduinoISP,
# so the RESET pin can be measured (should read ~0 V). Type "quit" to release.
# -F only skips the signature check; nothing is written.
PORT=${1:-/dev/ttyACM0}
avrdude -c stk500v1 -p m2560 -P "$PORT" -b 19200 -F -t
