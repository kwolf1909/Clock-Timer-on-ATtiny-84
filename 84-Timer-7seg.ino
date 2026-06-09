#include <Wire.h>
#include <OneWire.h>
#include <DS18B20_INT.h>
#include <RTClib.h>
#include "display.h"

//#define DEBUGSERIAL

// -------------------------- rotary encoder  -----------------------------------------

volatile bool button;
volatile uint8_t a0;
volatile uint8_t b0;
volatile uint8_t s0;
volatile uint8_t Rotary, newRotary;

#define ENCODER_A   0   // Pin PB0
#define ENCODER_B   1   // Pin PB1
#define ENCODER_S   2   // Pin PB2

// ----------------------------- globals ----------------------------------------------

#define ONEWIRE_PIN         3       // Pin PA3
#define BUZZER_PIN          7       // Pin PA7
#define BUZZER_WAIT         200

#define PUSH_DEBOUNCE_DELAY 500
#define TEMP_DELAY          5000
#define TEMP_REQDELAY       1000
#define FLASH1_DELAY        250
#define FLASH2_DELAY        125
#define BEEP_COUNT          80
#define TIMER_TICK          500

bool bNewRotary, bShow, bColon;
uint8_t timerMinutes, timerSeconds, state, min, sec, prevRotary, beepCount;
int16_t temp;
uint32_t curTime, lastPushTime, lastTempTime, lastTempReqTime, lastFlashTime, lastTimer;

volatile bool bBuzzer;
volatile uint8_t buzzerCount;
volatile uint16_t buzzerDuration;

enum { STATE_REQTEMP = 1, STATE_REQTEMPWAIT, STATE_SHOWTEMP, STATE_WAITTEMP, STATE_SELMIN, STATE_SELSEC, STATE_RUNTIMER, STATE_TIMERBEEP };

DateTime dt(2026, 1, 1, 0, 0, 0);
OneWire oneWire(ONEWIRE_PIN);
DS18B20_INT sensor(&oneWire);

#ifdef DEBUGSERIAL
char sDebug[64];
#endif

// ------------------------------- Setup ------------------------------------

void setup() {

  delay(100);
#ifdef DEBUGSERIAL
  //Serial.setTxBit(PB0);
  Serial.begin(9600);
  Serial.println("Init...");
#endif

  // internal oszillator calibration to 8.00 MHz
  OSCCAL = 113;

  //TCCR0A = 1 << COM0A0 | 1 << WGM01;  // Toogle OC0A, CTC
  //TCCR0B = 1 << CS00;                 // no pre-scaling
  //OCR0A = 0;
  //DDRB |= 1 << 2;                     // port PB2 output
  //while(1);

  Wire.begin();

  initDisplay(DISPLAY_ADDRESS, DISPLAY_BRIGHTNESS);
  blinkRate(HT16K33_BLINK_OFF);

  // Write "----" to the display
  Wire.beginTransmission(DISPLAY_ADDRESS);
  Wire.write(0);

  writeWord(SYMBOL_MINUS);
  writeWord(SYMBOL_MINUS);
  writeWord(false);
  writeWord(SYMBOL_MINUS);
  writeWord(SYMBOL_MINUS);

  Wire.endTransmission();

  // setup input pins with pull-ups
  DDRB &= ~(1 << ENCODER_A | 1 << ENCODER_B | 1 << ENCODER_S);
  PORTB |= (1 << ENCODER_A | 1 << ENCODER_B | 1 << ENCODER_S);

  // init rotary decoder PCINTs
  PCMSK1 = (1 << ENCODER_A | 1 << ENCODER_S);
  //PCMSK1 = (1 << ENCODER_S);
  GIMSK = 1 << PCIE1;
  GIFR = 1 << PCIF1;

  // Set output pin PA7 for buzzer
  DDRA |= 1 << BUZZER_PIN;

  button = false;
  Rotary = 0;
  newRotary = 0;
  timerMinutes = 0;
  timerSeconds = 0;
  bBuzzer = false;
  state = STATE_REQTEMP;

  // Initialize Timer1 for 1000 Hz interrupt (buzzer...)
  setupTimer1();

  buzzer(100, 2);
  delay(1000);

  sensor.begin();
  sensor.setResolution(11);

#ifdef DEBUGSERIAL
  // Read Vcc
  uint16_t volt = ReadVCC();
  sprintf(sDebug, "Voltage: %u mV", volt);
  Serial.println(sDebug);
#endif
}

// ------------------------------- Loop -------------------------------------

