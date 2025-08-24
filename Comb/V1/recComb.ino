// BOARD
// Pin definitions for the receiver hardware:
// - ldrPin: analog input connected to an LDR (light-dependent resistor) to receive light signals
// - ledRecPin: digital output used as a status LED to show reception/activity
// - micPin: analog input connected to microphone / audio detection circuit
const int ldrPin = A0;    // pin designation (A0) for LDR
const int buttonPin = 2;  // pin designation (D2) for the button
const int ledRecPin = 3;  // pin designation (D3) for LED
const int micPin = A1;    // pin designation (A1) for microphone

// =======================
// Light transmission parameters
// =======================

// 'period' is the inter-bit delay used while sampling light bits (in milliseconds).
// It should match the transmitter's timing to reliably sample each bit.
const int period = 3;  // period in milliseconds for each bit delay

// How long (microseconds) to spend warming up / measuring ambient light before starting.
// Used in getLightThreshold() to collect ambient light baseline.
const long durationToStart = 2000;  // time in microseconds to set up the LDR

// previousLight tracks the last read light state so the code can detect falling edges
// (i.e., the transition from LED on -> off) which mark the start/stop bits.
bool previousLight = true;  // because the LED is on before each byte

// =======================
// Sound transmission parameters
// =======================

// dotDuration: duration (microseconds) considered a "short beep" for one-bit sound mode.
const long dotDuration = 750;  // time duration for each short beep (dot or "0" in binary)

// fourDotDuration: reference duration for the four-bit sound encoding scheme.
const long fourDotDuration = 5000;  // time duration for each short beep (dot or "0" in binary)

// durationMicros: total sampling window length (microseconds).
// sampleIntervalMicros: time between analogRead() samples (microseconds).
const long durationMicros = 50;       // time in microseconds to complete the sampling of the sound
const long sampleIntervalMicros = 2;  // time in microseconds it takes between each sample

// Timing used to detect bit gaps and character gaps in sound mode.
const long bitGap = fourDotDuration * 0.5;
const long charGap = fourDotDuration * 1.5;  // gap in time for each character (or after each byte)

// =======================
// Plotting / thresholds / state tracking
// =======================

// plotLight / plotSound: if true, sample values are printed to Serial
// in the Serial Plotter for debugging (light values 0-1023 from analogRead, sound peak amplitude).
bool plotLight = false;  // true if want to visualize sound on serial plotter (for troubleshooting)
bool plotSound = false;  // true if want to visualize sound on serial plotter (for troubleshooting)

// Thresholds for deciding whether a light or sound event is present.
// They are initialized at startup by sampling ambient conditions, but can be overridden.
long lightThreshold = 100;  // random initial number; replaced at setup with ambient + buffer
long soundThreshold = 150;  // random initial number; replaced at setup with ambient + buffer

// Counters and flags for error handling, mode switching, and checksum handling.
int numSoundCrossing = 0;       // number of crossings across threshold for sound (used in translateIntoBinary)
int mistakeCounter = 0;         // counter to switch modes after repeated errors detected by parity
bool correctByParity = true;    // set by checkParity() after decoding a byte
bool lightMode = true;          // start in light reception mode (true = prefer light output to displayString)
bool soundOneBitMode = false;   // indicates receiver is currently showing 1-bit sound decoded chars
bool soundFourBitMode = false;  // indicates receiver is currently showing 4-bit sound decoded chars
bool ifSoundOneBit = false;     // temporary flags used while decoding to know which sound mode was active
bool ifSoundFourBit = false;
bool checkSumLight = false;         // if true, next char will be treated as checksum for light channel
bool checkSumSoundOneBit = false;   // same for 1-bit sound channel
bool checkSumSoundFourBit = false;  // same for 4-bit sound channel
int checkSumCounter = 0;            // unused counter placeholder for checksum logic (kept for compatibility)

// =======================
// Buffers / strings used to store received characters
// - real*String: all characters received (includes everything, used for diagnostics)
// - display*String: characters that are shown to the user when mode is active
// - *String: temporary strings used to accumulate a block used for checksum calculation
// - trash*String: characters failed parity check and considered corrupted
// =======================

