// === Pin & Threshold Configuration ===
const int micPin = A1;
const int soundThreshold = 52;  // Adjust based on your mic's response

// === Morse Timing (in microseconds) ===
const int dotDuration = 2000;
const int dashDuration = dotDuration * 2;
const int symbolGap = dotDuration * 1.3;
const int letterGap = dotDuration * 2;
const int wordGap = dotDuration * 4;
const int durationMicros = 5;
const int sampleIntervalMicros = 0.5;

// === Morse Mapping ===
String alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
String morseCodes[] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
  "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
  "..-", "...-", ".--", "-..-", "-.--", "--..",
  "-----", ".----", "..---", "...--", "....-", ".....",
  "-....", "--...", "---..", "----."
};

String morseLetter = "";
String decodedMessage = "";

void setup() {
  Serial.begin(115200);
  pinMode(micPin, INPUT);
  Serial.println("Listening for Morse Code...");
}

void loop() {
  // Check for serial input (type c to clear decoded message, p to print decoded message)
  checkSerial();

  if (pickUpSound()) {
    // Start of tone
    unsigned long start = micros();

    // Continue until stable silence
    while (true) {
      if (!pickUpSound()) {
        unsigned long silenceStart = micros();
        bool stableSilence = true;

        while (micros() - silenceStart < symbolGap) {  // Confirm symbolGap of silence
          if (pickUpSound()) {
            stableSilence = false;
            break;
          }
        }

        if (stableSilence) break;
      }
    }

    unsigned long duration = micros() - start - symbolGap;
    Serial.print("Duration: ");
    Serial.println(duration);

    // Classify tone
    if (duration < dashDuration) {
      morseLetter += ".";
      Serial.print(".");
    } else {
      morseLetter += "-";
      Serial.print("-");
    }

    // Now check silence duration after tone to decide on letter/word gap
    unsigned long gapStart = micros();
    while (!pickUpSound()) {
      unsigned long silence = micros() - gapStart;
      if (silence >= wordGap) {
        decodeLetter();
        decodedMessage += " ";
        Serial.print(" ");
        Serial.print("Decoded: ");
        Serial.println(decodedMessage);
        break;
      } else if (silence >= letterGap) {
        decodeLetter();
        break;
      }
    }
  }
}

// === Sound Detection Function ===
bool pickUpSound() {
  int peak = 0;
  int bias = 0;
  unsigned long start = micros();

  while (micros() - start < durationMicros) {
    int reading = analogRead(micPin);
    int deviation = abs(reading - bias);  // Assume mid-bias of 0
    if (deviation > peak) peak = deviation;
    delayMicroseconds(sampleIntervalMicros);
  }
  //Serial.println(peak);

  return peak > soundThreshold;
}

// === Decode Morse Sequence to Letter ===
void decodeLetter() {
  if (morseLetter.length() == 0) return;

  for (int i = 0; i < alphabet.length(); i++) {
    if (morseCodes[i] == morseLetter) {
      decodedMessage += alphabet[i];
      Serial.print(" -> ");
      Serial.println(alphabet[i]);
      break;
    }
  }
  morseLetter = "";
}

void checkSerial(){
    // === Check for serial input ===
  if (Serial.available()) {
    char input = Serial.read();

    if (input == 'p') {
      Serial.println();
      Serial.print("Current Decoded Message: ");
      Serial.println(decodedMessage);
    }

    if (input == 'c') {
      decodedMessage = "";
      Serial.println("Decoded message cleared.");
    }
  }
}