void loop () {

  curTime = millis();

  min = dt.minute();
  sec = dt.second();

  if (prevRotary != newRotary) {
    prevRotary = newRotary;
    bNewRotary = true;
  }

  //------------------- handle push button ---------------------------
  if (button) {
    // software debounce
    if (curTime - lastPushTime > PUSH_DEBOUNCE_DELAY) {
      lastPushTime = curTime;

      // valid push detected
#ifdef DEBUGSERIAL
      Serial.println("Rotary Push detected.");
#endif
      state = handleButton(state);
    }
    button = false;
  }

  //------------------ process state machine -------------------------
  switch (state) {
    case STATE_REQTEMP:
#ifdef DEBUGSERIAL
      Serial.println("Requesting temperature...");
#endif
      sensor.requestTemperatures();
      lastTempReqTime = curTime;
      state = STATE_REQTEMPWAIT;
      break;

    case STATE_REQTEMPWAIT:
      // check for conversion result
      if (curTime - lastTempReqTime > TEMP_REQDELAY) {
        if (!sensor.isConversionComplete()) {
          // conversion not complete
          lastTempReqTime = curTime;
          break;
        }
        state = STATE_SHOWTEMP;
      }
      break;

    case STATE_SHOWTEMP:
      temp = sensor.getTempCentiC();

#ifdef DEBUGSERIAL
      sprintf(sDebug, "Temperature: %d C", temp);
      Serial.println(sDebug);
#endif
      showDisplay (temp / 1000, (temp / 100) % 10, (temp / 10) % 10, 0, 0xF, false, true);
      lastTempTime = curTime;
      state = STATE_WAITTEMP;
      break;

    case STATE_WAITTEMP:
      if (curTime - lastTempTime > TEMP_DELAY) state = STATE_REQTEMP;
      break;

    case STATE_SELMIN:
      // grab new value
      if (bNewRotary) {
        timerMinutes = newRotary;
        bShow = true;
        buzzer(50, 1);
      }
      // flash minutes
      if (curTime - lastFlashTime > FLASH1_DELAY || bNewRotary) {
        lastFlashTime = curTime;
        if (bShow) showDisplay(timerMinutes / 10, timerMinutes % 10, 0, 0, 0xF, true, false);  // show minutes
        else       showDisplay(0, 0, 0, 0, 0x3, true, false);                                  // blank minutes
        bShow = !bShow;
        bNewRotary = false;
      }
      break;

    case STATE_SELSEC:
      // grab new value
      if (bNewRotary) {
        timerSeconds = newRotary;
        bShow = true;
        buzzer(50, 1);
      }
      // flash minutes
      if (curTime - lastFlashTime > FLASH1_DELAY || bNewRotary) {
        lastFlashTime = curTime;
        if (bShow) showDisplay(timerMinutes / 10, timerMinutes % 10, timerSeconds / 10, timerSeconds % 10, 0xF, true, false);  // show seconds
        else       showDisplay(timerMinutes / 10, timerMinutes % 10, 0, 0, 0xC, true, false);                                  // blank seconds
        bShow = !bShow;
        bNewRotary = false;
      }
      break;

    case STATE_RUNTIMER:
      // update every half second
      if (curTime > lastTimer) {
        lastTimer += TIMER_TICK;
        if (showTimer(bColon) == false) state = STATE_TIMERBEEP;
        bColon = !bColon;
      }
      break;

    case STATE_TIMERBEEP:
      if (curTime - lastFlashTime > FLASH2_DELAY) {
        lastFlashTime = curTime;
        if (bShow) showDisplay(0, 0, 0, 0, 0xF, true, false);
        else       showDisplay(0, 0, 0, 0, 0x0, false, false);
        bShow = !bShow;

        buzzer(100, 1);

        // exit after number of beeps
        if (--beepCount == 0) {
          state = STATE_SHOWTEMP;
          delay (1000);
        }
      }
      break;
  }
  delay(10);
}

//----------------------- helper function -----------------------------

uint8_t handleButton(uint8_t curState) {
  //setupTimer1();
  buzzer(50, 1);

  switch (curState) {
    case STATE_SHOWTEMP:
    case STATE_WAITTEMP:
      Rotary = 0;
      bShow = true;
      timerMinutes = 0;
      timerSeconds = 0;
      buzzer(50, 1);
      return STATE_SELMIN;

    case STATE_SELMIN:
      Rotary = 0;
      bShow = true;
      buzzer(50, 1);
      return STATE_SELSEC;

    case STATE_SELSEC:
      if (timerMinutes || timerSeconds) {
        // set timer start condition
        dt = DateTime(2026, 1, 1, 0, timerMinutes, timerSeconds);
        lastTimer = curTime + TIMER_TICK;
        buzzer(50, 1);
        bColon = true;
        return STATE_RUNTIMER;
      }
      else {
        buzzer(50, 1);
        delay(100);
        return STATE_REQTEMP;
      }
      break;

    case STATE_RUNTIMER:
      timerMinutes = 0;
      timerSeconds = 0;
      buzzer(50, 1);
      delay(100);
      return STATE_REQTEMP;

    case STATE_TIMERBEEP:
      delay(100);
      return STATE_REQTEMP;
  }
  return STATE_REQTEMP;
}