String realLightString = "";
String realSoundOneString = "";
String realSoundFourString = "";
String lightString = "";       // accumulates valid light-mode characters used to compute checksum
String soundOneString = "";    // accumulates valid one-bit sound characters used to compute checksum
String soundFourString = "";   // accumulates valid four-bit sound characters used to compute checksum
String trashLightString = "";  // characters that failed parity check for light mode
String trashSoundOneString = "";
String trashSoundFourString = "";
String displayLightString = "";      // what gets displayed when user asks for 'l'
String displaySoundOneString = "";   // what gets displayed when user asks for 's'
String displaySoundFourString = "";  // what gets displayed when user asks for 'f'
String displayString = "";           // overall message that reflects the active display mode(s)

// =======================
// Binary buffers for sound decoding
// - binaryOneBit: stores 8 bits decoded in 1-bit-per-sound mode
// - binaryFourBit: stores 8 bits decoded in 4-bit-per-sound mode
// =======================
int binaryOneBit[8];
int binaryFourBit[8];
int indexOneBit = 0;   // current write index into binaryOneBit (wraps at 8)
int indexFourBit = 0;  // current write index into binaryFourBit (wraps at 8)

// Forward declaration for pickUpSound variant that takes timing parameters.
bool pickUpSound(int durationMicros, int sampleIntervalMicros);  // to declare function title for pickUpSound

// =======================
// SETUP
// =======================
void setup() {
  // Reduce the ADC prescaler bits to make analogRead faster (prescaler = 8).
  ADCSRA = (ADCSRA & 0b11111000) | 0b011;  //prescaler 8 (~8us per analogRead)

  // Configure pins
  pinMode(ldrPin, INPUT);
  pinMode(micPin, INPUT);
  pinMode(ledRecPin, OUTPUT);        // LED used to indicate reception activity
  pinMode(buttonPin, INPUT_PULLUP);  // physical button (internal pull-up) – pressed = LOW

  // Make sure the terminal supports that speed.
  Serial.begin(2000000);
  Serial.println("Not yet ready for Code...");  // user-visible boot message

  // Ensure the receiver status LED is off at start.
  digitalWrite(ledRecPin, LOW);  // make sure the LED is not on on the receiver end

  // Calibrate thresholds from ambient values:
  // - lightThreshold: average ambient light + buffer (so small fluctuations aren't read as signal)
  // - soundThreshold: peak amplitude from a quick sample + buffer
  lightThreshold = getLightThreshold() + 50;             // get the ambient light (with buffer)
  Serial.println(lightThreshold);                        // print calibrated light threshold
  soundThreshold = sampleSound(durationToStart, 1) + 5;  // get the sound threshold (with buffer)
  Serial.println(soundThreshold);                        // print calibrated sound threshold

  // Wait until a transmitter turns the light on (ensure the sender is ready).
  // This blocks setup until the LDR sees a value above lightThreshold.
  while (analogRead(ldrPin) < lightThreshold) {
    delay(100);
  }
  Serial.println("Now ready for Code...");  // ready message
}

