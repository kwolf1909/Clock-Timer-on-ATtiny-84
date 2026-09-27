This is a timer using a 7-seg I2C-based display, a rotary encoder, a buzzer, and a DS18B20 temp-sensor.
The MCU used is an ATtiny84.
The timer is setup using the rotary encoder with push button.
If timer is not running, the temperature is displayed.
During countdown, timer beeps indicate the remaining five minutes.
A long press on the push button enables oscillator calibration mode. On buzzer output pin, a 1 KHz signal is output, which can be calibrated with rotary encoder (internally setting the OSCCAL-value) using a scope or frequency counter. The selected value is stored in EEPROM and used during boot.
