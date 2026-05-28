static char segment[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

#define HT16K33_BLINK_CMD       0x80
#define HT16K33_BLINK_DISPLAYON 0x01
#define HT16K33_BLINK_OFF       0
#define HT16K33_BLINK_2HZ       1
#define HT16K33_BLINK_1HZ       2
#define HT16K33_BLINK_HALFHZ    3
#define DISPLAY_BRIGHTNESS      8
#define DISPLAY_ADDRESS         0x70
#define SYMBOL_MINUS            0x40
#define SYMBOL_CELCIUS          0x39

uint8_t address;

void initDisplay (uint8_t addr, uint8_t bright) {

  address = addr;
  
  Wire.beginTransmission(addr);
  Wire.write(0x21);
  Wire.endTransmission();

  Wire.beginTransmission(addr);
  Wire.write(0x81);
  Wire.endTransmission();

  // brightness
  Wire.beginTransmission(addr);
  Wire.write(0xE0 | (bright & 0x0F));
  Wire.endTransmission();
}

void writeWord (uint8_t b) {

  Wire.write(b);
  Wire.write(0);
}

void blinkRate(uint8_t b) {

  if (b > 3) b = 0; // turn off if not sure
  Wire.beginTransmission(address);
  Wire.write(HT16K33_BLINK_CMD | HT16K33_BLINK_DISPLAYON | (b << 1));
  Wire.endTransmission();
}

void showDisplay (uint8_t digit1, uint8_t digit2, uint8_t digit3, uint8_t digit4, uint8_t showDigit, bool colon, bool temp) {

  Wire.beginTransmission(address);
  Wire.write(0);

  if (!temp) {
    // show 4 digits
    writeWord(showDigit & 0x08 ? segment[digit1] : 0);
    writeWord(showDigit & 0x04 ? segment[digit2] : 0);
    writeWord(colon ? 0x02 : 0);
    writeWord(showDigit & 0x02 ? segment[digit3] : 0);
    writeWord(showDigit & 0x01 ? segment[digit4] : 0);
  }
  else {
    // show three digits
    writeWord(showDigit & 0x08 ? segment[digit1] : 0);
    writeWord(showDigit & 0x04 ? segment[digit2] | 0x80 : 0);
    writeWord(0x10);
    writeWord(showDigit & 0x02 ? segment[digit3] : 0);
    writeWord(showDigit & 0x01 ? SYMBOL_CELCIUS : 0);   // �C
  }
  Wire.endTransmission();
}