// =======================
// LOOP
// =======================
void loop() {
  // Ensure status LED is off at loop start; it will be toggled when sound is detected.
  digitalWrite(ledRecPin, LOW);

  // Read current light state (true if brighter than threshold).
  bool currentLight = senseLight();  // detects current state of the LED in the transmitter

  // -----------------------
  // LIGHT MODE:
  // -----------------------
  // Look for a falling edge: LED was previously ON and is now OFF.
  // That transition indicates the end of a start/stop bit pattern and triggers byte read.
  if (!currentLight && previousLight)  // while it was on and now it is off
  {
    // Read a full 8-bit byte from the light channel using timed sampling.
    char c = getLightByte();  // get byte for that character using binary code

    // If checksum mode is not active treat as normal char; otherwise the byte is a checksum.
    if (!checkSumLight) {
      appendItems(c, 1);  // mode 1 = light
    } else {
      // When checksum is active, compare computed checksum to received value.
      if (getCheckSum(1) != int(c)) {
        Serial.println(getCheckSum(1));
        Serial.println(int(c));
        Serial.println("Error in message by light!!!");
      }
      checkSumLight = false;  // reset checksum flag after processing
    }
    //Serial.print(c);
  }

  // Save currentLight to previousLight for next iteration edge detection.
  previousLight = currentLight;  // to reset the light state (for light reception)

  // -----------------------
  // SOUND MODE: continuous detection and decoding
  // -----------------------
  if (pickUpSound()) {  // if sound is detected
    // Turn on status LED to show reception of a sound burst.
    digitalWrite(ledRecPin, HIGH);

    // Record the start time of the detected event.
    unsigned long start = micros();  // start time
    numSoundCrossing = 1;            // we've already crossed the threshold once

    // Loop to count threshold crossings until we detect a stable silence lasting at least bitGap.
    while (true) {
      // Wait for silence (no threshold crossings). If silence begins, check stability for bitGap.
      if (!pickUpSound()) {
        unsigned long silenceStart = micros();
        bool stableSilence = true;
        numSoundCrossing++;
        while (micros() - silenceStart < bitGap) {
          // If sound is detected again too soon, switch stableSilence to false and continue.
          if (pickUpSound()) {
            stableSilence = false;
            numSoundCrossing++;
            break;
          }
        }
        if (stableSilence) break;  // if the gap detected is bitGap
      }
    }

    // Calculate noisy-sound duration measured (subtract the last bitGap accounted in loop).
    unsigned long duration = micros() - start - bitGap;  // how long the sound lasts
    //Serial.print("Dur: ");
    //Serial.println(duration);
    //Serial.print("NSC: ");
    //Serial.println(numSoundCrossing);

    // Convert the measured duration (and crossing count previously collected) into bits.
    translateIntoBinary(duration);

    // After decoding bits from the burst, wait for the larger silence that separates characters.
    unsigned long gapStart = micros();
    detectCharGap(gapStart);
  }

  // -----------------------
  // SERIAL COMMANDS: allow user to fetch and clear buffers via Serial input
  // -----------------------
  if (Serial.available()) {
    char input = Serial.read();
    if (input == 'l') {
      Serial.println("");
      Serial.println("Printing light");
      printOut(displayLightString);
    } else if (input == 's') {
      Serial.println("");
      Serial.println("Printing sound (one bit)...");
      printOut(displaySoundOneString);
    } else if (input == 'f') {
      Serial.println("");
      Serial.println("Printing sound (four bit)");
      printOut(displaySoundFourString);
    } else if (input == 'c') {
      // Dump all buffers and clear memory
      Serial.println("");
      Serial.println("Light: " + displayLightString);
      Serial.println("Light (all): " + realLightString);
      Serial.println("Light (trash): " + trashLightString);

      Serial.println("Sound (One Bit): " + displaySoundOneString);
      Serial.println("Sound (all) (One Bit): " + realSoundOneString);
      Serial.println("Sound (trash) (One Bit): " + trashSoundOneString);

      Serial.println("Sound (Four Bit): " + displaySoundFourString);
      Serial.println("Sound (all) (Four Bit): " + realSoundFourString);
      Serial.println("Sound (trash) (Four Bit): " + trashSoundFourString);

      Serial.println("Overall message: " + displayString);

      // Clear working buffers so a new message can start fresh.
      lightString = "";
      soundOneString = "";
      soundFourString = "";
      realLightString = "";
      realSoundOneString = "";
      realSoundFourString = "";
      trashLightString = "";
      trashSoundOneString = "";
      trashSoundFourString = "";
      displayString = "";
      clearMem();
      Serial.println("Memory cleared");
    }
  }
}

// =================================================================================
// LIGHT FUNCTIONS
// =================================================================================

// senseLight(): read the LDR and compare to threshold.
// Returns true when measured brightness exceeds calibrated lightThreshold.
bool senseLight() {
  int val = analogRead(ldrPin);  // reads the instantaneous light brightness in surroundings
  if (plotLight) {
    Serial.println(val);  // print value for troubleshooting/plotting
  }
  // Returning boolean indicates whether current measured light qualifies as "on".
  return val > lightThreshold;  // true if the brightness exceeds the threshold
}

// getLightByte(): sample 8 timed bits from the LDR and assemble them into a char.
// The protocol:
// - Caller detected falling edge; we wait a bit (period * 1.5) then sample 8 bits separated by 'period'.
// - count accumulates number of '1' bits for parity check.
// - If not in checksum mode, checkParity() is called and the MSB (bit 7) is cleared before return.
char getLightByte() {
  char c = 0;
  int count = 0;
  delay(period * 1.5);  // wait after start bit — aligns sampling to the first data bit
  for (int i = 7; i >= 0; i--) {
    bool bit = senseLight();  // read a new bit every loop (synchronized with transmitter timing)
    count += bit;
    c |= (bit << i);  // place the sampled bit into the (i)th position of the char
    delay(period);    // delay between each bit to line up with transmitter bit timing
  }
  if (!checkSumLight) {
    // Parity check ensures simple single-bit error detection. It also clears MSB before use.
    checkParity(c, count, 1);
    c &= ~(1 << 7);  // clears bit 7 (mask off parity/check bit)
  }
  return c;
}

