// ========================== BOARD SETUP ==========================
const int ldrPin = A0;    // LDR sensor pin (light input)
const int buttonPin = 2;  // Button pin to switch between modes
const int ledRecPin = 3;  // Status LED on receiver
const int micPin = A1;    // Microphone pin (sound input)

// ========================== LIGHT SETTINGS ==========================
const int period = 3;               // delay (ms) between each bit
const long durationToStart = 2000;  // setup time for LDR (µs)
bool previousLight = true;          // assume light is ON before transmission starts

// ========================== SOUND SETTINGS ==========================
const long dotDuration = 5000;         // short beep duration ("0") in µs
const long charGap = dotDuration * 3;  // pause between bytes
const long bitGap = dotDuration;       // pause between bits
const long durationMicros = 100;       // sampling window (µs)
const long sampleIntervalMicros = 2;   // delay between each mic sample

// ========================== DEBUG TOOLS ==========================
bool plotLight = false;     // plot LDR values in Serial Plotter
bool plotSound = false;     // plot mic values in Serial Plotter
long lightThreshold = 100;  // starting threshold for light detection
long soundThreshold = 150;  // starting threshold for sound detection
int numSoundCrossing = 0;   // how many times sound crosses threshold

// ========================== MEMORY ==========================
String lightString = "";  // stores received characters (light)
String soundString = "";  // stores received characters (sound)
int binary[8];            // stores 8 bits per character
int index = 0;            // keeps track of where we’re writing bits

// function pre-declare
bool pickUpSound(int durationMicros, int sampleIntervalMicros);

// ========================== SETUP ==========================
void setup() {
  pinMode(ldrPin, INPUT);
  pinMode(micPin, INPUT);
  pinMode(ledRecPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);
  Serial.begin(115200);

  Serial.println("Not yet ready for Code...");
  digitalWrite(ledRecPin, LOW);  // LED off at start

  // calibrate thresholds (ambient noise + buffer)
  lightThreshold = getLightThreshold() + 50;
  Serial.println(lightThreshold);

  soundThreshold = sampleSound(durationToStart, 1) + 5;
  Serial.println(soundThreshold);

  // wait until light is ON (idle HIGH state)
  while (analogRead(ldrPin) < lightThreshold) {
    delay(100);
  }

  Serial.println("Now ready for Code...");
}

// ========================== MAIN LOOP ==========================
void loop() {
  bool currentLight = senseLight();

  // -------- LIGHT MODE --------
  if (!currentLight && previousLight) {  // falling edge detected (start bit)
    char c = getLightByte();
    if (isDisplayable(c)) {
      lightString += c;
    }
    //Serial.print(c);
  }
  previousLight = currentLight;

  // -------- SOUND MODE --------
  if (pickUpSound(durationMicros, sampleIntervalMicros)) {  // sound detected
    unsigned long start = micros();
    numSoundCrossing = 1;

    // track duration of sound until silence is stable
    while (true) {
      if (!pickUpSound(durationMicros, sampleIntervalMicros)) {
        unsigned long silenceStart = micros();
        bool stableSilence = true;
        numSoundCrossing++;
        while (micros() - silenceStart < bitGap) {
          if (pickUpSound(durationMicros, sampleIntervalMicros)) {
            stableSilence = false;
            numSoundCrossing++;
            break;
          }
        }
        if (stableSilence) break;
      }
    }

    unsigned long duration = micros() - start - bitGap;
    int soundPeriod = 2 * (duration) / (numSoundCrossing);
    numSoundCrossing = 0;

    int freq = 900000 / (soundPeriod);  // crude freq estimate

    // wrap around after 8 bits
    if (index >= 8) {
      index %= 8;
    }

    // distinguish dot vs dash
    if (duration < (dotDuration + dotDuration / 8)) {
      binary[index] = 0;
      // frequency-based encoding (extra 3 bits)
      if (freq < 330) {
        binary[index + 1] = 0;
        binary[index + 2] = 0;
        binary[index + 3] = 0;
      } else if (freq < 600) {
        binary[index + 1] = 0;
        binary[index + 2] = 0;
        binary[index + 3] = 1;
      } else if (freq < 800) {
        binary[index + 1] = 0;
        binary[index + 2] = 1;
        binary[index + 3] = 0;
      } else if (freq < 1050) {
        binary[index + 1] = 0;
        binary[index + 2] = 1;
        binary[index + 3] = 1;
      } else if (freq < 1300) {
        binary[index + 1] = 1;
        binary[index + 2] = 0;
        binary[index + 3] = 0;
      } else if (freq < 1500) {
        binary[index + 1] = 1;
        binary[index + 2] = 0;
        binary[index + 3] = 1;
      } else if (freq < 1800) {
        binary[index + 1] = 1;
        binary[index + 2] = 1;
        binary[index + 3] = 0;
      } else {
        binary[index + 1] = 1;
        binary[index + 2] = 1;
        binary[index + 3] = 1;
      }
    } else {
      binary[index] = 1;
      // frequency-based encoding (extra 3 bits)
      if (freq < 300) {
        binary[index + 1] = 0;
        binary[index + 2] = 0;
        binary[index + 3] = 0;
      } else if (freq < 550) {
        binary[index + 1] = 0;
        binary[index + 2] = 0;
        binary[index + 3] = 1;
      } else if (freq < 800) {
        binary[index + 1] = 0;
        binary[index + 2] = 1;
        binary[index + 3] = 0;
      } else if (freq < 1050) {
        binary[index + 1] = 0;
        binary[index + 2] = 1;
        binary[index + 3] = 1;
      } else if (freq < 1300) {
        binary[index + 1] = 1;
        binary[index + 2] = 0;
        binary[index + 3] = 0;
      } else if (freq < 1500) {
        binary[index + 1] = 1;
        binary[index + 2] = 0;
        binary[index + 3] = 1;
      } else if (freq < 1800) {
        binary[index + 1] = 1;
        binary[index + 2] = 1;
        binary[index + 3] = 0;
      } else {
        binary[index + 1] = 1;
        binary[index + 2] = 1;
        binary[index + 3] = 1;
      }
    }
    index += 4;  // jump to next slot

    // wait for silence to separate characters
    unsigned long gapStart = micros();
    while (!pickUpSound(durationMicros, sampleIntervalMicros)) {
      if (micros() - gapStart > charGap) {
        decodeBinary();
        break;
      }
    }
  }

  // -------- SERIAL COMMANDS --------
  if (Serial.available()) {
    char input = Serial.read();
    if (input == 'l') {
      Serial.println("");
      Serial.println("Printing light-dominant...");
      printOut(lightString, soundString);
    } else if (input == 's') {
      Serial.println("");
      Serial.println("Printing sound-dominant...");
      printOut(soundString, lightString);
    } else if (input == 'c') {
      Serial.println("");
      Serial.println("Light: " + lightString);
      Serial.println("Sound: " + soundString);
      lightString = "";
      soundString = "";
      memset(binary, 0, sizeof(binary));
      Serial.println("Memory cleared");
    }
  }
}

