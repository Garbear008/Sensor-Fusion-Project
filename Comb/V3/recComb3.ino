// ---------- Hardware pins ----------
const int ldrPin = A0;    // Analog pin A0 connected to the LDR (Light Dependent Resistor) for light detection
const int buttonPin = 2;  // Digital pin D2 connected to button that allows user to stop automatic messaging
const int ledRecPin = 3;  // Digital pin D3 connected to an LED to indicate reception mode/status
const int micPin = A1;    // Analog pin A1 connected to a microphone for sound detection

// ---------- Light reception timing & setup ----------
// period: time between sampling bits when reading light
// durationToStart: how long to sample ambient light during startup before deciding threshold
// previousLight: state-tracking helper (LED is on before each byte, so start true)
const int period = 2;               // period in milliseconds for each bit delay
const long durationToStart = 2000;  // time in microseconds to set up the LDR
bool previousLight = true;          // because the LED is on before each byte

// ---------- Sound timing ----------
// dotDuration: short beep timing for "1-bit" sound mode
// fourDotDuration: larger timing unit used for the 4-bit sound representation
const long dotDuration = 100;       // time duration for each short beep (dot or "0" in binary)
const long fourDotDuration = 2000;  // time duration for each short beep (dot or "0" in binary)

// How we sample the microphone: total sampling window and interval between samples
const long durationMicros = 20;       // Duration (in microseconds) for sampling sound once
const long sampleIntervalMicros = 2;  // Interval (in microseconds) between consecutive sound samples

const long bitGap = fourDotDuration *0.5;  // Gap between consecutive bits in 4-bit mode
const long charGap = fourDotDuration * 2;   // Gap between characters (or after each byte) in 4-bit mode

// ---------- Debug / display / thresholds ----------
// plotLight / plotSound: enable serial plot outputs for troubleshooting
// lightThreshold / soundThreshold: initial placeholders; recalculated in setup()
// toDebug: toggles verbose serial prints used throughout the code
long lightThreshold = 100;      // Initial placeholder for light threshold; later recalculated dynamically
long soundThreshold = 150;      // Initial placeholder for sound threshold; later recalculated dynamically
int displayMessageLength = 50;  // Max length of displayed message before clearing
bool toDebug = false;            // If true, prints detailed debug info
bool plotLight = false;         // If true, prints light readings to Serial Plotter for debugging
bool plotSound = false;         // If true, prints sound readings to Serial Plotter for debugging

// ---------- Internal state counters and flags (do not touch) ----------
// These track mistakes, counters, modes, and help the protocol decide what to do.
int numSoundCrossing = 0;           // Number of times the sound waveform crosses the threshold during a single beep
int mistakesLightCounter = 0;       // Mistakes in light reception
int mistakesSoundOneCounter = 0;    // Mistakes in one-bit sound reception
int mistakesSoundFourCounter = 0;   // Mistakes in four-bit sound reception
int messageCounter = 0;             // Counter for messages received
bool ifByteCorrect = true;          // Flag indicating if last byte received was correct
bool ifSoundOneBit = false;         // Flag for 1-bit sound mode
bool ifSoundFourBit = false;        // Flag for 4-bit sound mode
bool checkSumLight = false;         // Flag indicating next received light byte is checksum
bool checkSumSoundOneBit = false;   // Flag indicating next 1-bit sound byte is checksum
bool checkSumSoundFourBit = false;  // Flag indicating next 4-bit sound byte is checksum

// ---------- Buffers for incoming characters (strings) ----------
// real* = every decoded char
// *String = displayable chars that pass checks
// trash* = decoded but non-displayable / likely corrupted chars
String lightString = "";             // Stores displayable light-received characters
String soundOneString = "";          // Stores displayable 1-bit sound-received characters
String soundFourString = "";         // Stores displayable 4-bit sound-received characters
String realLightString = "";         // Stores all actual light-received characters
String realSoundOneString = "";      // Stores all actual 1-bit sound-received characters
String realSoundFourString = "";     // Stores all actual 4-bit sound-received characters
String trashLightString = "";        // Stores unreadable/mistaken light characters
String trashSoundOneString = "";     // Stores unreadable/mistaken 1-bit sound characters
String trashSoundFourString = "";    // Stores unreadable/mistaken 4-bit sound characters
String displayLightString = "";      // String to be printed/displayed for light
String displaySoundOneString = "";   // String to be printed/displayed for 1-bit sound
String displaySoundFourString = "";  // String to be printed/displayed for 4-bit sound

