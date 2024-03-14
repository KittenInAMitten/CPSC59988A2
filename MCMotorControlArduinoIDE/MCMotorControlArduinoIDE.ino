/*
  CPSC 599.88 | A2: Steve's Physical Map | Dr. Lora Oehlberg
  Winter 2024 | UCID: 30089672 | C.S.
*/
#include <AccelStepper.h>
#define step1 4
#define step2 8
#define dir1 5
#define dir2 9

//https://forum.arduino.cc/t/solved-accelstepper-library-and-multiple-stepper-motors/379416/2 to control multiple steppers
const int stepperAmount = 2;

const int stepsPerRevolution = 200;  // change this to fit the number of steps per revolution
// for your motor

// Set steppers
AccelStepper xStepper(AccelStepper::DRIVER, step1, dir1);
AccelStepper yStepper(AccelStepper::DRIVER, step2, dir2);

AccelStepper* steppers[stepperAmount] = {
  &xStepper,
  &yStepper
};

// Pins
const int endX = A0;
const int endY = A1;
const int calibrationBtn = 2;

// Map coordinate limits
const long minX = -116;
const long minZ = -62;

const long maxX = 208;
const long maxZ = 262;

// max motor travel
const long MAX_TRAVEL = -9500;

// State flags
bool calibrating = true;
bool movingToNewStart = false;
bool settingUp = false;
bool initPosition = false;

// Endstop flags
bool xStop = false;
bool yStop = false;

// Button volatile flag
volatile bool forceCalibration = false;

// These constrain functions just map minecraft position coordinates to number of steps/stepper position
long constrainX(long x) {
  return constrain(map(x, minX, maxX, 0, MAX_TRAVEL), MAX_TRAVEL, 0);

}

long constrainZ(long z) {
  return constrain(map(z, minZ, maxZ, 0, MAX_TRAVEL), MAX_TRAVEL, 0 );
}

//===============
// Both function signatures below and variables for python and arduino communication has been taken from here: https://forum.arduino.cc/t/pc-arduino-comms-using-python-updated/574496
void replyToPython(String reply);

void recvWithStartEndMarkers();

const byte numChars = 64;
char receivedChars[numChars];

boolean newData = false;

//===============

void calibrationButtonFunc() {
  if(!forceCalibration) {
    forceCalibration = true;
  }
}

void setup() {

  Serial.begin(115200);
  Serial.setTimeout(5);

  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);
  pinMode(endX, INPUT);
  pinMode(endY, INPUT);
  pinMode(calibrationBtn, INPUT);

  //Set MS1 to high to enable half stepping to avoid resonance or weird clunky noises
  digitalWrite(13, HIGH);
  digitalWrite(12, HIGH);

  //Initially set the speeds
  xStepper.setMaxSpeed(1000);
  xStepper.setAcceleration(300);

  yStepper.setMaxSpeed(1000);
  yStepper.setAcceleration(300);
  
  //Move the motors to the start and hit the end stops for initial calibration
  xStepper.moveTo(20000);
  yStepper.moveTo(20000);

  //Create interrupt for the buttons for forced calibration
  attachInterrupt(digitalPinToInterrupt(calibrationBtn), calibrationButtonFunc, RISING);

  // stepper1.moveTo(-11000);
  // stepper2.moveTo(-11000);

  calibrating = true;
  movingToNewStart = false;
  settingUp = false;
  initPosition = false;
}

// Calibration function that will handle start calibration
void calibration() {
  // If X-axis end stop reached, stop X motor and set flag.
  if(!xStop && analogRead(endX) > 100) {
    xStepper.stop();
    xStepper.setCurrentPosition(0);
    xStepper.moveTo(0);
    xStop = true;
    return;
  }

  // If Y-axis end stop reached, stop Y motor and set flag.
  if(!yStop && analogRead(endY) > 100) {
    yStepper.stop();
    yStepper.setCurrentPosition(0);
    yStepper.moveTo(0);
    yStop = true;
    return;
  }

  // Once both motors have stopped
  if(!xStepper.run() && !yStepper.run()) {
    // If the motors JUST stopped at the end stops, simply move them forward 200 steps to give some room.
    if(!movingToNewStart) {
      movingToNewStart = true;
      xStepper.moveTo(-200);
      yStepper.moveTo(-200);
    } 
    // Otherwise, set their new positions as 0 and send to Serial that Arduino is ready and done calibrating.
    else {
      xStepper.setCurrentPosition(0);
      yStepper.setCurrentPosition(0);
      movingToNewStart = false;
      calibrating = false;
      xStop = false;
      yStop = false;
      settingUp = true;
      Serial.println("<Arduino is ready>");
    }
  }
}

