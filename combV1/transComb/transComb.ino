// ==============================
// PIN ASSIGNMENTS
// ==============================
const int ledTransPin = 3;  // LED output pin (D3)
const int buzzerPin = 4;    // Buzzer output pin (D4)
const int buttonPin = 2;    // Button input pin (D2)

// ==============================
// LIGHT TRANSMISSION SETTINGS
// ==============================
const int lightPeriod = 3;   // Delay (ms) between each transmitted bit for light
const int brightness = 255;  // LED brightness (0–255)

// ==============================
// SOUND TRANSMISSION (BIT-BY-BIT)
// ==============================
const int dotDuration = 750;               // Duration (µs) for a short beep (represents 0)
const int dashDuration = dotDuration * 3;  // Duration (µs) for a long beep (represents 1)

// ==============================
// SOUND TRANSMISSION (4-BIT GROUPS)
// ==============================
const long fourDotDuration = 5000;                  // Duration (µs) for short beep in 4-bit encoding
const long fourDashDuration = fourDotDuration * 3;  // Duration (µs) for long beep in 4-bit encoding

// Timing gaps for sound encoding
const long bitGap = fourDotDuration * 1.5;  // Gap (µs) between bits
const long charGap = fourDotDuration * 3;   // Gap (µs) between characters

// ==============================
// MESSAGE VARIABLES
// ==============================
String inputMessage = "";   // Stores the input message from Serial
const int iterations = 1;   // Number of times to repeat transmission
bool toBreak = false;       // Flag for breaking loops in auto mode
bool manual = true;         // Flag: true = manual input, false = continuous mode
bool checkSumMode = false;  // Flag: true when transmitting checksum

// ==============================
// SETUP FUNCTION
// ==============================
void setup() {
  pinMode(ledTransPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);      // Ensure buzzer is off at start
  pinMode(buttonPin, INPUT_PULLUP);  // Configure button with pull-up resistor

  Serial.begin(2000000);
  Serial.println("Wait...");
  delay(5000);  // Delay before starting (stability)

  digitalWrite(ledTransPin, HIGH);  // Turn LED on initially (idle state)
  Serial.println("Type a message and press ENTER:");
}

// ==============================
// MAIN LOOP
// ==============================
void loop() {
  if (manual) {  // ---------- MANUAL MODE ----------
    if (Serial.available()) {
      inputMessage = Serial.readStringUntil('\n');  // Read input message from Serial
      inputMessage.trim();                          // Remove whitespace
      inputMessage += " ";                          // Append a space character

      Serial.print("Sending: ");
      Serial.println(inputMessage);

      for (int j = 0; j < iterations; j++) {  // Repeat transmission as per iterations
        for (int i = 0; i < inputMessage.length(); i++) {
          // Send each character via LED and sound encodings
          sendLightChar(inputMessage.charAt(i));
          delayMicroseconds(1);
          encodeOne(inputMessage.charAt(i));
          delayMicroseconds(charGap);
          encodeFour(inputMessage.charAt(i));
          delayMicroseconds(charGap);
        }

        // After message, transmit stop byte
        // switch to checksum mode
        checkSumMode = true;
        sendLightChar(char(0));
        delayMicroseconds(1);
        encodeOne(char(0));
        delayMicroseconds(charGap);
        encodeFour(char(0));
        delayMicroseconds(charGap);

        // After the stop byte, transmit the checksum byte
        sendLightChar(sendCheckSum());
        delayMicroseconds(1);
        encodeOne(sendCheckSum());
        delayMicroseconds(charGap);
        encodeFour(sendCheckSum());
        delayMicroseconds(charGap);

        // return back to normal mode
        checkSumMode = false;
      }

      Serial.println("");
      Serial.println("Done.");  // Transmission finished
      Serial.println("");
    }
  } else {  // ---------- AUTOMATIC MODE ----------
    if (Serial.available()) {
      while (toBreak) {  // Exit if break requested
        toBreak = false;
        break;
      }

      inputMessage = Serial.readStringUntil('\n');  // Get message
      inputMessage.trim(); // trim message
      inputMessage += " "; // append a space
      Serial.print("Sending: ");
      Serial.println(inputMessage);

      while (!toBreak) {
        for (int i = 0; i < inputMessage.length(); i++) {
          // Stop if 'z' received during transmission
          if (Serial.available() && Serial.read() == ('z')) {
            toBreak = true;
            break;
          }
          // Send character in light and sound
          sendLightChar(inputMessage.charAt(i));
          delayMicroseconds(5000);
          encodeOne(inputMessage.charAt(i));
          delayMicroseconds(5000);
          encodeFour(inputMessage.charAt(i));
          delayMicroseconds(5000);
        }

        if (toBreak) break;

        // Transmit stop byte at the end
        // Switch to checksum mode
        checkSumMode = true;
        sendLightChar(char(0));
        delayMicroseconds(5000);
        encodeOne(char(0));
        delayMicroseconds(5000);
        encodeFour(char(0));
        delayMicroseconds(5000);

        // Transmit checksum byte
        sendLightChar(sendCheckSum());
        delayMicroseconds(5000);
        encodeOne(sendCheckSum());
        delayMicroseconds(5000);
        encodeFour(sendCheckSum());
        delayMicroseconds(5000);

        // Switch back to normal mode
        checkSumMode = false;
      }
    }
  }
}