// ---------- Temporary binary buffers for decoded bits ----------
// For sound modes, bits are accumulated in arrays before forming a 12-bit Hamming code
int binaryOneBit[12];                                            // Stores bits for 1-bit sound decoding
int binaryFourBit[12];                                           // Stores bits for 4-bit sound decoding
int indexOneBit = 0;                                             // Index tracker for binaryOneBit array
int indexFourBit = 0;                                            // Index tracker for binaryFourBit array
bool pickUpSound(int durationMicros, int sampleIntervalMicros);  // to declare function title for pickUpSound

// ----------------------- SETUP -----------------------
// Lower-level ADC prescaler tweak improves analogRead timing; pins initialized; thresholds set.
void setup() {
  ADCSRA = (ADCSRA & 0b11111000) | 0b011;  //prescaler 8 (~8us per analogRead)
  pinMode(ldrPin, INPUT);
  pinMode(micPin, INPUT);
  pinMode(ledRecPin, OUTPUT);        // LED will be on or off based on the medium of reception
  pinMode(buttonPin, INPUT_PULLUP);  // button will be pressed to switch between light and sound transmission
  Serial.begin(2000000);
  Serial.println("Not yet ready for Code...");  // customize the message to see fit
  digitalWrite(ledRecPin, LOW);                 // make sure the LED is not on on the reciever end
  // get the ambient light and the sound threshold
  lightThreshold = getLightThreshold() + 50;             // get the ambient light (with buffer)
  soundThreshold = sampleSound(durationToStart, 1) + 5;  // get the sound threshold (with buffer)
  if (toDebug) {
    Serial.println(lightThreshold);  // to see the light threshold value
    Serial.println(soundThreshold);  // to see the sound threshold value
  }
  while (analogRead(ldrPin) < lightThreshold) {
    delay(100);
  }
  Serial.println("Now ready for Code...");  // customize the message to see fit
  Serial.println("");
}

