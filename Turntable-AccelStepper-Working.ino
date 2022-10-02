/*
Turntable Sketch v1-10-2022
By Anthony Kochevar
Sketch can be modified and distributed freely as long as it is done free of charge.
Please read all comments below and make changes as needed.

You will also need the following libraries installed in that Arduino IDE:
https://github.com/madleech/Auto485
https://www.arduino.cc/reference/en/libraries/accelstepper/
https://github.com/madleech/ArduinoCMRI
https://www.arduino.cc/reference/en/libraries/elapsedmillis/
https://docs.arduino.cc/learn/built-in-libraries/eeprom  (might come installed by default)
*/


#include <Auto485.h>
#include <AccelStepper.h>
#include <CMRI.h>
#include <elapsedMillis.h>
#include <EEPROM.h>

// Below sets up CMRI connection
#define CMRI_ADDR 3  // Address of CMRI node in JMRI.  If set to 3 first address in JMRI will be 3001.
#define DE_PIN 2 
Auto485 bus(DE_PIN);
CMRI cmri(CMRI_ADDR, 64, 64, bus);  // Setup CMRI connection with 64 inputs and outputs.  Up to 64 indexed turntable positions possible.

/* Below sets variables for the stepper motor object using Accelstepper library.
 * If you are not using a unipolar 28BYJ-48 stepper and ULN2003 Driver Board
 * you will need to create your own motor object and configure it to operate the correct direction
 * of clockWise and counterWise.  See AccelStepper library documentation.
 */
 //------------------------------
const int ttPin1 = 4;  // The four pins used on the Arduino to connect to the stepper board.
const int ttPin2 = 5;
const int ttPin3 = 6;
const int ttPin4 = 7;
const int stepsPerRevolution = 2048;  // Set to your number of steps per revolution per stepper/board specs. Change as needed.
const int maxSpeedLimit = 100;  // ---DO NOT CHANGE FROM 100--- or steps could be missed.
const int ttAccel = 50;  // Acceleration speed.  Change if wanted.  Higher is faster.

// Below creates a turntable stepper motor object for unipolar 28BYJ-48 stepper and ULN2003 Driver Board.
AccelStepper ttStepper(AccelStepper::FULL4WIRE, ttPin4, ttPin2, ttPin3, ttPin1);
//Comment out line above and uncomment line below if turntable is not moving clockwise and counterwise correctly.
//AccelStepper ttStepper(AccelStepper::FULL4WIRE, ttPin1, ttPin3, ttPin2, ttPin4);
//-------------------------------

//  Below defines Potentiometer and momentary switch pins and other variables.
//-------------------------------
char potsPin[] = "A0";  // Pin used by potentiometer signal on Arduino. Change if needed.
const int clockPin = 10;  // Pin used for clockWise switch calls.  Change as needed.
const int counterPin = 11;  // Pin used for counterclockwise(counterWise) switch calls. Change as needed.
const int eepromAdd = 0;  // EEPROM address to use throughout the sketch and the one after. Change as needed.
const int manMove = 1500; // Sets time to wait for manual turntable movement.  1500 = 1.5 seconds.  Change if wanted.
int clockWise;
int counterWise;
int potsReading;
int motorSpeed;
int currentLocation;
bool clockOn;
bool counterOn;

/*
 * Below sets the number of indexed positions the turntable will have.  Must have at least two.
 * indexPos indexed position values must go from smallest to greatest moving in clockwise direction.
 * An indexed position -CAN NOT- be home(0) or stepsPerRevolution above.
*/
const int numPos = 4;  // Total number of indexed positions there will be for this turntable.  Change if needed.
int statusCMRI[numPos];  // Array to define CMRI address status used to number of indexed positions
// Below array defines set indexed locations for turntable based on numPos.  Add/Remove/Change as needed.
int indexPos[numPos] = {  500,  1000,  1500,  2000};  


elapsedMillis switchTimer;  //  Initalize millis timer.

