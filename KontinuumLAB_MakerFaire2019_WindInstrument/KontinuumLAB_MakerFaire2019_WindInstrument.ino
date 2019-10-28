// Pin for the analog breath sensor:
int breathPin = 14;

// Pins for multiplexer control:
int muxPin1 = 8;
int muxPin2 = 7;
int muxPin3 = 6;
int muxPin4 = 5;
int touchPin = 0;

// Button pins:
int calibrateBtn = 2;
int feedbackBtn = 3;
int calibrating = 0;

// Variables for the analog breath sensor Initial minumum and maximum,
// plus raw values, mapped output values and previous values:
int breathMin = 250;
int breathMax = 40;
int breathRaw;
int lastBreathRaw;
int breathOut;
int lastBreathOut;

// Variables for the key readings. This includes pitch bend and octave key:
int keyReadings[16];
int lastKeyReadings[16];
// Binary on / off value for the individual keys:
int activeKeys[16];

// current and previous MIDI note number:
int currentNote;
int lastNote;
// Which MIDI channel to send on:
int MIDIchannel = 2;

// Initial minimum and maximum values for key sensors:
int keyMin[16] = {370, 370, 370, 370, 370, 370, 370, 370, 370, 370, 370, 370, 370, 370, 370, 370};
int keyMax[16] = {480, 480, 480, 480, 480, 480, 480, 480, 480, 480, 480, 480, 480, 480, 480, 480};
// Threshold for activating each sensor, in the context of a reading mapped to MIDI range:
int keyThreshold[16] = {40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40, 40};

// Specific "non key" sensor positions in the multiplexer/array:
int octaveSensor = 13;
int bendDnSensor = 14;
int bendUpSensor = 15;

// Intermediate and final output values for pitch bend:
int bendUp;
int bendDn;
int pitchBend;

// Octave value:
int octave = 0;

// Fingering arrays, used to determine which note is currently being played:
// Each array represents a specific note, as stated at the last position of each array. 
// Numbers 0 - 12 represent a specific key, "100" means go to next stage.
// Far left values (until first "100") are keys that HAVE to be pressed to produce the note in question.
// Next series of keys (until next "100") are keys that must NOT be pressed for the note to be valid.
int fingerings[20][14] = {
  {0, 7, 100, 100, 0, 0, 0, 0, 0, 0, 0, 0, 63}, // Eb
  {0, 100, 1, 100, 0, 0, 0, 0, 0, 0, 0, 0, 62}, // D
  {100, 1, 3, 100, 0, 0, 0, 0, 0, 0, 0, 0, 61}, // C#
  {3, 100, 1, 100, 0, 0, 0, 0, 0, 0, 0, 0, 60}, // C
  {1, 100, 2, 3, 100, 0, 0, 0, 0, 0, 0, 0, 59}, // B
  {1, 2, 100, 3, 100, 0, 0, 0, 0, 0, 0, 0, 58}, // Bb
  {1, 3, 7, 100, 4, 100, 0, 0, 0, 0, 0, 0, 58}, // Bb
  {1, 3, 100, 4, 7, 100, 0, 0, 0, 0, 0, 0, 57}, // A
  {1, 3, 4, 5, 100, 8, 9, 100, 0, 0, 0, 0, 56}, // Ab
  {1, 3, 4, 100, 5, 8, 9, 100, 0, 0, 0, 0, 55}, // G
  {1, 3, 4, 9, 100, 8, 100, 0, 0, 0, 0, 0, 54}, // F#
  {1, 3, 4, 8, 100, 9, 100, 0, 0, 0, 0, 0, 53}, // F
  {1, 3, 4, 8, 9, 100, 10, 100, 0, 0, 0, 0, 52}, // E
  {1, 3, 4, 8, 9, 10, 11, 100, 12, 100, 0, 0, 51}, // Eb
  {1, 3, 4, 8, 9, 10, 100, 11, 12, 100, 0, 0, 50}, // D
  {1, 3, 4, 5, 8, 9, 10, 12, 100, 6, 100, 0, 49}, // C#
  {1, 3, 4, 8, 9, 10, 12, 100, 5, 6, 100, 0, 48}, // C
  {1, 3, 4, 6, 8, 9, 10, 100, 12, 100, 0, 0, 47}, // B (doesn't work
  {1, 3, 4, 6, 8, 9, 10, 12, 100, 100, 0, 0, 46}, // Bb
};