// =================================================================================
// LOOP
// =================================================================================
void loop() {
  // default LED off until sound mode sets it HIGH
  digitalWrite(ledRecPin, LOW);

  // Light reception: senseLight() returns true if brightness > lightThreshold.
  // Compare to previousLight to detect a falling edge (on -> off) that signals the start of a byte from the transmitter.
  bool currentLight = senseLight();  // detects current state of the LED in the transmitter

  // -------------------------
  // LIGHT MODE (edge-detect)
  // -------------------------
  // Condition detects transition from "previously on" to "now off" — that falling edge triggers getLightByte() to read the 12-bit Hamming-coded byte.
  if (!currentLight && previousLight)  // while it was on and now it is off
  {
    char c = getLightByte();  // get byte for that character using binary code

    // If the incoming byte is a checksum marker (0) we set checkSumLight and compare the checksum. Otherwise we append the character into buffers.
    if (!checkSumLight) {
      appendItems(c, 1);
      if (toDebug) {
        Serial.print("Light Character: ");
        Serial.println(c);
        Serial.println("===============");
      }
    } else {
      // checksum handling: getCheckSum(1) returns the checksum (side-effect: also prints?)
      if (getCheckSum(1) != int(c)) {
        Serial.print("Checksum from decoded message: ");
        getCheckSum(1);
        Serial.print("Checksum byte in decimal: ");
        Serial.println(int(c));
        Serial.println("Error in message by light!!!");
      }
      checkSumLight = false;
    }
  }
  // update previousLight for next loop
  previousLight = currentLight;  // to reset the light state (for light reception)

  // -------------------------
  // SOUND MODE
  // -------------------------
  // pickUpSound() detects if the environment has a sound spike above soundThreshold.
  // When a sound is detected, we go into a loop to count crossings (numSoundCrossing) and wait for a stable silence to determine the duration of the sound.
  if (pickUpSound()) {  // if sound is detected
    digitalWrite(ledRecPin, HIGH);
    unsigned long start = micros();  // capture start time of the sound
    numSoundCrossing = 1;
    while (true) {
      if (!pickUpSound()) {
        unsigned long silenceStart = micros();
        bool stableSilence = true;
        numSoundCrossing++;
        // Wait for bitGap of silence to be sure the sound ended. If sound comes back, mark it unstable and continue counting.
        while (micros() - silenceStart < bitGap) {
          if (pickUpSound()) {
            stableSilence = false;
            numSoundCrossing++;
            break;
          }
        }
        if (stableSilence) break;
      }
    }
    // duration is computed as elapsed micros minus a single bitGap
    unsigned long duration = micros() - start - bitGap;  // how long the sound lasts
    if (toDebug) {
      Serial.print("Duration: ");
      Serial.println(duration);
      Serial.print("Number of crossings (sound): ");
      Serial.println(numSoundCrossing);
    }

    // Convert the measured duration and crossing count into binary bits appended to
    // the appropriate arrays (binaryOneBit / binaryFourBit).
    translateIntoBinary(duration);

    // After finishing translating the burst to bits, wait for a full character gap to trigger decoding of the byte in decodeBinary().
    unsigned long gapStart = micros();
    detectCharGap(gapStart);
  }

  // -------------------------
  // SERIAL COMMANDS (manual prints / clears)
  // -------------------------
  if (Serial.available()) {
    char input = Serial.read();
    if (input == 'l') {
      Serial.println("");
      Serial.print("Printing light");
      printOut(displayLightString);
      clearMem();
    } else if (input == 's') {
      Serial.println("");
      Serial.print("Printing sound (all) (one bit)...");
      printOut(displaySoundOneString);
      clearMem();
    } else if (input == 'f') {
      Serial.println("");
      Serial.print("Printing sound (all) (four bit)");
      printOut(displaySoundFourString);
      clearMem();
    } else if (input == 'c') {
      printAll();
    } else if (input == 'd') {
      // resets many of the runtime buffers and counters
      lightString = "";
      soundOneString = "";
      soundFourString = "";
      realLightString = "";
      realSoundOneString = "";
      realSoundFourString = "";
      trashLightString = "";
      trashSoundOneString = "";
      trashSoundFourString = "";
      displayLightString = "";
      displaySoundOneString = "";
      displaySoundFourString = "";
      mistakesLightCounter = 0;
      mistakesSoundOneCounter = 0;
      mistakesSoundFourCounter = 0;
      messageCounter = 0;
      clearMem();
      Serial.println("All messages have been deleted");
    }
  }

  // If hardware button pressed, call printAll() (provides a manual dump)
  if (digitalRead(buttonPin) == LOW) {
    printAll();
  }

  // Prevent display buffers from growing unbounded — truncate and reset counters when they exceed displayMessageLength. This keeps Serial output reasonable.
  if (displayLightString.length() > displayMessageLength) {
    displayLightString = "";
    mistakesLightCounter = 0;
  }
  if (displaySoundOneString.length() > displayMessageLength) {
    displaySoundOneString = "";
    mistakesSoundOneCounter = 0;
  }
  if (displaySoundFourString.length() > displayMessageLength) {
    displaySoundFourString = "";
    mistakesSoundFourCounter = 0;
  }
}

// ================= LIGHT FUNCTIONS =================
// Returns true when the LDR reading is above the calibrated threshold.
// When plotLight is true the raw analog value is also printed for debugging.
bool senseLight() {
  int val = analogRead(ldrPin);  // reads the instantaneous light brightness in surroundings
  if (plotLight) {
    Serial.println(val);  // for troubleshooting
  }
  return val > lightThreshold;  // true if the brightness exceeds the threshold
}