bool showTimer(bool colon) {
  if (colon) {
    showDisplay(dt.minute() / 10, dt.minute() % 10, dt.second() / 10, dt.second() % 10, 0xF, true, false);

#ifdef DEBUGSERIAL
    // print timer value
    sprintf(sDebug, "Timer: %02u:%02u", dt.minute(), dt.second());
    Serial.println(sDebug);
#endif
    // exit condition
    if (!dt.minute() && !dt.second()) {
      beepCount = BEEP_COUNT;
      return false;
    }

    // trigger minute beeps
    if (dt.second() == 0) {
      if (dt.minute() <= 5) {
        buzzer(100, dt.minute());
      }
    }
    // trigger seconds beeps
    if (dt.minute() == 0) {
      if (dt.second() && dt.second() <= 10) {
        buzzer(100, 1);
      }
    }
  }
  else {
    showDisplay(dt.minute() / 10, dt.minute() % 10, dt.second() / 10, dt.second() % 10, 0xF, false, false);

    // decrease second
    dt = dt - (TimeSpan)1;
  }
  return true;
}

uint16_t ReadVCC(void) {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  ADMUX =  _BV(MUX5) | _BV(MUX0);
  ADCSRA = (1 << ADPS2) | (1 << ADPS1); // prescaler of 64 = 8MHz/64 = 125KHz.
  delay(2);                         // Wait for Vref to settle
  ADCSRA |= (1 << ADEN);            // Enable ADC
  delay(2);
  ADCSRA |= _BV(ADSC);              // Start conversion
  while (bit_is_set(ADCSRA, ADSC)); // measuring

  uint8_t low = ADCL;               // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH;              // unlocks both
  uint32_t result = (high << 8) | low;

  result = 1126400L / result;       // Calculate Vcc (in mV); 1125300 = 1100*1024
  return result;                    // Vcc in millivolts
}

void changeValue (bool Up) {
  Rotary = max(min(Rotary + (Up ? 1 : -1), 118), 0);
  newRotary = Rotary / 2;
}

void setupTimer1 () {

  noInterrupts();
  // Clear registers
  TCCR1A = 0;
  TCCR1B = 1 << WGM12 | 1 << CS10 | 1 << CS11;  // CTC, Pre-Scaler 1/64

  // 8.000.000 Hz / 64 = 125.000 Hz -> 125.000 Hz / 125 = 1000 Hz
  // Output Compare Register
  OCR1A = 125 - 1;

  // Output Compare Match A Interrupt Enable
  TIMSK1 |= (1 << OCIE1A);
  interrupts();
}

void buzzer (uint16_t _buzzerDuration, uint8_t _buzzerCount) {

  buzzerDuration = _buzzerDuration;
  buzzerCount = _buzzerCount;
  bBuzzer = true;
}

//-------------------------------- ISR Rotary ----------------------------------

ISR (PCINT1_vect) {
  uint8_t a = (PINB >> ENCODER_A) & 1;
  uint8_t b = (PINB >> ENCODER_B) & 1;
  uint8_t s = (PINB >> ENCODER_S) & 1;

  if (a != a0) {              // A changed
    a0 = a;
    if (b != b0) {
      b0 = b;
      changeValue(a == b);
    }
  } else if (s != s0) {
    s0 = s;
    button = true;
  }
}

// ------------------------------- ISR Timer 1--------------------------------
// is being called at 1000 Hz

ISR(TIMER1_COMPA_vect) {
  volatile static uint16_t _buzzerDuration, _buzzerDurationSave, _buzzerWait;
  volatile static uint8_t _buzzerCount;

  if (bBuzzer) {
    // Setup buzzer
    _buzzerDuration = buzzerDuration;
    _buzzerDurationSave = buzzerDuration;
    _buzzerCount = buzzerCount;
    _buzzerWait = 0;
    bBuzzer = false;
  }
  if (_buzzerWait == 0 && _buzzerCount) {
    // toggle output pin PA7
    if (_buzzerDuration) {
      PINA |= 1 << BUZZER_PIN;
      if (--_buzzerDuration == 0) {
        // switch off buzzer
        PORTA &= ~(1 << BUZZER_PIN);
        _buzzerCount--;
        _buzzerWait = BUZZER_WAIT;
        _buzzerDuration = _buzzerDurationSave;
      }
    }
  }
  else if (_buzzerWait) _buzzerWait--;
}

// ------------------------------- End --------------------------------------
