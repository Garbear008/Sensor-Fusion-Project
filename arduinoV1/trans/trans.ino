//PINS
const int ledTransPin = 3;  // pin for LED
const int buzzerPin = 4;    // pin for buzzer
const int buttonPin = 2;    // pin for button

//light variables
int lightPeriod = 5;   // in milliseconds
int brightness = 255;  // 0–255

//messaging
String inputMessage = "";   // message
const int iterations = 10;  // number of times the message is sent

//sound variables
const int dotDuration = 200;               // in milliseconds; short beep
const int dashDuration = dotDuration * 3;  // long beep
const int bitGap = dotDuration;            // gap between each bit
const int charGap = dotDuration * 8;       // gap between each character

// SETUP
void setup() {
  pinMode(ledTransPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.begin(9600);                                 // begin serial monitor
  Serial.println("Type a message and press ENTER:");  // wait for this to print before sending a message
  delay(500);
}

// LOOP
void loop() {
  if (Serial.available()) {
    inputMessage = Serial.readStringUntil('\n');  // gets the message
    inputMessage.trim();
    Serial.print("Sending: ");
    Serial.println(inputMessage);
    Serial.println("Sending...");
    if (digitalRead(buttonPin) == HIGH)  // LOW = press button for light; HIGH = press button for sound
    {
      // sends message in light
      for (int j = 0; j < iterations; j++) {
        for (int i = 0; i < inputMessage.length(); i++) {
          sendChar(inputMessage.charAt(i));  // sends a character
        }
        delay(1000);  // Pause before repeating
      }
    } else {
      // sends message in sound
      for (int j = 0; j < iterations; j++) {
        charToTone(inputMessage);  // sends the full message
      }
    }
    Serial.println("Done.");
  }
}

// function to transmit each character in light
void sendChar(char c) {
  analogWrite(ledTransPin, 0);  // Start bit (low)
  delay(lightPeriod);
  for (int i = 0; i < 8; i++) {
    bool bit = bitRead(c, 7 - i);                    // reads each bit
    analogWrite(ledTransPin, bit ? brightness : 0);  // turns the LED on based on if bit is true
    delay(lightPeriod);
  }
  analogWrite(ledTransPin, brightness);  // Stop bit
  delay(lightPeriod);
}

// function to transmit a full message in sound
void charToTone(String msg) {
  for (int i = 0; i < msg.length(); i++) {
    char c = msg[i];  // gets the character
    zeroAndOne(c);    // calls for beeps for each character
    delay(charGap);
  }
}

// function to encode each character in sound
void zeroAndOne(char c) {
  for (int i = 0; i < 8; i++) {
    if (bitRead(c, 7 - i) == 0) {
      signal(dotDuration);  // signals a "0"
      Serial.print(0);      // prints out a 0 in serial monitor
    } else {
      signal(dashDuration);  // signals a "1"
      Serial.print(1);       // prints out a 1 in serial monitor
    }
    delay(bitGap);  // silence between each bit
  }
  Serial.print(" ---> ");
  Serial.println(c);  // prints the character in serial monitor
}

// function to play beeps based on duration
void signal(int duration) {
  tone(buzzerPin, 900);  // change tone frequency
  delay(duration);       // plays the beep for this long in time
  noTone(buzzerPin);     // stops the beep
}