// Error value for the exponential filter:
float error;



void setup() {
  // Start the serial connection: 
  Serial.begin(9600);

  // set multiplexer control pin pinModes:
  pinMode(muxPin1,OUTPUT);
  pinMode(muxPin2,OUTPUT);
  pinMode(muxPin3,OUTPUT);
  pinMode(muxPin4,OUTPUT);

  // Set the button pin pinMode:
  pinMode(calibrateBtn, INPUT_PULLUP);
//  digitalWrite(12, LOW);
//  digitalWrite(13, LOW);
}

// Helper function for the exponential filter function:
float snapCurve(float x){
  float y = 1.0 / (x + 1.0);
  y = (1.0 - y) * 2.0;
  if(y > 1.0) {
    return 1.0;
  }
  return y;
}

// Main exponential filter function. Input "snapMult" = speed setting. 0.001 = slow / 0.1 = fast:
int expFilter(int newValue, int lastValue, int resolution, float snapMult){
  unsigned int diff = abs(newValue - lastValue);
  error += ((newValue - lastValue) - error) * 0.4;
  float snap = snapCurve(diff * snapMult);
  float outputValue = lastValue;
  outputValue  += (newValue - lastValue) * snap;
  if(outputValue < 0.0){
    outputValue = 0.0;
  }
  else if(outputValue > resolution - 1){
    outputValue = resolution - 1;
  }
  return (int)outputValue;
}

// Function to access and read a single position on the multiplexer:
int readSingleCap(int number){
  digitalWrite(muxPin1, bitRead(number, 0)); 
  digitalWrite(muxPin2, bitRead(number, 1));
  digitalWrite(muxPin3, bitRead(number, 2));
  digitalWrite(muxPin4, bitRead(number, 3));
  int value = touchRead(touchPin);
  return value;
}

