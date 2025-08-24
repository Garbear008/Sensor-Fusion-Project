const int ledPin = 7;
const int buzzerPin = 4;
//microseconds
const int dotDuration = 2000;
const int dashDuration = dotDuration * 3;
const int symbolSpace = dotDuration * 2;
const int letterSpace = dotDuration * 4;
const int wordSpace = dotDuration * 6;

String alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
String morseCodes[] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
  "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
  "..-", "...-", ".--", "-..-", "-.--", "--..",
  "-----", ".----", "..---", "...--", "....-", ".....",
  "-....", "--...", "---..", "----."
};


String inputMessage = "w";

void setup() {
  pinMode(ledPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(ledPin, LOW);
  digitalWrite(buzzerPin, LOW);

  Serial.begin(115200);
  Serial.println("Type a message and press ENTER:");
}

void loop() {
  if (Serial.available()) {
    inputMessage = Serial.readStringUntil('\n');
    inputMessage.trim(); // remove trailing newline/space
    inputMessage.toUpperCase();

    Serial.print("Sending: ");
    Serial.println(inputMessage);

    Serial.println("Sending...");


    sendMorse(inputMessage);

    Serial.println("Done.");
  }
}

void sendMorse(String msg) {
  for (int i = 0; i < msg.length(); i++) {
    char c = msg[i];

    if (c == ' ') {
      delayMicroseconds(wordSpace);
      continue;
    }

    String morse = getMorse(c);
    if (morse != "") {
      Serial.print(String(c));
      Serial.print(" -> ");
      blinkAndBuzz(morse);
      Serial.println("");
      delayMicroseconds(letterSpace);
    }
  }
}

void blinkAndBuzz(String code) {
  for (int i = 0; i < code.length(); i++) {
    if (code[i] == '.') {
      signal(dotDuration);
      Serial.print(".");
    } else if (code[i] == '-') {
      signal(dashDuration);
      Serial.print("-");
    }
    delayMicroseconds(symbolSpace);
  }
}

void signal(int duration) {
  digitalWrite(ledPin, HIGH);
  tone(buzzerPin, 900); 
  delayMicroseconds(duration);
  digitalWrite(ledPin, LOW);
  noTone(buzzerPin);
}


String getMorse(char c) {
  for (int i = 0; i < alphabet.length(); i++) {
    if (alphabet[i] == c) {
      return morseCodes[i];
    }
  }
  return "";
}