// Reads 12 bits (Hamming-coded) from the light channel and returns the decoded char.
// Note: It delays between bits using 'period' to match transmitter timing.
char getLightByte() {
  uint16_t byte12 = 0;
  int count = 0;
  delay(period * 1.5);  // wait after start bit
  if (toDebug) {
    Serial.print("Received byte: ");
  }
  for (int i = 11; i >= 0; i--) {
    bool bit = senseLight();  // read a new bit every loop
    if (toDebug) {
      Serial.print(bit);
    }
    count += bit;
    byte12 |= (bit << i);  // assigns each bit of c to a 0 (if light is not sensed) or 1 (if sensed)
    delay(period);         // delay between each bit
  }
  if (toDebug) {
    Serial.println("");
  }
  char c = (char)hammingDecode(byte12, 1);
  return c;
}

// Samples ambient light for durationToStart ms and returns the average value + 1
// (used to compute the lightThreshold at startup).
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

// ================= SOUND FUNCTIONS =================
// pickUpSound returns true when a brief sampling window detects a peak above soundThreshold.
bool pickUpSound() {
  int sound = sampleSound(durationMicros, sampleIntervalMicros);  // get the peak volume
  return sound > soundThreshold;                                  // true if the difference (between generated sound and ambient noise) is higher than the threshold
}

// sampleSound samples the mic for 'durationMicros', spaced by 'sampleIntervalMicros'
// and returns the peak amplitude seen during that sampling window.
int sampleSound(int durationMicros, int sampleIntervalMicros) {
  int bias = 0;
  int peakAmplitude = 0;
  unsigned long startTime = micros();  // get the start time

  // for the duration of sampling
  while (micros() - startTime < durationMicros) {
    int sample = analogRead(micPin);     // reads the instantaneous volume of sound in surroundings
    int deviation = abs(sample - bias);  // difference between sample and 512
    if (deviation > peakAmplitude) {
      peakAmplitude = deviation;  // to get the highest deviation
    }
    delayMicroseconds(sampleIntervalMicros);  // how much delay after each sample
  }
  if (plotSound) {
    Serial.println(peakAmplitude);  // for troubleshooting
  }
  return peakAmplitude;
}

// decodeBinary reconstructs a 12-bit code from either binaryOneBit or binaryFourBit arrays,
// runs Hamming decode, then either appends to buffers or checks checksum depending on flags.
void decodeBinary(int mode) {
  uint16_t byte12 = 0;
  if (toDebug) {
    Serial.print("Received byte: ");
  }
  if (mode == 2) {
    for (int i = 0; i < 12; i++) {
      int n = binaryOneBit[i];  // gets the number from each bit slot
      if (n == 0) {
        byte12 |= (0 << (11 - i));  // sets the (7-i)th index to 0
        if (toDebug) {
          Serial.print(0);
        }
      } else {
        byte12 |= (1 << (11 - i));  // sets the (7-i)th index to 1
        if (toDebug) {
          Serial.print(1);
        }
      }
    }
    if (toDebug) {
      Serial.println("");
    }
    char c = (char)hammingDecode(byte12, 2);
    if (!checkSumSoundOneBit) {
      appendItems(c, 2);
      if (toDebug) {
        Serial.print("Sound1 Character: ");
        Serial.println(c);
        Serial.println("===============");
      }
    } else {
      if (getCheckSum(2) != int(c)) {
        Serial.print("Checksum from decoded message: ");
        Serial.println(getCheckSum(2));
        Serial.print("Checksum byte in decimal: ");
        Serial.println(int(c));
        Serial.println("Error in message by sound (one bit)!!!");
      }
      checkSumSoundOneBit = false;
    }

  } else if (mode == 3) {
    for (int i = 0; i < 12; i++) {
      int n = binaryFourBit[i];  // gets the number from each bit slot
      if (n == 0) {
        byte12 |= (0 << (11 - i));  // sets the (7-i)th index to 0
        if (toDebug) {
          Serial.print(0);
        }
      } else {
        byte12 |= (1 << (11 - i));  // sets the (7-i)th index to 1
        if (toDebug) {
          Serial.print(1);
        }
      }
    }
    if (toDebug) {
      Serial.println("");
    }
    char c = (char)hammingDecode(byte12, 3);
    if (!checkSumSoundFourBit) {
      appendItems(c, 3);
      if (toDebug) {
        Serial.print("Sound4 Character: ");
        Serial.println(c);
        Serial.println("===============");
      }
    } else {
      if (getCheckSum(3) != int(c)) {
        Serial.print("Checksum from decoded message: ");
        Serial.println(getCheckSum(3));
        Serial.print("Checksum byte in decimal: ");
        Serial.println(int(c));
        Serial.println("Error in message by sound (four bit)!!!");
      }
      messageCounter++;
      printAll();
      checkSumSoundFourBit = false;
    }
  }
  clearMem();
}