// =========================================================================================
// LIGHT FUNCTIONS
// =========================================================================================
// Transmit a character via LED in serial-like encoding (start bit, parity, 7 data bits, stop bit)
void sendLightChar(char c) {
  analogWrite(ledTransPin, 0);  // Start bit (LED OFF)
  delay(lightPeriod);

  bool firstBit;
  if (!checkSumMode) {
    firstBit = getParityBit(c);  // Parity bit if normal mode
  } else {
    firstBit = bitRead(c, 7);  // Use MSB in checksum mode
  }
  analogWrite(ledTransPin, firstBit ? brightness : 0);
  delay(lightPeriod);

  // Transmit 7 remaining bits
  for (int i = 1; i < 8; i++) {
    bool bit = bitRead(c, 7 - i);
    analogWrite(ledTransPin, bit ? brightness : 0);
    delay(lightPeriod);
  }

  analogWrite(ledTransPin, brightness);  // Stop bit (LED ON)
  delay(lightPeriod);
}

// =========================================================================================
// SOUND FUNCTIONS (BIT-BY-BIT)
// =========================================================================================
// Encode a character into 8 individual sound bits (dot/dash beeps)
void encodeOne(char c) {
  bool firstBit;
  if (!checkSumMode) {
    firstBit = getParityBit(c);
  } else {
    firstBit = bitRead(c, 7);
  }

  // Transmit parity/MSB
  if (!firstBit) {
    signal(dotDuration, 900);
  } else {
    signal(dashDuration, 900);
  }
  delayMicroseconds(bitGap);

  // Transmit remaining 7 bits
  for (int i = 1; i < 8; i++) {
    if (bitRead(c, 7 - i) == 0) {
      signal(dotDuration, 900);
    } else {
      signal(dashDuration, 900);
    }
    delayMicroseconds(bitGap);
  }
}

// =========================================================================================
// SOUND FUNCTIONS (4-BIT GROUPS)
// =========================================================================================
// Encode a character into 2 groups of 4 bits using unique frequencies per nibble
void encodeFour(char c) {
  bool firstBit;
  if (!checkSumMode) {
    firstBit = getParityBit(c);
  } else {
    firstBit = bitRead(c, 7);
  }

  // Process first 4 bits (includes parity/MSB)
  for (int i = 0; i < 8; i += 4) {
    if (i == 0) {
      if (!firstBit) {
        sendThreeBits(0, i, c, fourDotDuration);
      } else {
        sendThreeBits(1, i, c, fourDashDuration);
      }
    } else {
      if (bitRead(c, 7 - i) == 0) {
        sendThreeBits(0, i, c, fourDotDuration);
      } else {
        sendThreeBits(1, i, c, fourDashDuration);
      }
    }
    delayMicroseconds(bitGap);
  }
  Serial.print(" ");
}

// =========================================================================================
// SOUND HELPERS
// =========================================================================================
// Play a beep for given duration and frequency
void signal(int duration, int frequency) {
  tone(buzzerPin, frequency);
  delayMicroseconds(duration);
  noTone(buzzerPin);
}

// Compute even parity bit (returns true if odd number of 1s in character)
bool getParityBit(char c) {
  int counter = 0;
  for (int i = 0; i < 8; i++) {
    if (bitRead(c, 7 - i) == 1) {
      counter++;
    }
  }
  return (counter % 2 == 1);
}

// Encode 3 trailing bits after leading bit, mapping them to specific frequencies
void sendThreeBits(int firstNum, int i, char c, int soundDuration) {
  if (bitRead(c, 7 - i - 1) == 0) {
    if (bitRead(c, 7 - i - 2) == 0) {
      if (bitRead(c, 7 - i - 3) == 0) {
        signal(soundDuration, 1120);
        Serial.print(firstNum);
        Serial.print("000");
      } else {
        signal(soundDuration, 1310);
        Serial.print(firstNum);
        Serial.print("001");
      }
    } else {
      if (bitRead(c, 7 - i - 3) == 0) {
        signal(soundDuration, 1520);
        Serial.print(firstNum);
        Serial.print("010");
      } else {
        signal(soundDuration, 1710);
        Serial.print(firstNum);
        Serial.print("011");
      }
    }
  } else {
    if (bitRead(c, 7 - i - 2) == 0) {
      if (bitRead(c, 7 - i - 3) == 0) {
        signal(soundDuration, 1920);
        Serial.print(firstNum);
        Serial.print("100");
      } else {
        signal(soundDuration, 2110);
        Serial.print(firstNum);
        Serial.print("101");
      }
    } else {
      if (bitRead(c, 7 - i - 3) == 0) {
        signal(soundDuration, 2320);
        Serial.print(firstNum);
        Serial.print("110");
      } else {
        signal(soundDuration, 2810);
        Serial.print(firstNum);
        Serial.print("111");
      }
    }
  }
}

// =========================================================================================
// CHECKSUM FUNCTION
// =========================================================================================
// Compute checksum based on bits at index 1 and 5 of each character
// Returns char containing product of (countOne % 12) and (countTwo % 12)
char sendCheckSum() {
  int countOne = 0;
  int countTwo = 0;
  int indexOne = 1;
  int indexTwo = 5;

  for (int i = 0; i < inputMessage.length(); i++) {
    if (bitRead(inputMessage.charAt(i), 7 - indexOne) == 1) {
      countOne++;
    }
    if (bitRead(inputMessage.charAt(i), 7 - indexTwo) == 1) {
      countTwo++;
    }
  }

  countOne %= 12;
  countTwo %= 12;
  int countMult = int(countOne) * int(countTwo);
  return char(countMult);
}