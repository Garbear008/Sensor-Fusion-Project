// PINS
const int ldrPin = A0;    // pin for LDR
const int buttonPin = 2;  // pin for button
const int ledRecPin = 3;  // pin for signal LED
const int micPin = A1;    // pin for mic

// light variables
const int period = 5;           // in milliseconds
const int lightThreshold = 60;  //Adjust based on environment

// sound variables
const int soundThreshold = 200;            // Adjust based on your environment
const int dotDuration = 200;               // in milliseconds; should match transmitter
const int dashDuration = dotDuration * 3;  //change these at your own risk
const int charGap = dotDuration * 6;       //change these at your own risk; should detect silence for character
const int durationMs = 50;                 // length of time to sample sound
const int sampleIntervalMs = 1;            // gap between each sample of sound

// switches for debugging
bool plotSound = false;  // plots the loudness values
bool plotLight = false;  // plots the brightness values

// DON'T TOUCH (important)
bool previousLight = true;  // light is on for start bit
bool pickUpSound(int durationMs, int sampleIntervalMs);
int binary[8];  // array of bits
int index = 0;  // starts the array at index 0

// SETUP
void setup() {
  pinMode(ldrPin, INPUT);
  pinMode(micPin, INPUT);
  pinMode(ledRecPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.begin(9600);
  Serial.println("Looking for Code...");
}

// LOOP
void loop() {
  bool currentLight = senseLight();    // gets the current state of brightness
  if (digitalRead(buttonPin) == HIGH)  // LOW = press button for light; HIGH = press button for sound
  {
    digitalWrite(ledRecPin, LOW);          // turn off the signal LED
    if (!currentLight && previousLight) {  // if the transmitting LED was on and is now off; start bit
      char c = getLightByte();             // gets the character based on flashing
      Serial.print(c);                     // prints the character
    }
  } else {
    digitalWrite(ledRecPin, HIGH);                    // turns on the signal LED
    if (pickUpSound(durationMs, sampleIntervalMs)) {  // if sound is picked up
      unsigned long start = millis();                 // get start time
      while (pickUpSound(durationMs, sampleIntervalMs)) {
        // Wait until sound stops
      }
      unsigned long duration = millis() - start;  // get length of beep
      if (index >= 8) {
        index %= 8;  // resets index for byte array if the index reaches 8
      }
      if (duration < (dotDuration + dotDuration / 2)) {
        binary[index] = 0;  // gets a 0 if short beep
        index++;            // increases index by 1
      } else {
        binary[index] = 1;  // gets a 1 if long beep
        index++;            // increases index by 1
      }

      // Wait for silence to measure gap
      unsigned long gapStart = millis();
      while (!pickUpSound(durationMs, sampleIntervalMs)) {
        if (millis() - gapStart > charGap) {  // if detects silence longer than gap between character
          decodeBinary();                     // decode the byte array
          break;
        }
      }
    }
  }
  previousLight = currentLight;  // sets current state of light to previous state for the next time
}

// determines if picks up sound
bool pickUpSound(int durationMs, int sampleIntervalMs) {
  int bias = 0;  // should be silence
  int peakAmplitude = 0;
  unsigned long startTime = millis();  // gets start time

  // gets the peak amplitude
  while (millis() - startTime < durationMs) {
    int sample = analogRead(micPin);
    int deviation = abs(sample - bias);
    if (deviation > peakAmplitude) {
      peakAmplitude = deviation;
    }
    delay(sampleIntervalMs);
  }

  // if plotSound is true in the beginning
  if (plotSound == true) {
    Serial.println(peakAmplitude);  // plots the peak loudness
  }

  return peakAmplitude > soundThreshold;  // returns whether sound is actually detected
}

// determines if light is sensed
bool senseLight() {
  int val = analogRead(ldrPin);  // reads the value from LDR
  // prints out the value if plotLight is true in the beginning
  if (plotLight == true) {
    Serial.println(val);
  }
  return val > lightThreshold;  // returns whether light is actually detected
}

// translates the ons and offs of light into a character
char getLightByte() {
  char c = 0;
  delay(period * 1.5); // Wait after start bit
  for (int i = 7; i >= 0; i--) {
    bool bit = senseLight();  // Read a new bit every loop
    //Serial.print(bit);
    c |= (bit << i);
    delay(period);
  }
  return c;
}

// translates the array of byte in sound into a character
void decodeBinary() {
  char c = 0;
  for (int i = 0; i < 8; i++) {
    int n = binary[i];  // gets the number from each slot
    if (n == 0) {
      c |= (0 << (7 - i));  // "0"
    } else {
      c |= (1 << (7 - i));  // "1"
    }
  }
  Serial.print(c);
  memset(binary, 0, sizeof(binary));  // resets the sound byte
}