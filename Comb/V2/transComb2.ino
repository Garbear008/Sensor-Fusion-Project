// ================================= BOARD PIN CONFIGURATION =================================

const int ledTransPin = 3;  // D3 is used to drive the LED for light transmission
const int buzzerPin = 4;  // D4 is used to drive the buzzer
const int buttonPin = 2;  // D2 is used as input with internal pull-up resistor to break from automatic mode

// ================================= LIGHT TRANSMISSION SETTINGS ============================

const int lightPeriod = 3;  // delay in milliseconds between each bit during light transmission
const int brightness = 255;  // full brightness for LED (range 0–255)

// ================================= SOUND TRANSMISSION SETTINGS ============================

// Sound duration for transmitting 1 bit
const int dotDuration = 750;               // duration in microseconds for a "dot" (represents 0)
const int dashDuration = dotDuration * 3;  // duration in microseconds for a "dash" (represents 1)

// Sound duration for transmitting 4 bits
const long fourDotDuration = 5000;                  // duration for "0" in 4-bit grouped sound transmission
const long fourDashDuration = fourDotDuration * 3;  // duration for "1" in 4-bit grouped sound transmission

const long bitGap = fourDotDuration * 1.5;  // microseconds delay between consecutive bits
const long charGap = fourDotDuration * 3;   // microseconds delay between characters

// ================================= MESSAGE CONTROL VARIABLES ==============================

// Message to be transmitted
String inputMessage = "";  // initialize empty message string

// Number of times the message is repeated
const int iterations = 1;  // repeat count

// Control flags
bool toBreak = false;       // used to break out of loops in automatic mode
bool manual = true;         // true = manual mode, false = automatic mode
bool checkSumMode = false;  // true when sending checksum byte

// Debug flag for printing internal states
bool toDebug = false;  // enable/disable debug prints

// ================================= SETUP FUNCTION =========================================
void setup() {
  pinMode(ledTransPin, OUTPUT);      // configure LED pin as output
  pinMode(buzzerPin, OUTPUT);        // configure buzzer pin as output
  digitalWrite(buzzerPin, LOW);      // make sure buzzer is off at startup
  pinMode(buttonPin, INPUT_PULLUP);  // configure button pin as input with pull-up

  Serial.begin(2000000);  // start serial communication at 2,000,000 baud
  Serial.println("Wait...");
  delay(5000);                      // 5-second delay to allow serial initialization
  digitalWrite(ledTransPin, HIGH);  // turn on LED (default state)

  Serial.println("Type a message and press ENTER:");  // prompt for user input
}