// detectCharGap waits for a long silence after the last beep and then decodes the accumulated bits.
void detectCharGap(unsigned long gapStart) {
  if (ifSoundOneBit) {
    while (!pickUpSound()) {
      // detects for the absence of sound (gap)
      if (micros() - gapStart > charGap) {  // run this at the end of each byte to separate characters
        decodeBinary(2);                    // decodes the byte array
        break;
      }
    }
  }
  if (ifSoundFourBit) {
    while (!pickUpSound()) {                // detects for the absence of sound (gap)
      if (micros() - gapStart > charGap) {  // run this at the end of each byte to separate characters
        decodeBinary(3);                    // decodes the byte array
        break;
      }
    }
  }
}

//==================== DISPLAY FUNCTIONS ========================

// printOut prints a string and clears the temporary binary buffers (clearMem).
void printOut(String string) {
  Serial.println(string);
  clearMem();
}

// isDisplayable limits which ASCII characters we consider printable on the display.
bool isDisplayable(char c) {
  return (c >= 32 && c <= 126);
}

// clearMem zeroes the working binary arrays after a byte is handled.
void clearMem() {
  memset(binaryOneBit, 0, sizeof(binaryOneBit));
  memset(binaryFourBit, 0, sizeof(binaryFourBit));
  indexOneBit = 0; 
  indexFourBit = 0; 
}

// translateIntoBinary converts a sound duration and crossing count into bits, filling either binaryOneBit (1-bit mode) or binaryFourBit (4-bit mode).
void translateIntoBinary(unsigned long duration) {
  int soundPeriod = 2 * (duration) / (numSoundCrossing);
  numSoundCrossing = 0;
  int freq = 1000000 / (soundPeriod);
  if (toDebug) {
    Serial.print("Freq: ");
    Serial.print(freq);
    Serial.print(" --------> ");
  }
  // resets index of the array of greater than or equal to 8 for number of bits in a byte
  if (indexOneBit >= 12) {
    indexOneBit %= 12;
  }
  if (indexFourBit >= 12) {
    indexFourBit %= 12;
  }
  if (duration < dotDuration * 5 / 3) {
    binaryOneBit[indexOneBit] = 0;
    indexOneBit++;
    ifSoundOneBit = true;
    ifSoundFourBit = false;
    if (toDebug) {
      Serial.println("oneBitMode");
    }
  } else if (duration < fourDotDuration * 7 / 8) {
    binaryOneBit[indexOneBit] = 1;
    indexOneBit++;
    ifSoundOneBit = true;
    ifSoundFourBit = false;
    if (toDebug) {
      Serial.println("oneBitMode");
    }
  }
  // to distinguish between short beep and long beep
  else if (duration < (fourDotDuration + fourDotDuration / 8)) {
    ifSoundOneBit = false;
    ifSoundFourBit = true;
    if (toDebug) {
      Serial.println("fourBitMode");
    }
    binaryFourBit[indexFourBit] = 0;
    if (freq < 1600) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 1700) {  // never used
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 1850) {  // never used
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 2100) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 2300) {  // never used
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 2560) {
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 3100) {
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 0;
    } else {  // never used
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 1;
    }
    indexFourBit += 4;  // goes to next bit space in the byte array
  } else {
    ifSoundOneBit = false;
    ifSoundFourBit = true;
    if (toDebug) {
      Serial.println("fourBitMode");
    }
    binaryFourBit[indexFourBit] = 1;
    if (freq < 1300) {  // never used
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 1550) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 1800) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 1900) {  // never used
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 2150) {
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 2300) {  // never used
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 2600) {  // never used
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 0;
    } else {
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 1;
    }
    indexFourBit += 4;  // goes to next bit space in the byte array
  }
}


