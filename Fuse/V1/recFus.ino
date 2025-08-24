// BOARD
const int ldrPin = A0;    // pin designation (A0) for LDR
const int buttonPin = 2;  // pin designation (D2) for the button
const int ledRecPin = 3;  // pin designation (D3) for LED
const int micPin = A1;    // pin designation (A1) for microphone

// light transmission
const int period = 10;              // period in milliseconds for each bit delay
const long durationToStart = 2000;  // time in milliseconds to set up the LDR
bool previousLight = true;          // because the LED is on before each byte

// sound transmission in milliseconds
const long dotDuration = 10;           // time duration for each short beep (dot or "0" in binary)
const long charGap = dotDuration * 6;  // gap in time for each character (or after each byte)
const long durationMs = 1;             // time in milliseconds to complete the sampling of the sound
const long sampleIntervalMs = 0.1;     // time in milliseconds it takes between each sample

// to plot the light/sound on scale of (0-255 for light) (0-1023 for sound)
bool plotLight = false;    // true if want to visualize sound on serial plotter (for troubleshooting)
bool plotSound = false;    // true if want to visualize sound on serial plotter (for troubleshooting)
int lightThreshold = 100;  // Adjust based on your environment
int soundThreshold = 200;  // Adjust based on your environment

// Don't touch!!!
String lightString = "";  // an array of 8 number; to store 8 bits, acts as a byte array
String soundString = "";
int binary[8];
int index = 0;
bool pickUpSound(long durationMs, long sampleIntervalMs);  // to declare function title for pickUpSound

// SETUP
void setup() {
  pinMode(ldrPin, INPUT);
  pinMode(micPin, INPUT);
  pinMode(ledRecPin, OUTPUT);        // LED will be on or off based on the medium of reception
  pinMode(buttonPin, INPUT_PULLUP);  // button will be pressed to switch between light and sound transmission
  Serial.begin(115200);
  Serial.println("Not yet ready for Code...");  // customize the message to see fit
  digitalWrite(ledRecPin, LOW);                 // make sure the LED is not on on the reciever end
  // get the ambient light and the sound threshold
  // comment the following two lines out if you want to set it constant above
  lightThreshold = getLightThreshold() + 50;             // get the ambient light (with buffer)
  Serial.println(lightThreshold);                        // to see the light threshold value
  soundThreshold = sampleSound(durationToStart, 1) + 5;  // get the sound threshold (with buffer)
  Serial.println(soundThreshold);                        // to see the sound threshold value
  while (analogRead(ldrPin) < lightThreshold) {
    delay(100); // wait for transmitter LED to turn on
  }
  Serial.println("Now ready for Code...");  // customize the message to see fit
}

// LOOP
void loop() {
  bool currentLight = senseLight();  // detects current state of the LED in the transmitter
  // light mode
  if (!currentLight && previousLight)  // while it was on and now it is off
  {
    char c = getLightByte();  // get byte for that character using binary code
    if (isDisplayable(c)) {
      lightString += c;
    }
    Serial.print(c);  // prints out the character
  }
  previousLight = currentLight;  // to reset the light state (for light reception)
    // sound mode
  if (pickUpSound(durationMs, sampleIntervalMs)) {  // if sound is detected
    unsigned long start = millis();                 // start time

    while (pickUpSound(durationMs, sampleIntervalMs)) {
      // wait until sound stops
    }
    unsigned long duration = millis() - start;  // how long the sound lasts

    // resets index of the array of greater than or equal to 8 for number of bits in a byte
    if (index >= 8) {
      index %= 8;
    }

    // to distinguish between short beep and long beep
    if (duration < (dotDuration + dotDuration / 2)) {
      binary[index] = 0;  // short beep if less than 1.5x the dotDuration (the designated time for a short beep)
    } else {
      binary[index] = 1;  // long beep if more than 1.5x the dotDuration
    }
    index++;  // goes to next bit space in the byte array

    // Wait for silence to measure gap
    unsigned long gapStart = millis();
    while (!pickUpSound(durationMs, sampleIntervalMs)) {  // detects for the absence of sound (gap)
      if (millis() - gapStart > charGap) {                // run this at the end of each byte to separate characters
        decodeBinary();                                   // decodes the byte array
        break;
      }
    }
  }
  if (Serial.available()) {
    char input = Serial.read();
    if (input == 'l')  // command "l" allows for light-dominant string to display
    {
      Serial.println("");
      Serial.println("Printing light-dominant...");
      printOut(lightString, soundString);
    } else if (input == 's')  // command "s" allows for sound-dominant string to display
    {
      Serial.println("");
      Serial.println("Printing sound-dominant...");
      printOut(soundString, lightString);
    } else if (input == 'c')  // command "c" allows for strings for both light and string to display
    {
      Serial.println("");
      Serial.println("Light: " + lightString);
      Serial.println("Sound: " + soundString);
      lightString = "";
      soundString = "";
      memset(binary, 0, sizeof(binary));  // clears the array of bits
      Serial.println("Memory cleared");
    }
  }
}

