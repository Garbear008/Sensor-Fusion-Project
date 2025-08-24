// ======================================================================================
// BOARD PINS
// ======================================================================================
const int ledTransPin = 3;  // pin designation (D3) for LED
const int buzzerPin = 4;    // pin designation (D4) for buzzer
const int buttonPin = 2;    // pin designation (D2) for button

// ======================================================================================
// LIGHT TRANSMISSION SETTINGS
// ======================================================================================
int lightPeriod = 3;   // delay between each bit for light transmission (ms)
int brightness = 255;  // brightness level for LED (0–255)

// ======================================================================================
// SOUND TRANSMISSION SETTINGS (in microseconds)
// ======================================================================================
const long dotDuration = 5000;              // short beep (dot or "0" in binary)
const long dashDuration = dotDuration * 3;  // long beep (dash or "1" in binary)
const long bitGap = dotDuration * 3;        // gap between 2 encoded bits
const long charGap = dotDuration * 4;       // gap between 2 encoded characters

// ======================================================================================
// MESSAGING SETTINGS
// ======================================================================================
String inputMessage = "";    // string buffer to store message from serial
const int iterations = 100;  // number of times the message will repeat
bool toBreak = false;        // flag to break out of auto loop
bool manual = false;         // flag to choose manual or auto mode

// ======================================================================================
// SETUP FUNCTION (runs once at startup)
// ======================================================================================
void setup() {
  pinMode(ledTransPin, OUTPUT);      // configure LED pin as output
  pinMode(buzzerPin, OUTPUT);        // configure buzzer pin as output
  digitalWrite(buzzerPin, LOW);      // ensure buzzer starts OFF
  pinMode(buttonPin, INPUT_PULLUP);  // button pin with internal pull-up resistor

  Serial.begin(115200);              // initialize serial at 115200 baud
  Serial.println("Wait...");         // simple startup message
  delay(5000);                       // wait 5 seconds before starting

  digitalWrite(ledTransPin, HIGH);   // turn LED ON (idle indicator)
  Serial.println("Type a message and press ENTER:");  
}

// ======================================================================================
// MAIN LOOP (runs continuously)
// ======================================================================================
void loop() {
  if (manual) {  // -------- MANUAL MODE --------
    if (Serial.available()) {
      inputMessage = Serial.readStringUntil('\n');  // read input line
      inputMessage.trim();                          // remove leading/trailing spaces
      inputMessage += " ";                          // add space as end marker
      Serial.print("Sending: ");
      Serial.println(inputMessage);

      // Repeat message multiple times
      for (int j = 0; j < iterations; j++) {
        for (int i = 0; i < inputMessage.length(); i++) {
          sendLightChar(inputMessage.charAt(i));  // send char as light signal
          delayMicroseconds(1000);                // short pause between light and sound
          zeroAndOne(inputMessage.charAt(i));     // send char as sound signal
          delayMicroseconds(5000);                // pause before next char
        }
        delayMicroseconds(5);  // pause before repeating full message
      }
      Serial.println("Done.");  // end-of-transmission indicator
    }
  } else {  // -------- AUTO MODE --------
    // stay idle if "toBreak" is set, wait for serial reset
    while (toBreak) {
      if (Serial.read() > 0) {  // any serial input resets flag
        toBreak = false;
        break;
      }
    }

    // if a new message is typed in Serial Monitor
    if (Serial.available()) {
      inputMessage = Serial.readStringUntil('\n');  // read input line
      inputMessage.trim();                          // clean up spaces
      inputMessage += " ";                          // add space at end
      Serial.print("Sending: ");
      Serial.println(inputMessage);

      // send until break signal "z" is received
      while (!toBreak) {
        for (int i = 0; i < inputMessage.length(); i++) {
          if (Serial.available() && Serial.read() == ('z')) {  
            toBreak = true;    // stop if 'z' is sent
            break;
          }
          sendLightChar(inputMessage.charAt(i));  // transmit character as light
          delayMicroseconds(1000);                // short pause
          zeroAndOne(inputMessage.charAt(i));     // transmit character as sound
          delayMicroseconds(5000);                // pause before next char
        }
        if (toBreak) { break; }                   // exit outer loop too
        delayMicroseconds(5);                     // pause before repeating
      }
    }
  }
}

// ======================================================================================
// LIGHT FUNCTIONS
// ======================================================================================