// checkParity(): verify parity of the byte using 'n' (number of ones) and report if mismatch.
// mode indicates which channel triggered the parity message in Serial output (1=light,2=1-bit sound,3=4-bit sound).
void checkParity(char c, int n, int mode) {
  if (n % 2 == 0) {
    // even parity => correctByParity true
    correctByParity = true;
  } else {
    // odd parity => parity error
    correctByParity = false;
    Serial.println("");
    if (mode == 1) {
      Serial.print("Light");
    } else if (mode == 2) {
      Serial.print("Sound (One bit)");
    } else if (mode == 3) {
      Serial.print("Sound (Four bit)");
    }
    Serial.print(" incorrect by parity bit. Received: ");
    // Clear bit 7 before printing bits so output matches the displayed data bits only.
    c &= ~(1 << 7);  // clears bit 7
    for (int i = 0; i < 8; i++) {
      Serial.print(bitRead(c, 7 - i));  // prints each bit from MSB to LSB
    }
    Serial.println("");
  }
}

// getLightThreshold(): measure ambient light for durationToStart ms and return the mean reading (+1).
// This function uses millis() timing and analogRead to compute a baseline for dynamic thresholding.
int getLightThreshold() {
  double sum = 0;
  double numOfSample = 0;
  unsigned long startTime = millis();
  while (millis() - startTime < durationToStart) {
    double detected = analogRead(ldrPin);
    sum += detected;  // accumulate readings
    numOfSample += 1;
    delay(1);
  }
  return (int)(sum / numOfSample) + 1;  // return average +1 (small buffer)
}

// ==================================================================================
// SOUND FUNCTIONS
// ==================================================================================

// pickUpSound(): wrapper that samples sound peak and compares to soundThreshold.
// Returns true when the sampled peak amplitude exceeds the calibrated threshold.
bool pickUpSound() {
  int sound = sampleSound(durationMicros, sampleIntervalMicros);  // get the peak volume
  return sound > soundThreshold;                                  // true if the difference (between generated sound and ambient noise) is higher than the threshold
}

// sampleSound(): capture analog microphone samples for 'durationMicros' microseconds,
int sampleSound(int durationMicros, int sampleIntervalMicros) {
  int bias = 0;
  int peakAmplitude = 0;
  unsigned long startTime = micros();  // get the start time

  // For the duration of sampling, read analog mic and record the biggest deviation.
  while (micros() - startTime < durationMicros) {
    int sample = analogRead(micPin);     // instantaneous microphone reading
    int deviation = abs(sample - bias);  // difference between sample and bias
    if (deviation > peakAmplitude) {
      peakAmplitude = deviation;  // update peak amplitude
    }
    delayMicroseconds(sampleIntervalMicros);  // spacing between consecutive samples
  }
  if (plotSound) {
    Serial.println(peakAmplitude);  // print peak amplitude for troubleshooting/plotting
  }
  return peakAmplitude;
}

// decodeBinary(): take the integer arrays binaryOneBit or binaryFourBit and assemble a character.
// - mode 2: one-bit-per-sound decoding
// - mode 3: four-bit-per-sound decoding
// The function counts the number of '1' bits for parity, checks parity unless checksum mode set,
// clears MSB, appends to appropriate buffers, and finally clears temporary binary buffers.
void decodeBinary(int mode) {
  char c = 0;
  int count = 0;
  if (mode == 2) {
    for (int i = 0; i < 8; i++) {
      int n = binaryOneBit[i];  // gets the number from each bit slot
      if (n == 0) {
        c |= (0 << (7 - i));  // explicitly set 0 (no-op but clear for clarity)
        //Serial.print("0");
      } else {
        c |= (1 << (7 - i));  // set the corresponding bit in the output char
        count++;
        //Serial.print("1");
      }
    }
    if (!checkSumSoundOneBit) {
      // Check parity, then clear MSB (parity bit), then append to buffers.
      checkParity(c, count, 2);
      c &= ~(1 << 7);  // clears bit 7
      //Serial.println("");
      appendItems(c, 2);
      //Serial.print(" -----> ");
      //Serial.print(c);
    } else {
      // If checksum mode active, the received char should equal the computed checksum.
      if (getCheckSum(2) != int(c)) {
        Serial.println(getCheckSum(2));
        Serial.println(int(c));
        Serial.println("Error in message by sound (one bit)!!!");
      }
      checkSumSoundOneBit = false;
    }
  } else if (mode == 3) {
    for (int i = 0; i < 8; i++) {
      int n = binaryFourBit[i];  // gets the number from each bit slot
      if (n == 0) {
        c |= (0 << (7 - i));  // explicitly set 0 (keeps behavior explicit)
        //Serial.print("0");
      } else {
        c |= (1 << (7 - i));  // set the corresponding bit in output char
        count++;
        //Serial.print("1");
      }
    }
    if (!checkSumSoundFourBit) {
      checkParity(c, count, 3);
      c &= ~(1 << 7);  // clears bit 7
      //Serial.println("");
      appendItems(c, 3);
      //Serial.print(" -----> ");
      //Serial.print(c);
    } else {
      if (getCheckSum(3) != int(c)) {
        Serial.println(getCheckSum(3));
        Serial.println(int(c));
        Serial.println("Error in message by sound (four bit)!!!");
      }
      checkSumSoundFourBit = false;
    }
  }
  // Clear the temporary binary arrays to prepare for next byte.
  clearMem();
}