void loop() {
  // Initiate position variable for for loops:
  int i;
  
  // Read calibrate button. Inverse result for "1" to be "yes":
  calibrating = !digitalRead(calibrateBtn);
  Serial.println(calibrating);
  // Calibration function:
  if(calibrating == 1){
    // Set sensor minimum really high, and sensor maximum really low:
    for(i = 0; i < 16; i++){
      keyMax[i] = 0;
      keyMin[i] = 2000;
      breathMin = 0;
      breathMax = 1023;
    }
    // Keep calibrating while button is pressed:
    while(calibrating == 1){
      // Read all sensors. If lower than "min" set as new min. If higher than "max" set as new max
      for(i = 0; i < 16; i++){
        keyReadings[i] = readSingleCap(i);
        if(keyReadings[i] < keyMin[i]){
          keyMin[i] = keyReadings[i];
        }
        else if(keyReadings[i] > keyMax[i]){
          keyMax[i] = keyReadings[i];
        }
      }
      breathRaw = analogRead(breathPin);
      if(breathRaw > breathMin){
        breathMin = breathRaw;
      }
      else if(breathRaw < breathMax){
        breathMax = breathRaw;
      }
     // Read button every loop:
      calibrating = !digitalRead(calibrateBtn);
    }

    // Adjust minimum and maximum values with a buffer value:
    for(i = 0; i < 16; i++){
      keyMax[i] = keyMax[i] - 10;
      keyMin[i] = keyMin[i] + 10;
    }
    breathMax = breathMax + 20;
    breathMin = breathMin - 30;
  }
  // Finish calibration//

  // Save previous breath sensor raw value, then read and filter new value:
  lastBreathRaw = breathRaw;
  breathRaw = analogRead(breathPin);
  breathRaw = expFilter(breathRaw, lastBreathRaw, 1024, 0.01);
  // Save previous breath sensor output value, then map new value from raw reading:
  lastBreathOut = breathOut;
  breathOut = map(breathRaw, breathMin, breathMax, 0, 127);
//  Serial.println(analogRead(breathPin));

  // Limit output to MIDI range:
  if(breathOut < 0){
    breathOut = 0;
  }
  else if(breathOut > 127){
    breathOut = 127;
  }
  // If breath sensor output value has changed: 
  if(breathOut != lastBreathOut){
//    Serial.println(breathOut);
    // Send CC2 volume control:
    usbMIDI.sendControlChange(2, breathOut, MIDIchannel);
    // If breath sensor recently DEactivated, send note off message:
    if(breathOut == 0){
      usbMIDI.sendControlChange(123, 0, MIDIchannel);
    }
    // Else if breath sensor recently activated, send note on message:
    else if(breathOut != 0 && lastBreathOut == 0){
      usbMIDI.sendNoteOn(currentNote, 127, MIDIchannel);
    }
  }
  // Key reading for loop is only done if breath sensor output is NOT equal to zero:
  if(breathOut != 0){ 
    // Main key reading for loop. "i" is sensor number:
    for(i = 0; i < 16; i++){
      // Read sensor at current position:
      keyReadings[i] = readSingleCap(i);
      // Map reading to MIDI range:
      keyReadings[i] = map(keyReadings[i], keyMin[i], keyMax[i], 0, 127);
      // Limit reading to MIDI range:
      if(keyReadings[i] > 127){
        keyReadings[i] = 127;
      }
      if(keyReadings[i] < 0){
        keyReadings[i] = 0;
      }
      // Filter reading for smoothness:
      keyReadings[i] = expFilter(keyReadings[i], lastKeyReadings[i], 128, 0.01);
      // Activate "activeKeys" at current position, if reading is higher than threshold:
      if(keyReadings[i] > keyThreshold[i]){
        activeKeys[i] = 1;
      }
      // Else, DEactivate "activeKeys" at current position:
      else{
        activeKeys[i] = 0;
      }
      Serial.print(activeKeys[i]);
      Serial.print(" - ");
    }
    Serial.println();

    // Variables for the fingering-analysis section:
    int keyPhase;
    int correct = 0;
    int newNote = 0;
    lastNote = currentNote;

    // If octave key is activated, add 12 semitones to final result, else add zero:
    if(activeKeys[octaveSensor] == 1){
      octave = 12;
    }
    else{
      octave = 0;
    }
    
    // Main fingering-analysis for-loop.
    // "n" = fingering array, "i" = position within the fingering loop:
    int n;
    for(n = 0; n < 20; n++){
      // During first keyPhase, check if the requested sensor is active. 
      keyPhase = 0;
      for(i = 0; i < 16; i++){
        if(fingerings[n][i] != 100){
          // If active sensors coincide with "fingerings" array requirements: "correct" = yes.
          // Else: "correct" = no and exit current "fingering" array.
          if(keyPhase == 0){  // Check "closed" keys
            if(activeKeys[fingerings[n][i]] == 1){
              correct = 1;
            }
            else{
              correct = 0;
              break;
            }
          }
          // During second keyPhase, check 
          else if(keyPhase == 1){ // Check "open" keys
            if(activeKeys[fingerings[n][i]] == 0){
              correct = 1;
            }
            else{
              correct = 0;
              break;
            }
          }       
        }
        // When a "100" is encountered, add 1 to keyPhase, except if keyPhase is 1, exit loop:
        else if(fingerings[n][i] == 100){
          if(keyPhase == 1){
            break;
          }
          else{
            keyPhase ++;
          }
        }
      }
      // if "correct" is still yes after all loops finish, then a "fingerings" array has perfectly coincided with
      // current situation. Set "currentNote to the note defined at the end of that array:
      if(correct == 1){
        currentNote = fingerings[n][12];
        break;
      }
    }
    // If correct is 1, then there is a new note to report, so set "newNote" to 1, and set "currentNote" to
    // currentNote plus octave: 
    if(correct == 1){
      currentNote = currentNote + octave;
      if(currentNote != lastNote){
        newNote = 1;
      }
    }
    // If newNote is a yes, then send MIDI messages. lastNote off and currentNote On
    if(newNote == 1){
      Serial.print("Note: ");
      Serial.println(currentNote);
      usbMIDI.sendNoteOn(lastNote, 0, MIDIchannel);
      usbMIDI.sendNoteOn(currentNote, 127, MIDIchannel);
    }
  }
  // map the bendValues from their respective "keyReading" array positions, to half a MIDI value each:
  bendDn = map(keyReadings[15], 0, 127, 0, 64);
  bendUp = map(keyReadings[14], 0, 127, 0, 63);
  // Calculate pitchBend from these two values. Then send MIDI pitchBend message:
  pitchBend = 0 - bendDn + bendUp;
  usbMIDI.sendPitchBend(pitchBend<<7, MIDIchannel);
//  delay(2);

}