// function to send a single character as light pulses
void sendLightChar(char c) {
  analogWrite(ledTransPin, 0);  // start bit (LED OFF)
  delay(lightPeriod);           // short pause before sending bits

  // send each of the 8 bits
  for (int i = 0; i < 8; i++) {
    bool bit = bitRead(c, 7 - i);                    // extract each bit (MSB first)
    analogWrite(ledTransPin, bit ? brightness : 0);  // LED ON if 1, OFF if 0
    delay(lightPeriod);                              // pause between bits
  }

  analogWrite(ledTransPin, brightness);  // stop bit (LED ON)
  delay(lightPeriod);                    // pause before next character
}

// ======================================================================================
// SOUND FUNCTIONS
// ======================================================================================

// function to convert character to binary groups of 4 bits, then beep
void zeroAndOne(char c) {
  // process 8 bits in two groups of 4 bits
  for (int i = 0; i < 8; i += 4) {
    // nested if statements check the 4-bit pattern
    if (bitRead(c, 7 - i) == 0) {
      if (bitRead(c, 7 - i - 1) == 0) {
        if (bitRead(c, 7 - i - 2) == 0) {
          if (bitRead(c, 7 - i - 3) == 0) {
            signal(dotDuration, 160);  // short beep 160 Hz if "0000"
            Serial.print("0000");      // debug print
          } else {
            signal(dotDuration, 410);  // short beep 410 Hz if "0001"
            Serial.print("0001");
          }
        } else {
          if (bitRead(c, 7 - i - 3) == 0) {
            signal(dotDuration, 660);  // short beep 660 Hz if "0010"
            Serial.print("0010");
          } else {
            signal(dotDuration, 910);  // short beep 910 Hz if "0011"
            Serial.print("0011");
          }
        }
      } else {
        if (bitRead(c, 7 - i - 2) == 0) {
          if (bitRead(c, 7 - i - 3) == 0) {
            signal(dotDuration, 1160);  // short beep 1160 Hz if "0100"
            Serial.print("0100");
          } else {
            signal(dotDuration, 1410);  // short beep 1410 Hz if "0101"
            Serial.print("0101");
          }
        } else {
          if (bitRead(c, 7 - i - 3) == 0) {
            signal(dotDuration, 1660);  // short beep 1660 Hz if "0110"
            Serial.print("0110");
          } else {
            signal(dotDuration, 2000);  // short beep 2000 Hz if "0111"
            Serial.print("0111");
          }
        }
      }
    } else {
      if (bitRead(c, 7 - i - 1) == 0) {
        if (bitRead(c, 7 - i - 2) == 0) {
          if (bitRead(c, 7 - i - 3) == 0) {
            signal(dashDuration, 150);  // long beep 150 Hz if "1000"
            Serial.print("1000");
          } else {
            signal(dashDuration, 410);  // long beep 410 Hz if "1001"
            Serial.print("1001");
          }
        } else {
          if (bitRead(c, 7 - i - 3) == 0) {
            signal(dashDuration, 660);  // long beep 660 Hz if "1010"
            Serial.print("1010");
          } else {
            signal(dashDuration, 910);  // long beep 910 Hz if "1011"
            Serial.print("1011");
          }
        }
      } else {
        if (bitRead(c, 7 - i - 2) == 0) {
          if (bitRead(c, 7 - i - 3) == 0) {
            signal(dashDuration, 1160);  // long beep 1160 Hz if "1100"
            Serial.print("1100");
          } else {
            signal(dashDuration, 1410);  // long beep 1410 Hz if "1101"
            Serial.print("1101");
          }
        } else {
          if (bitRead(c, 7 - i - 3) == 0) {
            signal(dashDuration, 1660);  // long beep 1660 Hz if "1110"
            Serial.print("1110");
          } else {
            signal(dashDuration, 2000);  // long beep 2000 Hz if "1111"
            Serial.print("1111");
          }
        }
      }
    }
    delayMicroseconds(bitGap);  // gap between two 4-bit groups
  }
  // (optional debugging) print original character
  // Serial.print (" ---> ");
  // Serial.println(c);
}

// ======================================================================================
// SIGNAL FUNCTION
// ======================================================================================

// plays a tone of a given frequency and duration
void signal(int duration, int frequency) {
  tone(buzzerPin, frequency);   // play tone on buzzer
  delayMicroseconds(duration);  // keep tone ON for duration
  noTone(buzzerPin);            // stop tone
}