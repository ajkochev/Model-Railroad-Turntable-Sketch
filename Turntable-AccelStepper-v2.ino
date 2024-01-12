/*  
Turntable Sketch v2.1.2024
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

#include <AccelStepper.h>
#include <elapsedMillis.h>
#include <EEPROM.h>
#include <CMRI.h>
#include <Auto485.h>


// Below sets up CMRI connection
#define CMRI_ADDR 3  // Address of CMRI node in JMRI.  If set to 3 first address in JMRI will be 3001.
#define DE_PIN 2 
Auto485 bus(DE_PIN);
CMRI cmri(CMRI_ADDR, 0, 64, bus);  // Setup CMRI connection with 64 outputs.  Up to 64 indexed positions possible.

/* Below sets variables for the stepper motor object using Accelstepper library.
 * If you are not using a unipolar 28BYJ-48 stepper and ULN2003 Driver Board
 * you will need to create your own motor object and configure it to operate the correct direction
 * of clockWise and counterWise.  See AccelStepper library documentation.
 */
 //------------------------------
 
 // The four pins used on the Arduino to connect to the ULN2003 stepper board.
const int ttPin1 = 4;  // Arduino pin to connect to pin 1 on stepper board.
const int ttPin2 = 5;  // Arduino pin to connect to pin 2 on stepper board.
const int ttPin3 = 6;  // Arduino pin to connect to pin 3 on stepper board.
const int ttPin4 = 7;  // Arduino pin to connect to pin 4 on stepper board.
const int stepsPerRevolution = 2048;  // Set to your number of steps per revolution per stepper/board specs. Change as needed.
const int maxSpeedLimit = 100;  // ---DO NOT CHANGE FROM 100--- or steps could be missed.
const int startSpeed = 3; // Number counters must be at to start moving the turntable.
const int ttAccel = 50;  // Acceleration speed.  Change as wanted.  Higher is faster.
const int indexSpeed = 20;  //Speed of turntable when index moving.  Higher is faster.

// Below creates a turntable stepper motor object for unipolar 28BYJ-48 stepper and ULN2003 Driver Board.
//AccelStepper ttStepper(AccelStepper::FULL4WIRE, ttPin4, ttPin2, ttPin3, ttPin1);
// Comment out line above and uncomment line below if turntable is not moving clockwise and counterwise correctly.
AccelStepper ttStepper(AccelStepper::FULL4WIRE, ttPin1, ttPin3, ttPin2, ttPin4);
//-------------------------------

// Below sets variables for the rotary encoder pins the Arduino will use and states.
const int rotaryCLK = 8;  // Arduino pin for Rotary encoder CLK pin.
const int rotaryDT = 9;   // Rotary encoder DT pin.
const int rotarySW = 10;  // Rotary encoder SW or button pin.
const int pinLED = 11;    // Arduino pin to LED used to show turntable movement.
int counter = 0; 
int currentStateCLK;
int previousStateCLK;
bool currentStateSW;


//  Below defines other variables.
//-------------------------------
const int eepromAdd = 0;  // EEPROM address to use throughout the sketch and the one after. Change as needed.
const int manMove = 1500; // Sets time to wait for manual turntable movement.  1500 = 1.5 seconds.  Change as wanted.
int clockWise;
int counterWise;
int currentLocation;
bool turntableMove;


/*
 * Below sets the number of indexed positions the turntable will have.  Must have at least two.
 * indexPos indexed position values must go from smallest to greatest moving in clockwise direction.
 * An indexed position -CAN NOT- be home(0) or stepsPerRevolution above.
*/
const int numPos = 4;  // Total number of indexed positions there will be for this turntable.  Change if needed.

// Define set indexed locations for turntable based on numPos.  Add/Remove/Change as needed.
int indexPos[numPos] = {  500,  1000,  1500,  2000};  

int statusCMRI[numPos];  // Array to define CMRI address status used to number of indexed positions
int oldCMRI[numPos];  // Array to define CMRI address status used to number of indexed positions
// Define set indexed locations for turntable based on numPos.  Add/Remove/Change as needed.

const int indexTime = 2000;  //Time in milliseconds to wait while rotary switch is pressed and held to do a index move.  1000 = 1 Second.
const int zeroPosClear = 10000; // Time to press and hold rotary switch before zeroing the position.
elapsedMillis buttonTimer;  //  Initalize millis timer.