// appendItems routes decoded characters to the right buffers.
// It also recognizes checksum-marker bytes (int(c) == 0) and flips the checksum mode flags.
void appendItems(char c, int mode) {
  if (int(c) == 0) {
    if (mode == 1) {
      checkSumLight = true;
    }
    if (mode == 2) {
      checkSumSoundOneBit = true;
    }
    if (mode == 3) {
      checkSumSoundFourBit = true;
    }
  }
  if (mode == 1) {
    if (!checkSumLight) {
      realLightString += c;
      if (ifByteCorrect && isDisplayable(c)) {
        lightString += c;
      } else {
        trashLightString += c;
      }
    }
  } else if (mode == 2) {
    if (!checkSumSoundOneBit) {
      realSoundOneString += c;
      if (ifByteCorrect && isDisplayable(c)) {
        soundOneString += c;
      } else {
        trashSoundOneString += c;
      }
    }
  } else if (mode == 3) {
    if (!checkSumSoundFourBit) {
      realSoundFourString += c;
      if (ifByteCorrect && isDisplayable(c)) {
        soundFourString += c;
      } else {
        trashSoundFourString += c;
      }
    }
  }
}

// ========================================
// FUNCTION: getCheckSum
// Computes a checksum for received messages depending on mode
// Mode 1 = Light, Mode 2 = 1-bit Sound, Mode 3 = 4-bit Sound
// Checksum is calculated by counting 1s in two specific bit positions across all characters, taking modulo 12, and multiplying the counts together.
// The function also appends the received string to the display buffer and clears the temporary buffer
// ========================================
int getCheckSum(int mode) {
  if (mode == 1) {                                    // Light mode
    int countOne = 0;                                 // Count of 1s in the first selected bit position
    int countTwo = 0;                                 // Count of 1s in the second selected bit position
    int indexOne = 1;                                 // Bit index for first count (MSB=7, LSB=0)
    int indexTwo = 5;                                 // Bit index for second count
    for (int i = 0; i < lightString.length(); i++) {  // Iterate over all characters in lightString
      if (bitRead(lightString.charAt(i), 7 - indexOne) == 1) {
        countOne++;  // Increment countOne if bit at indexOne is 1
      }
      if (bitRead(lightString.charAt(i), 7 - indexTwo) == 1) {
        countTwo++;  // Increment countTwo if bit at indexTwo is 1
      }
    }
    countOne %= 12;                                 // Ensure countOne stays within 0-11
    countTwo %= 12;                                 // Ensure countTwo stays within 0-11
    int countMult = int(countOne) * int(countTwo);  // Multiply the two counts to create checksum
    if (toDebug) {                                  // Optional debug print
      Serial.print("Light Checksum: ");
      Serial.println(countMult);
      Serial.println("===============");
    }
    displayLightString += lightString;  // Append processed characters to display buffer
    lightString = "";                   // Clear temporary string buffer
    return countMult;                   // Return the computed checksum
  } else if (mode == 2) {               // 1-bit sound mode
    int countOne = 0;
    int countTwo = 0;
    int indexOne = 1;
    int indexTwo = 5;
    for (int i = 0; i < soundOneString.length(); i++) {
      if (bitRead(soundOneString.charAt(i), 7 - indexOne) == 1) {
        countOne++;
      }
      if (bitRead(soundOneString.charAt(i), 7 - indexTwo) == 1) {
        countTwo++;
      }
    }
    countOne %= 12;
    countTwo %= 12;
    int countMult = int(countOne) * int(countTwo);
    if (toDebug) {
      Serial.print("Sound1 Checksum: ");
      Serial.println(countMult);
      Serial.println("===============");
    }
    displaySoundOneString += soundOneString;  // Append processed string to display buffer
    soundOneString = "";                      // Clear temporary buffer
    return countMult;
  } else if (mode == 3) {  // 4-bit sound mode
    int countOne = 0;
    int countTwo = 0;
    int indexOne = 1;
    int indexTwo = 5;
    for (int i = 0; i < soundFourString.length(); i++) {
      if (bitRead(soundFourString.charAt(i), 7 - indexOne) == 1) {
        countOne++;
      }
      if (bitRead(soundFourString.charAt(i), 7 - indexTwo) == 1) {
        countTwo++;
      }
    }
    countOne %= 12;
    countTwo %= 12;
    int countMult = int(countOne) * int(countTwo);
    if (toDebug) {
      Serial.print("Sound4 Checksum: ");
      Serial.println(countMult);
      Serial.println("===============");
    }
    displaySoundFourString += soundFourString;  // Append processed string to display buffer
    soundFourString = "";                       // Clear temporary buffer
    return countMult;
  }
}