void setup() {

  //Serial.begin(9600, SERIAL_8N2); // SERIAL_8N2 to match what JMRI expects CMRI hardware to use
  Serial.begin(19200);

  pinMode(clockPin, INPUT_PULLUP); //  Pin setup for side of switch for clockwise motion
  pinMode(counterPin, INPUT_PULLUP);  //  Pin setup for side of switch for counter-clockwise motion

  for (int x = 0; x < numPos; x++) {  // Initallize CMRI arrays
    statusCMRI[x] = 0;
    cmri.set_bit(x, false);
  }
  Serial.println();
  Serial.println(F("Turntable Movement Sketch v1.0-9-2022"));
  Serial.println(F("By Anthony Kochevar")); delay(3000); Serial.println();
  Serial.println(F("Arduino booted,  checking if homing to 0 is requested..."));
  Serial.println(F("Set Speed to low and hold Clockwise switch on..."));
  Serial.print(F("5... ")); delay(1000);
  Serial.print(F("4... ")); delay(1000);
  Serial.print(F("3... ")); delay(1000);
  Serial.print(F("2... ")); delay(1000);
  Serial.print(F("1... ")); delay(1000); Serial.println();
  potsReading = analogRead(potsPin);  // Check Pots value
  clockWise = digitalRead(clockPin);  // Check if clockwise switch is on
  if (potsReading <= 5) potsReading = 0; // Removes false low values when pots dial is turned all the way down.
  if ((clockWise == LOW)  && (potsReading == 0))  {
    Serial.println(F("Homing has been requested... Setting current turntable position to 0..."));
    currentLocation = 0;
    ttStepper.setCurrentPosition(0);
    Serial.println(F("Turntable set to 0... Saving to EEPROM..."));
    Serial.println(F("Release switch..."));
    EEPROM.update(eepromAdd, currentLocation >> 8);  // Save an Int to EEPROM.  Uses two addresses.
    EEPROM.update(eepromAdd + 1, currentLocation & 0xFF);
    delay(3000);

  }
  else {
    Serial.println(F("Skipping homing...  Getting last position from EEPROM..."));
    currentLocation = (EEPROM.read(eepromAdd) << 8) + EEPROM.read(eepromAdd + 1);  // Read a Int from EEPROM.  Uses two Addresses.
    ttStepper.setCurrentPosition(currentLocation);
  }
  potsReading = analogRead(potsPin); //  Get reading from 10K Potentiometer.
  motorSpeed = map(potsReading, 0, 1023, 5, maxSpeedLimit);  // Map speed to a range from 5 to maxSpeedLimit variable.
  ttStepper.setMaxSpeed(maxSpeedLimit);
  ttStepper.setSpeed(motorSpeed);    // Initalize speed.
  ttStepper.setAcceleration(ttAccel);
  switchTimer = 0;
  Serial.println(F("Turntable ready."));
  Serial.println(F("Current position:"));
  Serial.println(currentLocation);
  Serial.println();
}

void loop(){

//  ====Step 1 - Always check if the switch is thrown====
  clockWise = digitalRead(clockPin);
  counterWise = digitalRead(counterPin);
//  =====================================================

//  ==== Step 2 - If switch is thrown do the following or skip to next step====
  if ((clockWise == LOW) || (counterWise == LOW)) {
    if (clockWise == LOW) clockOn = true;
    if (counterWise == LOW) counterOn = true;
    switchTimer = 0;
    delay(200);  // Prevents intermittent switch connection issues that causes skipping parts of this step.
    while ((switchTimer <= manMove) && ((clockWise == LOW) || (counterWise == LOW))) {
      clockWise = digitalRead(clockPin);
      counterWise = digitalRead(counterPin);
    }
    if ((clockOn == true) && (switchTimer < manMove))  {  
      //  If switch was thrown and released in less than manMove, move clockwise to next indexed position
      ClockWiseIndexMove();
    }
    if (clockWise == LOW) {
      clockWise = digitalRead(clockPin);
      while ((clockWise == LOW) && (switchTimer >= manMove)) {  
        //  If switch is held for more than manMove, manually move the turntable at the speed of the pots until switch is released.
        if (clockOn == true) Serial.println(F("ClockWise Manual Move..."));
        clockOn = false;
        ttStepper.setAcceleration(0);  // Disable Acceleration
        potsReading = analogRead(potsPin);  // Check Pots value
        motorSpeed = map(potsReading, 0, 1023, 5, maxSpeedLimit);  // Map speed to a range from 5 to maxSpeedLimit variable.
        ttStepper.setSpeed((float)motorSpeed);
        ttStepper.runSpeed();
        clockWise = digitalRead(clockPin);
        currentLocation = ttStepper.currentPosition();
        if (currentLocation == stepsPerRevolution + 1) currentLocation = 0;  //Reset to 0 position if goes over stepsPerRevolution
        ttStepper.setCurrentPosition(currentLocation); 
      }  // End while ClockWise == LOW 
    }  // End if clockWise == LOW
	
    if ((counterOn == true) && (switchTimer < manMove))  {  
      //  If switch was thrown and released in less than manMove move counterwise to next indexed position
      CounterWiseIndexMove();
    }
    if (counterWise == LOW) {
      counterWise = digitalRead(counterPin);
      while ((counterWise == LOW) && (switchTimer >= manMove)) {  
        //  If switch is held for more than manMove, in seconds manually move the turntable until switch is released.
        if (counterOn == true) Serial.println(F("CounterWise Manual Move..."));
        counterOn = false;
        ttStepper.setAcceleration(0);  // Disable Acceleration
        potsReading = analogRead(potsPin);  // Read the Pots value.
        motorSpeed = map(potsReading, 0, 1023, 5, maxSpeedLimit);  // Map to maximum speed range.
        ttStepper.setSpeed((float)-motorSpeed);
        ttStepper.runSpeed();
        counterWise = digitalRead(counterPin);
        currentLocation = ttStepper.currentPosition();
		    if (currentLocation == -1) currentLocation = stepsPerRevolution;  //Reset to stepsPerRevolution if position passes less than 0.
        ttStepper.setCurrentPosition(currentLocation);

      }  // End while counterWise == LOW
    }  // End if counterWise == LOW

    Serial.println(F("Move finished, new position:"));
    currentLocation = ttStepper.currentPosition();
    Serial.println(currentLocation);
    Serial.println();
    EEPROM.update(eepromAdd, currentLocation >> 8);  // Save an Int to EEPROM.  Uses two addresses.
    EEPROM.update(eepromAdd + 1, currentLocation & 0xFF);
  }   // End if ((clockWise == LOW) || (counterWise == LOW))
  //  ====End Step 2===============================================================

  //  ====Step 3 - Check CMRI data and make changes if change is requested====
  
  for (int x = 0; x < numPos; x++) {
    cmri.process();
    statusCMRI[x] = (cmri.get_bit(x));
    if (statusCMRI[x] == 1) {
      for (int i = 0; i < numPos; i++) {
        statusCMRI[i] = 0;
        cmri.set_bit(i, false);
      }
      cmri.transmit();
      delay(500);  //  Allows JMRI to catch up.
      ttStepper.setAcceleration(ttAccel);
      ttStepper.runToNewPosition(indexPos[x]);
      currentLocation = indexPos[x];
      ttStepper.setCurrentPosition(currentLocation);
      EEPROM.update(eepromAdd, currentLocation >> 8);  // Save an Int to EEPROM.  Uses two addresses.
      EEPROM.update(eepromAdd + 1, currentLocation & 0xFF);
    } // End if ((statusCMRI[x] == 1) && (oldCMRI[x] == 0))
    else {
        statusCMRI[x] = 0;
        cmri.set_bit(x, false);
    }
  } // End for x
  //  ====End Step 3======================================================================

}  // End void loop