// ================================= LOOP FUNCTION ==========================================
void loop() {

  if (manual) {                                     // MANUAL MODE
    if (Serial.available()) {                       // check if user typed a message
      inputMessage = Serial.readStringUntil('\n');  // read message until newline

      if (inputMessage.equalsIgnoreCase("auto")) {  // switch to automatic mode
        manual = false;
        Serial.println("Automatic mode on...");
      }

      if (manual) {           // proceed if still in manual mode
        inputMessage.trim();  // remove leading/trailing whitespace
        inputMessage += " ";  // append a space
        Serial.println("");
        Serial.print("Sending: ");
        Serial.println(inputMessage);
        Serial.println("");

        // Repeat message as per 'iterations'
        for (int j = 0; j < iterations; j++) {
          for (int i = 0; i < inputMessage.length(); i++) {  // send each character
            sendLightChar(inputMessage.charAt(i));           // send character via LED
            delayMicroseconds(1);                            // very short delay
            encodeOne(inputMessage.charAt(i));               // send character via 1-bit sound encoding
            delayMicroseconds(charGap);                      // wait between characters
            encodeFour(inputMessage.charAt(i));              // send character via 4-bit grouped sound
            delayMicroseconds(charGap);                      // wait between characters
          }

          // Send null character (0) for separation
          sendLightChar(char(0));
          delayMicroseconds(1);
          encodeOne(char(0));
          delayMicroseconds(charGap);
          encodeFour(char(0));
          delayMicroseconds(charGap);

          // Send checksum byte
          checkSumMode = true;  // flag to mark checksum transmission
          sendLightChar(sendCheckSum());
          delayMicroseconds(1);
          encodeOne(sendCheckSum());
          delayMicroseconds(charGap);
          encodeFour(sendCheckSum());
          delayMicroseconds(charGap);
          checkSumMode = false;  // reset flag
        }

        if (toDebug) {
          Serial.println("");
        }

        Serial.println("Done.");  // indicate completion
      }
    }

  } else {  // AUTOMATIC MODE

    if (Serial.available()) {  // check for input
      while (toBreak) {        // safety break loop
        toBreak = false;
        break;
      }

      inputMessage = Serial.readStringUntil('\n');  // read input

      if (inputMessage.equalsIgnoreCase("manual")) {  // switch back to manual mode
        manual = true;
        Serial.println("Manual mode on...");
      }

      if (!manual) {          // send message automatically
        inputMessage.trim();  // remove whitespace
        inputMessage += " ";  // append a space
        Serial.println("");
        Serial.print("Sending: ");
        Serial.println(inputMessage);
        Serial.println("");

        while (!toBreak) {
          for (int i = 0; i < inputMessage.length(); i++) {
            // Check if user pressed 'z' or button to stop
            if ((Serial.available() && Serial.read() == ('z')) || digitalRead(buttonPin) == LOW) {
              toBreak = true;
              Serial.println("Stopped...");
              break;
            }

            sendLightChar(inputMessage.charAt(i));
            delayMicroseconds(1);
            encodeOne(inputMessage.charAt(i));
            delayMicroseconds(charGap);
            encodeFour(inputMessage.charAt(i));
            delayMicroseconds(charGap);
          }

          if (toBreak) { break; }  // exit loop if stopped

          sendLightChar(char(0));
          delayMicroseconds(1);
          encodeOne(char(0));
          delayMicroseconds(charGap);
          encodeFour(char(0));
          delayMicroseconds(charGap);

          // Send checksum byte
          checkSumMode = true;
          sendLightChar(sendCheckSum());
          delayMicroseconds(1);
          encodeOne(sendCheckSum());
          delayMicroseconds(charGap);
          encodeFour(sendCheckSum());
          delayMicroseconds(charGap);
          checkSumMode = false;
        }
      }
    }
  }
}

// ================================= LIGHT FUNCTIONS ========================================

// Send a single character via LED
void sendLightChar(char c) {
  analogWrite(ledTransPin, 0);  // start bit
  delay(lightPeriod);

  uint16_t code12 = hammingEncode11_7(c);  // encode 7-bit char into 12-bit Hamming

  if (toDebug) {
    Serial.print("Light Bits for ");
    Serial.print(c);
    if (checkSumMode) { Serial.print("(Checksum byte) "); }
    Serial.print(": ");
  }

  // Send 12 bits
  for (int i = 0; i < 12; i++) {
    bool bit = bitRead(code12, 11 - i);              // extract bit from encoded data
    analogWrite(ledTransPin, bit ? brightness : 0);  // set LED ON for 1, OFF for 0
    if (toDebug) { Serial.print(bit); }
    delay(lightPeriod);  // wait between bits
  }

  if (toDebug) { Serial.println(""); }

  analogWrite(ledTransPin, brightness);  // stop bit (LED ON)
  delay(lightPeriod);                    // inter-byte delay
}

// ================================= SOUND FUNCTIONS ========================================

// Encode a character and send via 1-bit sound
void encodeOne(char c) {
  uint16_t code12 = hammingEncode11_7(c);

  if (toDebug) {
    Serial.print("Sound1 Bits for ");
    Serial.print(c);
    if (checkSumMode) { Serial.print("(Checksum byte) "); }
    Serial.print(": ");
  }

  // Send 12 bits as individual tones
  for (int i = 0; i < 12; i++) {
    if (bitRead(code12, 11 - i) == 0) {
      signal(dotDuration, 900);  // short beep for 0
      if (toDebug) { Serial.print(0); }
    } else {
      signal(dashDuration, 900);  // long beep for 1
      if (toDebug) { Serial.print(1); }
    }
    delayMicroseconds(bitGap);  // wait between bits
  }

  if (toDebug) { Serial.println(""); }
}

// Encode a character and send via 4-bit grouped sound
void encodeFour(char c) {
  uint16_t code12 = hammingEncode11_7(c);

  if (toDebug) {
    Serial.print("Sound4 Bits for ");
    Serial.print(c);
    if (checkSumMode) { Serial.print("(Checksum byte) "); }
    Serial.print(": ");
  }

  for (int i = 0; i < 12; i += 4) {  // process 4-bit chunks
    if (bitRead(code12, 11 - i) == 0) {
      sendThreeBits(0, i, code12, fourDotDuration);
    } else {
      sendThreeBits(1, i, code12, fourDashDuration);
    }
    delayMicroseconds(bitGap);  // wait between groups
  }

  if (toDebug) { Serial.println(""); }
}