// ========================================
// FUNCTION: hammingDecode
// Implements Hamming (11,7) error detection and correction with overall parity bit
// Input: 12-bit integer where:
// bit 0 = overall parity (parity of bits 1-11)
// bits 1-11 = Hamming encoded data
// Output: 7-bit decoded data (uint8_t)
// This function also detects single-bit, double-bit, parity-bit errors, and flags suspicious multi-bit errors
// ========================================
uint8_t hammingDecode(uint16_t code12, int mode) {
  ifByteCorrect = true;  // Assume byte is correct initially
  bool h[12];            // Array to store individual bits of code12
  for (int i = 0; i < 12; i++) {
    h[i] = (code12 >> i) & 1;  // Extract each bit from code12 into h[0..11]
  }

  // Calculate syndrome bits for single-bit error detection
  int s1 = h[1] ^ h[3] ^ h[5] ^ h[7] ^ h[9] ^ h[11];
  int s2 = h[2] ^ h[3] ^ h[6] ^ h[7] ^ h[10] ^ h[11];
  int s4 = h[4] ^ h[5] ^ h[6] ^ h[7];
  int s8 = h[8] ^ h[9] ^ h[10] ^ h[11];

  int syndrome = s1 + (s2 << 1) + (s4 << 2) + (s8 << 3);  // Syndrome value points to error bit (1-based)

  // Compute overall parity from bits 1-11
  bool overall = 0;
  for (int i = 1; i <= 11; i++) overall ^= h[i];  // XOR all data/parity bits

  bool overallMismatch = (overall != h[0]);  // True if overall parity does not match bit 0

  if (syndrome == 0 && !overallMismatch) {
    // No error detected; all bits are correct
  } else if (syndrome > 0 && !overallMismatch) {
    // Nonzero syndrome but overall parity correct → indicates ≥3-bit error (cannot correct)
    ifByteCorrect = false;
    if (mode == 1) Serial.print("Light... ");
    else if (mode == 2) Serial.print("Sound1... ");
    else if (mode == 3) Serial.print("Sound4... ");
    Serial.println("Suspicious syndrome (possible triple-bit error)");
  } else if (syndrome == 0 && overallMismatch) {
    // Only overall parity bit is flipped; data is otherwise correct
    if (mode == 1) Serial.print("Light... ");
    else if (mode == 2) Serial.print("Sound1... ");
    else if (mode == 3) Serial.print("Sound4... ");
    Serial.println("Parity bit error detected (safe)");
    h[0] = overall;  // Correct the parity bit
  } else if (syndrome > 0 && overallMismatch) {
    // Single-bit error detected; correct the bit at position indicated by syndrome
    h[syndrome] = !h[syndrome];
    if (mode == 1) Serial.print("Light... ");
    else if (mode == 2) Serial.print("Sound1... ");
    else if (mode == 3) Serial.print("Sound4... ");
    Serial.print("Single-bit error corrected at position: ");
    Serial.println(syndrome);
  } else {
    // Double-bit error detected; cannot correct reliably
    ifByteCorrect = false;
    if (mode == 1) Serial.print("Light... ");
    else if (mode == 2) Serial.print("Sound1... ");
    else if (mode == 3) Serial.print("Sound4... ");
    Serial.println("Double-bit error detected! Data may be corrupted.");
  }

  // Extract the 7 data bits from positions of Hamming code (d1-d7)
  uint8_t data7 = 0;
  data7 |= h[3] << 0;   // d1 = h[3]
  data7 |= h[5] << 1;   // d2 = h[5]
  data7 |= h[6] << 2;   // d3 = h[6]
  data7 |= h[7] << 3;   // d4 = h[7]
  data7 |= h[9] << 4;   // d5 = h[9]
  data7 |= h[10] << 5;  // d6 = h[10]
  data7 |= h[11] << 6;  // d7 = h[11]
  return data7;         // Return the decoded 7-bit data
}