// THis function will wait for initial positions sent by the python script to grab the position from minecraft
void waitForInitialPosition() {
  recvWithStartEndMarkers();
  // If new data is received, read it and set the new stepper target positions
  if(newData == true) {
    long x = minX;
    long z = minZ;
    String read = String(receivedChars);
    x = constrainX(read.substring(0, read.indexOf(",")).toInt());
    z = constrainZ(read.substring(read.indexOf(",") + 1, read.length()).toInt());

    xStepper.moveTo(x);
    yStepper.moveTo(z);

    String reply = String("just got this yo: X = ") + x + String(" And Z = ") + z;

    replyToPython(reply);

    // Now go to initPosition state
    calibrating = false;
    initPosition = true;
    settingUp = false;
    newData = false;
  }
}

// This function simply moves the motors to the initial position
void moveToInitialPosition() {
  for(int i = 0; i < stepperAmount; i++) {
    steppers[i]->run();
  }
  if(!xStepper.run() && !yStepper.run() ) {
    initPosition = false;
    forceCalibration = false;
    replyToPython("done");
  }
}

// The regular updating loop that will update the target positions and move the motors
void updatePosition() {
  recvWithStartEndMarkers();
  if(newData == true) {
    String read = String(receivedChars);
    long x = constrainX(read.substring(0, read.indexOf(",")).toInt());
    long z = constrainZ(read.substring(read.indexOf(",") + 1, read.length()).toInt());

    if(x != xStepper.targetPosition()) {
      xStepper.moveTo(x);
    }

    if(z != yStepper.targetPosition()) {
      yStepper.moveTo(z);
    }
  }
  newData = false;
}

// Event loop
void loop() {

  // The different states in order: Calibration State > WaitForInitialPosition State > MoveToInitialPosition State > Regular Update State
  if(calibrating) {
    calibration();
  } 
  else if(settingUp && !initPosition) {
    waitForInitialPosition();
  }
  else if(initPosition) {
    moveToInitialPosition();
  }
  else {
    
    // Check if force calibration button was pressed and start calibration the same way as start up calibration
    if(forceCalibration) {
      xStepper.moveTo(20000);
      yStepper.moveTo(20000);
      calibrating = true;
      return;
    }

    updatePosition();

    // Check if the endstops were reached.
    if(!xStop && analogRead(endX) > 100) {
      xStepper.stop();
      xStepper.setCurrentPosition(0);
      xStepper.moveTo(0);
      xStop = true;
      calibrating = true;
      yStepper.moveTo(20000);
      return;
    }

    if(!yStop && analogRead(endY) > 100) {
      yStepper.stop();
      yStepper.setCurrentPosition(0);
      yStepper.moveTo(0);
      yStop = true;
      calibrating = true;
      xStepper.moveTo(20000);
      return;
    }

    // Run the steppers
    for(int i = 0; i < stepperAmount; i++) {
      steppers[i]->run();
    }
  }

}

// ============================================== START ===================================================
// Al code between this used to communicate between Python and Arduino Serial was from here: https://forum.arduino.cc/t/pc-arduino-comms-using-python-updated/574496

void replyToPython(String reply) {
    Serial.print("<");
    Serial.print(reply);
    Serial.print('>');
}

void recvWithStartEndMarkers() {
    static boolean recvInProgress = false;
    static byte ndx = 0;
    char startMarker = '<';
    char endMarker = '>';
    char rc;

    while (Serial.available() > 0 && newData == false) {
        rc = Serial.read();

        if (recvInProgress == true) {
            if (rc != endMarker) {
                receivedChars[ndx] = rc;
                ndx++;
                if (ndx >= numChars) {
                    ndx = numChars - 1;
                }
            }
            else {
                receivedChars[ndx] = '\0'; // terminate the string
                recvInProgress = false;
                ndx = 0;
                newData = true;
            }
        }

        else if (rc == startMarker) {
            recvInProgress = true;
        }
    }
}

// ============================================= END =====================================================