void setup() {

  Serial.begin(19200); 

  for (int x = 0; x < numPos; x++) {  // Initallize CMRI arrays
    statusCMRI[x] = 0;
    oldCMRI[x] = 0;
  }

  pinMode(rotaryCLK, INPUT);        //  Pin setup for rotary CLK pin.
  pinMode(rotaryDT, INPUT);         //  Pin setup for rotary DT pin.
  pinMode(rotarySW, INPUT_PULLUP);  //  Pin setup for rotary switch SWD pin.
  pinMode(pinLED, OUTPUT);          //  Pin setup for movement LED.
  clockWise = 0;
  counterWise = 0;

  Serial.println();
  Serial.println(F("Turntable Movement Sketch v2.0-12-2023"));
  Serial.println(F("By Anthony Kochevar")); delay(3000); Serial.println();
  Serial.println(F("Arduino booted,  checking if homing to 0 is requested..."));
  Serial.println(F("Press and hold rotary switch in..."));
  Serial.print(F("5... ")); delay(1000);
  Serial.print(F("4... ")); delay(1000);
  Serial.print(F("3... ")); delay(1000);
  Serial.print(F("2... ")); delay(1000);
  Serial.print(F("1... ")); delay(1000); Serial.println();


  if (digitalRead(rotarySW) == LOW)  {
    Serial.println(F("Homing has been requested... Setting current turntable position to 0..."));
    currentLocation = 0;
    ttStepper.setCurrentPosition(0);
    Serial.println(F("Turntable set to 0... Saving to EEPROM..."));
    Serial.println(F("Release switch..."));
    digitalWrite(pinLED, HIGH);  // Blink LED three times for clearing and setting zero position.
    delay(500);
    digitalWrite(pinLED, LOW);
    delay(500);
    digitalWrite(pinLED, HIGH);
    delay(500);
    digitalWrite(pinLED, LOW);
    delay(500);
    digitalWrite(pinLED, HIGH);
    delay(500);
    digitalWrite(pinLED, LOW);
    EEPROM.update(eepromAdd, currentLocation >> 8);  // Save an Int to EEPROM.  Uses two addresses.
    EEPROM.update(eepromAdd + 1, currentLocation & 0xFF);
    delay(500);

  }
  else {
    Serial.println(F("Skipping homing...  Getting last position from EEPROM..."));
    currentLocation = (EEPROM.read(eepromAdd) << 8) + EEPROM.read(eepromAdd + 1);  // Read a Int from EEPROM.  Uses two Addresses.
    ttStepper.setCurrentPosition(currentLocation);
  }  // end else
	  
  previousStateCLK = digitalRead(rotaryCLK);
  ttStepper.setMaxSpeed(maxSpeedLimit);
  ttStepper.setSpeed(0);    // Initalize speed.
  ttStepper.setAcceleration(ttAccel);
  buttonTimer = 0;
  Serial.println(F("Turntable ready."));
  Serial.println(F("Current position:"));
  Serial.println(currentLocation);
  Serial.println();
  ttStepper.disableOutputs();  //  Turn off power to stepper when idle to reserve power and reduce heat.
}