// ========================================
// FUNCTION: printAll
// Displays all received data, reliability statistics, and clears memory
// Computes the percentage of correct characters since last clear
// Maintains counters of mistakes for each channel
// Clears the temporary and permanent buffers when display length exceeds displayMessageLength
// ========================================
void printAll() {
  // Compute percentage of presumably correct characters since last clear
  double correctLightPercentage = (double)(realLightString.length() - trashLightString.length()) / (realLightString.length());
  double correctSoundOnePercentage = (double)(realSoundOneString.length() - trashSoundOneString.length()) / (realSoundOneString.length());
  double correctSoundFourPercentage = (double)(realSoundFourString.length() - +trashSoundFourString.length()) / (realSoundFourString.length());

  // Increment mistake counters if any corruption detected
  if (correctLightPercentage < 1) {
    mistakesLightCounter++;
  }
  if (correctSoundOnePercentage < 1) {
    mistakesSoundOneCounter++;
  }
  if (correctSoundFourPercentage < 1) {
    mistakesSoundFourCounter++;
  }

  // Clear display buffers if they exceed maximum length
  if (displayLightString.length() > displayMessageLength) {
    displayLightString = "";
    mistakesLightCounter = 0;
  }
  if (displaySoundOneString.length() > displayMessageLength) {
    displaySoundOneString = "";
    mistakesSoundOneCounter = 0;
  }
  if (displaySoundFourString.length() > displayMessageLength) {
    displaySoundFourString = "";
    mistakesSoundFourCounter = 0;
  }

  Serial.println("");              // Blank line for readability
  Serial.println(messageCounter);  // Show message sequence number

  // -------------------------------
  // Display Light data and reliability info
  // -------------------------------
  Serial.print("Light (all): ");
  if (mistakesLightCounter == 0) {
    Serial.print("(reliable channel) -----> ");
  }
  Serial.println(displayLightString);
  Serial.print("Light (since last cleared): ");
  Serial.println(realLightString);
  Serial.print("Light (trash since last cleared): ");
  Serial.println(trashLightString);
  if (realLightString.length() != 0) {
    Serial.print(correctLightPercentage * 100);
    Serial.println("% presumably correct since last cleared");
  }
  Serial.println("");

  // -------------------------------
  // Display 1-bit Sound data
  // -------------------------------
  Serial.print("Sound (all) (One Bit): ");
  if (mistakesSoundOneCounter == 0) {
    Serial.print("(reliable channel) -----> ");
  }
  Serial.println(displaySoundOneString);
  Serial.print("Sound (since last cleared) (One Bit): ");
  Serial.println(realSoundOneString);
  Serial.print("Sound (trash since last cleared) (One Bit): ");
  Serial.println(trashSoundOneString);
  if (realSoundOneString.length() != 0) {
    Serial.print(correctSoundOnePercentage * 100);
    Serial.println("% presumably correct since last cleared");
  }
  Serial.println("");

  // -------------------------------
  // Display 4-bit Sound data
  // -------------------------------
  Serial.print("Sound (all) (Four Bit): ");
  if (mistakesSoundFourCounter == 0) {
    Serial.print("(reliable channel) -----> ");
  }
  Serial.println(displaySoundFourString);
  Serial.print("Sound (since last cleared) (Four Bit): ");
  Serial.println(realSoundFourString);
  Serial.print("Sound (trash since last cleared) (Four Bit): ");
  Serial.println(trashSoundFourString);
  if (realSoundFourString.length() != 0) {
    Serial.print(correctSoundFourPercentage * 100);
    Serial.println("% presumably correct since last cleared");
  }
  Serial.println("");

  // Clear temporary buffers after printing
  lightString = "";
  soundOneString = "";
  soundFourString = "";
  realLightString = "";
  realSoundOneString = "";
  realSoundFourString = "";
  trashLightString = "";
  trashSoundOneString = "";
  trashSoundFourString = "";

  clearMem();                        // Clear any other memory structures
  Serial.println("Memory cleared");  // Indicate that buffers are cleared
}