void ClockWiseIndexMove() {
  Serial.println(F("ClockWise Index Move Running..."));
  clockOn = false;
  int nextPos;
  int tmpPos = ttStepper.currentPosition();
  bool posSet = false;
  for (int x = 0; x < numPos; x++) {   
    // This for loop cycles through all index positions starting from first to last.
    if (tmpPos == indexPos[x])  {  //  This if checkes if already at a indexed position and advances to the next one.
      nextPos = x + 1;
      if (nextPos > (numPos - 1)) nextPos = 0;  // Start over if next position is greater than last position in the array.
      break;
    }
    if (tmpPos < indexPos[x]) {
      //  This if check for next closest index position and sets it.
      nextPos = x;
      break;
    }
    if (tmpPos > indexPos[numPos - 1])  {
      //  This if checks for any position after last index position and sets it back to the first index.
      nextPos = 0;
      break;
    }
  }
  ttStepper.setAcceleration(ttAccel);
  ttStepper.runToNewPosition(indexPos[nextPos]);
  currentLocation = indexPos[nextPos];
  ttStepper.setCurrentPosition(currentLocation);
  return;
}  // End ClockWiseIndexMove

void CounterWiseIndexMove() {
  Serial.println(F("CounterWise Index Move Running..."));
  counterOn = false;
  int nextPos;
  int tmpPos = ttStepper.currentPosition();
  for (int x = numPos - 1; x >= 0; x--) {   
    // This for loop cycles through all the index positions from the last to first.
    if (tmpPos == indexPos[x]) {  //  This if checkes if already at a indexed position and goe back to the one before.
      nextPos = x - 1;
      if (nextPos < 0) nextPos = numPos - 1;  // If on the first array position(0) set to the last array position.
      break;
    }
    if (tmpPos > indexPos[x]) {
      // This if checks for the next index position behind the current position and sets it.
      nextPos = x;
      break;
    }
    if (tmpPos < indexPos[0])  {
      // This if checks if lower than first index position and sets the last array position.
      nextPos = numPos - 1;
      break;
    }
  }  // End For x loop
 
  ttStepper.setAcceleration(ttAccel);
  ttStepper.runToNewPosition(indexPos[nextPos]);
  currentLocation = indexPos[nextPos];
  ttStepper.setCurrentPosition(currentLocation);
  return;
}  // End CounterWiseIndexMove