// printOut(): wrapper to print a full String and then clear binary buffers.
void printOut(String string) {
  Serial.println(string);
  clearMem();
}

// isDisplayable(): restrict characters to printable ASCII range (32..126).
// Used to avoid showing control characters or garbage.
bool isDisplayable(char c) {
  return (c >= 32 && c <= 126);
}

// clearMem(): reset the binary buffers used during decoding.
void clearMem() {
  memset(binaryOneBit, 0, sizeof(binaryOneBit));
  memset(binaryFourBit, 0, sizeof(binaryFourBit));
}

// translateIntoBinary(): converts a measured sound burst duration into bit(s) based on the encoding.
// The function:
// - computes a soundPeriod from duration and numSoundCrossing
// - estimates frequency from period and then maps that + duration into either a one-bit or four-bit pattern
// - writes the decoded bits into the appropriate binary array and advances indexOneBit/indexFourBit
// Important notes on variables:
// - numSoundCrossing: used here to compute the average sound period across crossings (set in main loop).
void translateIntoBinary(unsigned long duration) {
  int soundPeriod = 2 * (duration) / (numSoundCrossing);
  numSoundCrossing = 0;
  int freq = 1000000 / (soundPeriod);  // 1e6 / period gives an estimated frequency (Hz)
  //Serial.print("Freq: ");
  //Serial.println(freq);

  if (indexOneBit >= 8) {
    indexOneBit %= 8;
  }
  if (indexFourBit >= 8) {
    indexFourBit %= 8;
  }

  // Decision tree to classify the burst as:
  // - a short sound corresponding to a '0' in one-bit mode,
  // - a short sound corresponding to a '1' in one-bit mode,
  // - or a longer sound representing multiple bits in the four-bit encoding scheme.
  if (duration < (dotDuration + dotDuration / 2)) {
    // Very short => interpreted as a '0' in one-bit mode
    binaryOneBit[indexOneBit] = 0;
    indexOneBit++;
    ifSoundOneBit = true;
    ifSoundFourBit = false;
    //Serial.println("oneBitMode");

  } else if (duration < fourDotDuration * 2 / 3) {
    // Slightly longer but still under this threshold => interpreted as a '1' in one-bit mode
    binaryOneBit[indexOneBit] = 1;
    indexOneBit++;
    ifSoundOneBit = true;
    ifSoundFourBit = false;
    //Serial.println("oneBitMode");

  }
  // For durations that are mid-to-long range, interpret as four-bit groups:
  else if (duration < (fourDotDuration + fourDotDuration / 8)) {
    ifSoundOneBit = false;
    ifSoundFourBit = true;
    //Serial.println("fourBitMode");
    binaryFourBit[indexFourBit] = 0;
    if (freq < 1300) {
      // frequency bucket maps to 000
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 1500) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 1650) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 1900) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 2100) {
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 2300) {
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 2600) {
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 0;
    } else {
      // highest freq bucket -> 111
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 1;
    }
    indexFourBit += 4;  // advance by nibble size
  } else {
    // Very long duration: treat as four-bit group with MSB = 1 and additional bits from freq buckets.
    ifSoundOneBit = false;
    ifSoundFourBit = true;
    //Serial.println("fourBitMode");
    binaryFourBit[indexFourBit] = 1;  // long beep -> set MSB of nibble
    if (freq < 1300) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 1500) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 1650) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 1900) {
      binaryFourBit[indexFourBit + 1] = 0;
      binaryFourBit[indexFourBit + 2] = 1;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 2100) {
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 0;
    } else if (freq < 2300) {
      binaryFourBit[indexFourBit + 1] = 1;
      binaryFourBit[indexFourBit + 2] = 0;
      binaryFourBit[indexFourBit + 3] = 1;
    } else if (freq < 2600) {
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

// detectCharGap(): after a sound event, wait for silence long enough to mark character boundary.
// Depending on whether the last decoded bits were 1-bit or 4-bit, call decodeBinary() accordingly.
void detectCharGap(unsigned long gapStart) {
  if (ifSoundOneBit) {
    while (!pickUpSound()) {
      // detects for the absence of sound (gap)
      if (micros() - gapStart > charGap) {  // run this at the end of each byte to separate characters
        decodeBinary(2);                    // decodes the byte array for one-bit mode
        break;
      }
    }
  }
  if (ifSoundFourBit) {
    while (!pickUpSound()) {                // detects for the absence of sound (gap)
      if (micros() - gapStart > charGap) {  // run this at the end of each byte to separate characters
        decodeBinary(3);                    // decodes the byte array for four-bit mode
        break;
      }
    }
  }
}

// appendItems(): central place to handle a newly-decoded character 'c' for each mode.
// - If c == 0 it's treated as checksum indicator: the next received byte is interpreted as checksum.
// - Otherwise characters are appended into the appropriate buffers and displayed based on mode.
// - If parity showed an error, the char goes into trash*String and mistakeCounter increments.
// - After repeated mistakes, system switches to another mode as an adaptive recovery strategy.
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
      if (lightMode) {
        displayString += c;
      }
      realLightString += c;
      if (correctByParity && isDisplayable(c)) {
        lightString += c;
      } else {
        trashLightString += c;
        mistakeCounter++;
        if (mistakeCounter >= 2) {
          // on repeated mistakes switch to sound one-bit mode to try recovery
          lightMode = false;
          soundOneBitMode = true;
          mistakeCounter = 0;
        }
      }
    }
  } else if (mode == 2) {
    if (!checkSumSoundOneBit) {
      if (soundOneBitMode) {
        displayString += c;
      }
      realSoundOneString += c;
      if (correctByParity && isDisplayable(c)) {
        soundOneString += c;
      } else {
        trashSoundOneString += c;
        mistakeCounter++;
        if (mistakeCounter >= 2) {
          // switch to four-bit sound mode on repeated errors
          soundOneBitMode = false;
          soundFourBitMode = true;
          mistakeCounter = 0;
        }
      }
    }
  } else if (mode == 3) {
    if (!checkSumSoundFourBit) {
      if (soundFourBitMode) {
        displayString += c;
      }
      realSoundFourString += c;
      if (correctByParity && isDisplayable(c)) {
        soundFourString += c;
      } else {
        trashSoundFourString += c;
        mistakeCounter++;
        if (mistakeCounter >= 2) {
          // return to light mode after errors in four-bit mode
          soundFourBitMode = false;
          lightMode = true;
          mistakeCounter = 0;
        }
      }
    }
  }
}