void loop() {

//  ====Step 1 - Always check if rotary dial turned or switch is pressed====
  
  currentStateSW = digitalRead(rotarySW);
  currentStateCLK = digitalRead(rotaryCLK);
//  =====================================================

//  ==== Step 2 - If rotary is turned do the following====
  if (currentStateCLK != previousStateCLK) {
	if (digitalRead(rotaryDT) != currentStateCLK) {  // Rotary encoder is moving Counter Clockwise
		counterWise ++;
		clockWise --;
	}
	else { //Rotary Encoder is moving Clockwise
		counterWise --;
		clockWise ++;
	}
	// Check and set min and max values for counterWise and clockWise;
	if (counterWise > maxSpeedLimit) counterWise = maxSpeedLimit;
	if (counterWise < -maxSpeedLimit) counterWise = -maxSpeedLimit;
	if (clockWise > maxSpeedLimit) clockWise = maxSpeedLimit;
	if (clockWise < -maxSpeedLimit) clockWise = -maxSpeedLimit;
	previousStateCLK = currentStateCLK;
    }
  
//  ====Step 3 - Move the Turntable if move is needed====  
  if ((clockWise >= startSpeed) && (counterWise < 0)) {
    digitalWrite(pinLED, HIGH);
	  if (turntableMove == false) { 
      Serial.println(F("Counter ClockWise Manual Move...")); 
      ttStepper.enableOutputs();
      }
	  ttStepper.setAcceleration(0);  // Disable Acceleration
	  ttStepper.setSpeed((float)clockWise);
    ttStepper.runSpeed();
    currentLocation = ttStepper.currentPosition();
    if (currentLocation == stepsPerRevolution + 1) currentLocation = 0;
    ttStepper.setCurrentPosition(currentLocation);
	  turntableMove = true;
  }
  
  if ((counterWise >= startSpeed ) && (clockWise  < 0)) {
    digitalWrite(pinLED, HIGH);
	  if (turntableMove == false) { 
      Serial.println(F("ClockWise Manual Move..."));
      ttStepper.enableOutputs(); 
      }
	  ttStepper.setAcceleration(0);  // Disable Acceleration
    ttStepper.setSpeed((float)-counterWise);
    ttStepper.runSpeed();
    currentLocation = ttStepper.currentPosition();
    if (currentLocation == -1) currentLocation = stepsPerRevolution;
    ttStepper.setCurrentPosition(currentLocation);
	  turntableMove = true;
  }
  
  if ((((clockWise < startSpeed) && (counterWise < startSpeed)) && (turntableMove == true)) || ((turntableMove == true) && (currentStateSW == LOW))) {
	  turntableMove = false;
    currentStateSW = HIGH;
    digitalWrite(pinLED, LOW);
	  clockWise = 0;
	  counterWise = 0;
	  Serial.println(F("Move finished, new position:"));
    currentLocation = ttStepper.currentPosition();
    Serial.println(currentLocation);
    Serial.println();
    EEPROM.update(eepromAdd, currentLocation >> 8);  // Save an Int to EEPROM.  Uses two addresses.
    EEPROM.update(eepromAdd + 1, currentLocation & 0xFF);
    ttStepper.disableOutputs();
  }

// ======================================================

	
// ====Step 4 - Check if Rotary Switch is Pressed and move to next index position====

  buttonTimer = 0;  
  while ((currentStateSW == LOW) && (turntableMove == false)) { currentStateSW = digitalRead(rotarySW); }

  if ((buttonTimer >= indexTime) && (buttonTimer < zeroPosClear)) {
    digitalWrite(pinLED, HIGH);
    ttStepper.enableOutputs();
    Serial.println(F("Index Move Running..."));
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
    ttStepper.setSpeed(indexSpeed);    // Initalize speed.
    ttStepper.runToNewPosition(indexPos[nextPos]);
    currentLocation = indexPos[nextPos];
    ttStepper.setCurrentPosition(currentLocation);
    turntableMove = false;
    currentStateSW == HIGH;
    digitalWrite(pinLED, LOW);
	  clockWise = 0;
	  counterWise = 0;
	  Serial.println(F("Move finished, new position:"));
    currentLocation = ttStepper.currentPosition();
    Serial.println(currentLocation);
    Serial.println();
    EEPROM.update(eepromAdd, currentLocation >> 8);  // Save an Int to EEPROM.  Uses two addresses.
    EEPROM.update(eepromAdd + 1, currentLocation & 0xFF);
    buttonTimer = 0;  // Clear timer to prevent long index moves triggering homing.
    ttStepper.disableOutputs();
  }  // End if ((buttonTimer >= indexTime) && (buttonTimer < zeroPosClear))


//=======Step 5 - Check if Rotary button is pressed for homing reset=========

  if (buttonTimer >= zeroPosClear) {
    ttStepper.enableOutputs();
    Serial.println(F("Homing has been requested... Setting current turntable position to 0..."));
    currentLocation = 0;
    ttStepper.setCurrentPosition(0);
    Serial.println(F("Turntable set to 0... Saving to EEPROM..."));
    Serial.println(F("Release switch..."));
    EEPROM.update(eepromAdd, currentLocation >> 8);  // Save an Int to EEPROM.  Uses two addresses.
    EEPROM.update(eepromAdd + 1, currentLocation & 0xFF);
    digitalWrite(pinLED, HIGH);  // Blink LED three times for clearing and setting zero position.
    delay(500);
    digitalWrite(pinLED, LOW);
    delay(500);
    digitalWrite(pinLED, HIGH);
    delay(500);
    digitalWrite(pinLED, LOW);
    delay(500);
    digitalWrite(pinLED, HIGH);
    delay(500);
    digitalWrite(pinLED, LOW);
    previousStateCLK = digitalRead(rotaryCLK);
    ttStepper.setMaxSpeed(maxSpeedLimit);
    ttStepper.setSpeed(0);    // Initalize speed.
    ttStepper.setAcceleration(ttAccel);
    buttonTimer = 0;
    clockWise = 0;
    counterWise = 0;
    Serial.println(F("Turntable ready."));
    Serial.println(F("Current position:"));
    Serial.println(currentLocation);
    Serial.println();
    ttStepper.disableOutputs();
  }  // End if (buttonTimer >= zeroPosClear)

  //  ====Step 6 - Check CMRI data and make changes if change is requested====
  cmri.process();
  for (int x = 0; x < numPos; x++) {
    statusCMRI[x] = (cmri.get_bit(x));
    if ((statusCMRI[x] == 1) && (oldCMRI[x] == 0)) {
      for (int i = 0; i < numPos; i++) {
        if (i != x) {
          statusCMRI[i] = 0;
          oldCMRI[i] = 0;
        }
      }
      statusCMRI[x] = 1;
      oldCMRI[x] = 1;
      ttStepper.enableOutputs();
      ttStepper.setSpeed(indexSpeed);    // Initalize speed.
      ttStepper.setAcceleration(ttAccel);
      ttStepper.runToNewPosition(indexPos[x]);
      currentLocation = indexPos[x];
      ttStepper.setCurrentPosition(currentLocation);
      ttStepper.disableOutputs();
      EEPROM.update(eepromAdd, currentLocation >> 8);  // Save an Int to EEPROM.  Uses two addresses.
      EEPROM.update(eepromAdd + 1, currentLocation & 0xFF);
    } // End if ((statusCMRI[x] == 1) && (oldCMRI[x] == 0))
  } // End for x
  //  ====End Step 6======================================================================


}  // End void loop


