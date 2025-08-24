// BOARD
const int ledTransPin = 3; // pin designation (D3) for LED
const int buzzerPin = 4; // pin designation (D4) for buzzer
const int buttonPin = 2; // pin designation (D2) for button

// for sending light
int lightPeriod = 3; // delay between each bit for light transmission
int brightness = 255; // to tune how bright the light shines (0–255 for light brightness)

// for sending sound
const int dotDuration = 10; // time duration for each short beep (dot or "0" in binary)
const int dashDuration = dotDuration * 3; // time duration for each long beep (dash or "1" in binary)
const int bitGap = dotDuration; // time gap between each bit
const int charGap = dotDuration * 8; // time gap between each character
const int frequency = 900; // frequency of the sound played by the buzzer

// for messaging
String inputMessage = ""; // initialize the string
const int iterations = 1; // number of loops the message is sent

// SETUP
void setup() {
  pinMode(ledTransPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW); // make sure the buzzer is turned off
  pinMode(buttonPin, INPUT_PULLUP); // to switch between transmitting light and sound
  Serial.begin(115200);
  Serial.println("Wait...");
  delay(5000);
  //digitalWrite(ledTransPin, HIGH); // turns on the LED, uncomment if light mode is HIGH
  Serial.println("Type a message and press ENTER:"); // customize the message to whatever seems fit
}

// LOOP
void loop() {
  //runs the following if there is a message typed in the serial input
  if (Serial.available())
  {
    inputMessage = Serial.readStringUntil('\n'); // gets the message
    inputMessage.trim(); // trims the message 
    Serial.print("Sending: ");
    Serial.println(inputMessage);

    if (digitalRead(buttonPin)==LOW) // LOW = button is pressed; HIGH = button is not pressed
    {
      // light mode
      for (int j =0; j<iterations; j++) // sends the message for the number of times stored in iterations
      {
        for (int i = 0; i < inputMessage.length(); i++) // sends the message once
        {
          sendChar(inputMessage.charAt(i)); // sends each character of the message
        }
        delay(1000); // Pause before repeating
      }     
    }
    else
    {
      // sound mode
      for (int j =0; j<iterations; j++) // sends the message an iteration amount of times
      {
        messageToTone(inputMessage); // sends the full message via beeps
      }
    }
    Serial.println("Done."); // signals the end of transmission
  }
}

// =========================================================================================
// LIGHT FUNCTIONS
// function to send one single character for light transmission 
void sendChar(char c) {
  analogWrite(ledTransPin, 0); // start bit
  delay(lightPeriod);
  for (int i = 0; i < 8; i++) 
  {
    bool bit = bitRead(c, 7 - i); // gets the bit from the character
    analogWrite(ledTransPin, bit ? brightness : 0); // if bit is 1, then the light is on; if bit is 0, then light is off
    delay(lightPeriod); // time between each bit
  }
  analogWrite(ledTransPin, brightness); // stop bit
  delay(lightPeriod); // time between each byte
}

// ======================================================================================
// SOUND FUNCTIONS
// overall function to transmit message in sound
void messageToTone(String msg) {
  // translates the entire message 
  for (int i = 0; i < msg.length(); i++) {
    char c = msg[i]; // stores the ith char in the string
    zeroAndOne(c); // turns the character into binary code
    delay(charGap); // time between each character
  }
}

// function to convert each character to binary code
void zeroAndOne(char c) {
  // translates a single character
  for (int i = 0; i < 8; i++) {
    if (bitRead(c, 7 - i)== 0) {
      signal(dotDuration); // calls to play a short beep if "0"
      //Serial.print(0); // for troubleshooting
    } else {
      signal(dashDuration); // calls to play a long beep if "1"
      //Serial.print(1); // for troubleshooting
    }
    delay(bitGap); // time between each bit
  }
  //Serial.print (" ---> "); // for troubleshooting
  //Serial.println(c); // prints out the character for troubleshooting
}

// function to play the beeps
void signal(int duration) {
  tone(buzzerPin, frequency); // plays a tone at the frequency stored
  delay(duration); // play the tone for a amount of time (via delay)
  noTone(buzzerPin); // stops the tone
}