// getCheckSum(): compute a checksum value based on accumulated valid characters for each mode.
// The algorithm:
// - counts the number of '1' bits at two specific bit positions across the accumulated string
// - reduces each count modulo 12, multiplies them and returns that product
// - it also appends the block to display string and clears the per-mode accumulation buffer.
//
// Notes:
// - indexOne and indexTwo are positions within each char used for checksum; currently 1 and 5.
// - The function returns an int that the transmitter should match when sending checksum byte.
int getCheckSum(int mode) {
  if (mode == 1) {
    int countOne = 0;
    int countTwo = 0;
    int indexOne = 1;
    int indexTwo = 5;
    for (int i = 0; i < lightString.length(); i++) {
      if (bitRead(lightString.charAt(i), 7 - indexOne) == 1) {
        countOne++;
      }
      if (bitRead(lightString.charAt(i), 7 - indexTwo) == 1) {
        countTwo++;
      }
    }
    countOne %= 12;
    countTwo %= 12;
    int countMult = int(countOne) * int(countTwo);
    //Serial.print("Light ");
    //Serial.println(countMult);
    displayLightString += lightString;
    lightString = "";
    return countMult;
  } else if (mode == 2) {
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
    //Serial.print("Sound1 ");
    //Serial.println(countMult);
    displaySoundOneString += soundOneString;
    soundOneString = "";
    return countMult;
  } else if (mode == 3) {
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
    //Serial.print("Sound4 ");
    //Serial.println(countMult);
    displaySoundFourString += soundFourString;
    soundFourString = "";
    return countMult;
  }
}