// =================================================================================
// LIGHT FUNCTIONS
// function to detect light
bool senseLight() {
  int val = analogRead(ldrPin);  // reads the instantaneous light brightness in surroundings
  if (plotLight) {
    Serial.println(val);  // for troubleshooting
  }
  return val > lightThreshold;  // true if the brightness exceeds the threshold
}

// function to translate light to bits for a character
char getLightByte() {
  char c = 0;
  delay(period * 1.5);  // wait after start bit
  for (int i = 7; i >= 0; i--) {
    bool bit = senseLight();  // read a new bit every loop
    c |= (bit << i);          // assigns each bit of c to a 0 (if light is not sensed) or 1 (if sensed)
    delay(period);            // delay between each bit
  }
  return c;
}

// function to get the average light brightness before transmission
int getLightThreshold() {
  double sum = 0;
  double numOfSample = 0;
  unsigned long startTime = millis();
  while (millis() - startTime < durationToStart) {
    double detected = analogRead(ldrPin);
    sum += detected;  // adds the read value to the sum in each sample
    numOfSample += 1;
    delay(1);
  }
  return (int)(sum / numOfSample) + 1;  // return average +1
}

// ==================================================================================
// SOUND FUNCTIONS
// function to detect sound
bool pickUpSound(long durationMs, long sampleIntervalMs) {
  int sound = sampleSound(durationMs, sampleIntervalMs);  // get the peak volume
  return sound > soundThreshold;                          // true if the difference (between generated sound and ambient noise) is higher than the threshold
}

int sampleSound(long durationMs, long sampleIntervalMs) {
  int bias = 0;  // midway between 0 and 1023
  int peakAmplitude = 0;
  unsigned long startTime = millis();  // get the start time

  // for the duration of sampling
  while (millis() - startTime < durationMs) {
    int sample = analogRead(micPin);     // reads the instantaneous volume of sound in surroundings
    int deviation = abs(sample - bias);  // difference between sample and 512
    if (deviation > peakAmplitude) {
      peakAmplitude = deviation;  // to get the highest deviation
    }
    delay(sampleIntervalMs);  // how much delay after each sample
  }
  if (plotSound) {
    Serial.println(peakAmplitude);  // for troubleshooting
  }
  return peakAmplitude;
}

// function to translate the array of bits into a character (for sound)
void decodeBinary() {
  char c = 0;
  for (int i = 0; i < 8; i++) {
    int n = binary[i];  // gets the number from each bit slot
    if (n == 0) {
      c |= (0 << (7 - i));  // sets the (7-i)th index to 0
    } else {
      c |= (1 << (7 - i));  // sets the (7-i)th index to 1
    }
  }
  if (isDisplayable(c)) {
    soundString += c;  // add the character to the collection of chars if displayable
  }
  Serial.print(c);                    // prints the character out
  memset(binary, 0, sizeof(binary));  // clears the array of bits
}

// ========================================================================================
//DISPLAY FUNCTIONS
// prints out strings on serial monitor based on preference (dominance)
void printOut(String dominant, String weak) {
  for (int i = 0; i < max(dominant.length(), weak.length()); i++) {  // gets the length of longest string
    if (isDisplayable(dominant[i]))                                  // if dominant character is displayable
    {
      Serial.print(dominant[i]);        // then print that character
    } else if (isDisplayable(weak[i]))  // if dominant character displayable and the weak character is displayable
    {
      Serial.print(weak[i]);  // then prints the weak character
    } else {
      Serial.print("?");  // otherwise print a question mark
    }
  }
  Serial.print("");
  memset(binary, 0, sizeof(binary));  // clears the array of bits
}

// function to check if the character is displayable
bool isDisplayable(char c) {
  return (c >= 32 && c <= 126);
}