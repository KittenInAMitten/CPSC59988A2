#include <AccelStepper.h>
#define step1 4
#define step2 8
#define dir1 5
#define dir2 9

//https://forum.arduino.cc/t/solved-accelstepper-library-and-multiple-stepper-motors/379416/2 to control multiple steppers
const int stepperAmount = 2;

const int stepsPerRevolution = 200;  // change this to fit the number of steps per revolution
// for your motor

AccelStepper xStepper(AccelStepper::DRIVER, step1, dir1);
AccelStepper yStepper(AccelStepper::DRIVER, step2, dir2);

AccelStepper* steppers[stepperAmount] = {
  &xStepper,
  &yStepper
};

const int endX = A0;
const int endY = A1;
const int calibrationBtn = 2;

const long minX = 14;
const long minZ = -70;
const long maxX = 119;
const long maxZ = 20;

const long MAX_TRAVEL = -9500;

bool calibrating = true;
bool movingToNewStart = false;
bool settingUp = false;
bool initPosition = false;

bool xStop = false;
bool yStop = false;

volatile bool forceCalibration = false;

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

  digitalWrite(13, HIGH);
  digitalWrite(12, HIGH);

  xStepper.setMaxSpeed(1000);
  xStepper.setAcceleration(300);

  yStepper.setMaxSpeed(1000);
  yStepper.setAcceleration(300);

  xStepper.moveTo(20000);
  yStepper.moveTo(20000);

  attachInterrupt(digitalPinToInterrupt(calibrationBtn), calibrationButtonFunc, RISING);

  // stepper1.moveTo(-11000);
  // stepper2.moveTo(-11000);

  calibrating = true;
  movingToNewStart = false;
  settingUp = false;
  initPosition = false;
}

void calibration() {
  if(!xStop && analogRead(endX) > 100) {
    xStepper.stop();
    xStepper.setCurrentPosition(0);
    xStepper.moveTo(0);
    xStop = true;
    return;
  }

  if(!yStop && analogRead(endY) > 100) {
    yStepper.stop();
    yStepper.setCurrentPosition(0);
    yStepper.moveTo(0);
    yStop = true;
    return;
  }

  if(!xStepper.run() && !yStepper.run()) {
    if(!movingToNewStart) {
      movingToNewStart = true;
      xStepper.moveTo(-200);
      yStepper.moveTo(-200);
    } else {
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

void waitForInitialPosition() {
  recvWithStartEndMarkers();
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

    calibrating = false;
    initPosition = true;
    settingUp = false;
    newData = false;
  }
}

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

void loop() {

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
    
    if(forceCalibration) {
      xStepper.moveTo(20000);
      yStepper.moveTo(20000);
      calibrating = true;
      return;
    }

    updatePosition();

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