// Play tone of given duration and frequency
void signal(int duration, int frequency) {
  tone(buzzerPin, frequency);   // start tone
  delayMicroseconds(duration);  // hold tone for duration
  noTone(buzzerPin);            // stop tone
}

// Send 3-bit group with distinct frequency based on pattern
void sendThreeBits(int firstNum, int i, uint16_t c, int soundDuration) {
  if (bitRead(c, 11 - i - 1) == 0) {
    if (bitRead(c, 11 - i - 2) == 0) {
      if (bitRead(c, 11 - i - 3) == 0) {
        signal(soundDuration, 1120);
        if (toDebug) {
          Serial.print(firstNum);
          Serial.print("000");
        }
      } else {
        signal(soundDuration, 1310);
        if (toDebug) {
          Serial.print(firstNum);
          Serial.print("001");
        }
      }
    } else {
      if (bitRead(c, 11 - i - 3) == 0) {
        signal(soundDuration, 1520);
        if (toDebug) {
          Serial.print(firstNum);
          Serial.print("010");
        }
      } else {
        signal(soundDuration, 1710);
        if (toDebug) {
          Serial.print(firstNum);
          Serial.print("011");
        }
      }
    }
  } else {
    if (bitRead(c, 11 - i - 2) == 0) {
      if (bitRead(c, 11 - i - 3) == 0) {
        signal(soundDuration, 1920);
        if (toDebug) {
          Serial.print(firstNum);
          Serial.print("100");
        }
      } else {
        signal(soundDuration, 2110);
        if (toDebug) {
          Serial.print(firstNum);
          Serial.print("101");
        }
      }
    } else {
      if (bitRead(c, 11 - i - 3) == 0) {
        signal(soundDuration, 2320);
        if (toDebug) {
          Serial.print(firstNum);
          Serial.print("110");
        }
      } else {
        signal(soundDuration, 2810);
        if (toDebug) {
          Serial.print(firstNum);
          Serial.print("111");
        }
      }
    }
  }
}

// ================================= CHECKSUM FUNCTION ======================================

// Generate simple checksum byte for the message
char sendCheckSum() {
  int countOne = 0;  // counter for bit at indexOne
  int countTwo = 0;  // counter for bit at indexTwo
  int indexOne = 1;  // first bit position to consider for checksum
  int indexTwo = 5;  // second bit position

  for (int i = 0; i < inputMessage.length(); i++) {
    if (bitRead(inputMessage.charAt(i), 7 - indexOne) == 1) { countOne++; }
    if (bitRead(inputMessage.charAt(i), 7 - indexTwo) == 1) { countTwo++; }
  }

  countOne %= 12;  // modulo to keep within 12-bit space
  countTwo %= 12;

  int countMult = int(countOne) * int(countTwo);  // multiply counts
  return char(countMult);                         // return as checksum byte
}

// ================================= HAMMING ENCODING ======================================

// Encode 7-bit data into 12-bit Hamming(11,7) code
uint16_t hammingEncode11_7(uint8_t data7) {
  bool d[7];  // array to hold data bits

  // Extract bits from input
  for (int i = 0; i < 7; i++) {
    d[i] = (data7 >> i) & 1;
  }

  bool h[12] = { 0 };  // Hamming code bits (0th = overall parity)

  // Assign data bits to Hamming positions
  h[3] = d[0];   // d1
  h[5] = d[1];   // d2
  h[6] = d[2];   // d3
  h[7] = d[3];   // d4
  h[9] = d[4];   // d5
  h[10] = d[5];  // d6
  h[11] = d[6];  // d7

  // Calculate Hamming parity bits
  h[1] = h[3] ^ h[5] ^ h[7] ^ h[9] ^ h[11];
  h[2] = h[3] ^ h[6] ^ h[7] ^ h[10] ^ h[11];
  h[4] = h[5] ^ h[6] ^ h[7];
  h[8] = h[9] ^ h[10] ^ h[11];

  // Overall parity
  bool overall = 0;
  for (int i = 1; i <= 11; i++) overall ^= h[i];
  h[0] = overall;

  // Convert Hamming bits array to uint16_t
  uint16_t code = 0;
  for (int i = 0; i < 12; i++) code |= (h[i] << i);

  return code;  // return 12-bit Hamming code
}