// ========================== LIGHT FUNCTIONS ==========================

// reads brightness (0–1023) and checks threshold
bool senseLight() {
  int val = analogRead(ldrPin);
  if (plotLight) Serial.println(val);
  return val > lightThreshold;
}

// grab 8 bits over time and turn into a character
char getLightByte() {
  char c = 0;
  delay(period * 1.5);
  for (int i = 7; i >= 0; i--) {
    bool bit = senseLight();
    c |= (bit << i);
    delay(period);
  }
  return c;
}

// average brightness during idle state (calibration)
int getLightThreshold() {
  double sum = 0;
  double numOfSample = 0;
  unsigned long startTime = millis();
  while (millis() - startTime < durationToStart) {
    double detected = analogRead(ldrPin);
    sum += detected;
    numOfSample += 1;
    delay(1);
  }
  return (int)(sum / numOfSample) + 1;
}

// ========================== SOUND FUNCTIONS ==========================

// return true if peak sound > threshold
bool pickUpSound(int durationMicros, int sampleIntervalMicros) {
  int sound = sampleSound(durationMicros, sampleIntervalMicros);
  return sound > soundThreshold;
}

// sample mic input for a duration, return loudest peak
int sampleSound(int durationMicros, int sampleIntervalMicros) {
  int bias = 0;
  int peakAmplitude = 0;
  unsigned long startTime = micros();

  while (micros() - startTime < durationMicros) {
    int sample = analogRead(micPin);
    int deviation = abs(sample - bias);
    if (deviation > peakAmplitude) {
      peakAmplitude = deviation;
    }
    delayMicroseconds(sampleIntervalMicros);
  }
  if (plotSound) Serial.println(peakAmplitude);
  return peakAmplitude;
}

// turn binary[] into a char and append to string
void decodeBinary() {
  char c = 0;
  for (int i = 0; i < 8; i++) {
    int n = binary[i];
    if (n == 0) c |= (0 << (7 - i));
    else c |= (1 << (7 - i));
  }
  if (isDisplayable(c)) {
    soundString += c;
  }
  Serial.print(c);
  memset(binary, 0, sizeof(binary));
}

// print two strings at once, prioritizing one
void printOut(String dominant, String weak) {
  for (int i = 0; i < max(dominant.length(), weak.length()); i++) {
    if (isDisplayable(dominant[i])) Serial.print(dominant[i]);
    else if (isDisplayable(weak[i])) Serial.print(weak[i]);
    else Serial.print("?");
  }
  Serial.print("");
  memset(binary, 0, sizeof(binary));
}

// check if ASCII is printable
bool isDisplayable(char c) {
  return (c >= 32 && c <= 126);
}
