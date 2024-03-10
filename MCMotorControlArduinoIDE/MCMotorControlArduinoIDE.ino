#include <AccelStepper.h>
#define step1 4
#define step2 8
#define dir1 5
#define dir2 9

//https://forum.arduino.cc/t/solved-accelstepper-library-and-multiple-stepper-motors/379416/2 to control multiple steppers
const int stepperAmount = 2;

const int stepsPerRevolution = 200;  // change this to fit the number of steps per revolution
// for your motor

AccelStepper stepper1(AccelStepper::DRIVER, step1, dir1);
AccelStepper stepper2(AccelStepper::DRIVER, step2, dir2);

AccelStepper* steppers[stepperAmount] = {
  &stepper1,
  &stepper2
};

const long minX = 14;
const long minZ = -70;
const long maxX = 119;
const long maxZ = 20;

bool settingUp = true;
bool initPosition = false;

long constrainX(long x) {
  return constrain(map(x, minX, maxX, 0, -11000), -11000, 0);

}

long constrainZ(long z) {
  return constrain(map(z, minZ, maxZ, 0, -11000), -11000, 0 );
}

//===============
// Both function signatures below and variables for python and arduino communication has been taken from here: https://forum.arduino.cc/t/pc-arduino-comms-using-python-updated/574496
void replyToPython(String reply);

void recvWithStartEndMarkers();

const byte numChars = 64;
char receivedChars[numChars];

boolean newData = false;

//===============

void setup() {

  Serial.begin(115200);
  Serial.setTimeout(5);

  pinMode(13, OUTPUT);
  pinMode(12, OUTPUT);

  digitalWrite(13, HIGH);
  digitalWrite(12, HIGH);

  stepper1.setMaxSpeed(800);
  stepper1.setAcceleration(175);

  stepper2.setMaxSpeed(800);
  stepper2.setAcceleration(175);

  // stepper1.moveTo(-11000);
  // stepper2.moveTo(-11000);

  settingUp = true;
  initPosition = false;

  Serial.println("<Arduino is ready>");
}

void loop() {
  if(settingUp && !initPosition) {
    recvWithStartEndMarkers();
    if(newData == true) {
      long x = minX;
      long z = minZ;
      String read = String(receivedChars);
      x = constrainX(read.substring(0, read.indexOf(",")).toInt());
      z = constrainZ(read.substring(read.indexOf(",") + 1, read.length()).toInt());

      stepper1.moveTo(x);
      stepper2.moveTo(z);

      String reply = String("just got this yo: X = ") + x + String(" And Z = ") + z;

      replyToPython(reply);

      initPosition = true;
      settingUp = false;
      newData = false;
    }


  } else if(initPosition) {
    for(int i = 0; i < stepperAmount; i++) {
      steppers[i]->run();
    }
    if(stepper1.distanceToGo() == 0 && stepper2.distanceToGo() == 0) {
      initPosition = false;
      replyToPython("done");
    }
  } else {
    
    recvWithStartEndMarkers();
    if(newData == true) {
      String read = String(receivedChars);
      long x = constrainX(read.substring(0, read.indexOf(",")).toInt());
      long z = constrainZ(read.substring(read.indexOf(",") + 1, read.length()).toInt());

      if(x != stepper1.targetPosition()) {
        stepper1.moveTo(x);
      }

      if(z != stepper2.targetPosition()) {
        stepper2.moveTo(z);
      }
    }
    newData = false;
  }

  for(int i = 0; i < stepperAmount; i++) {
    steppers[i]->run();
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

