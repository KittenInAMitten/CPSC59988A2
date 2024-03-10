from mcpi.minecraft import Minecraft
from time import sleep
import time 
import serial
import math

# ============================================== START ===================================================
# Al code between this used to communicate between Python and Arduino Serial was from here: https://forum.arduino.cc/t/pc-arduino-comms-using-python-updated/574496

startMarker = '<'
endMarker = '>'
dataStarted = False
dataBuf = ""
messageComplete = False

#========================
#========================
    # the functions https://forum.arduino.cc/t/pc-arduino-comms-using-python-updated/574496

def setupSerial(baudRate, serialPortName):
    
    global  arduino
    
    arduino = serial.Serial(port= serialPortName, baudrate = baudRate, timeout=0, rtscts=True)

    print("Serial port " + serialPortName + " opened  Baudrate " + str(baudRate))

    waitForArduino()

#======================== https://forum.arduino.cc/t/pc-arduino-comms-using-python-updated/574496

def sendToArduino(stringToSend):
    
        # this adds the start- and end-markers before sending
    global startMarker, endMarker, arduino
    
    stringWithMarkers = (startMarker)
    stringWithMarkers += stringToSend
    stringWithMarkers += (endMarker)

    arduino.write(stringWithMarkers.encode('utf-8')) # encode needed for Python3


#================== https://forum.arduino.cc/t/pc-arduino-comms-using-python-updated/574496

def recvLikeArduino():

    global startMarker, endMarker, arduino, dataStarted, dataBuf, messageComplete

    if arduino.inWaiting() > 0 and messageComplete == False:
        x = arduino.read().decode("utf-8") # decode needed for Python3
        
        if dataStarted == True:
            if x != endMarker:
                dataBuf = dataBuf + x
            else:
                dataStarted = False
                messageComplete = True
        elif x == startMarker:
            dataBuf = ''
            dataStarted = True
    
    if (messageComplete == True):
        messageComplete = False
        return dataBuf
    else:
        return "XXX" 

#================== https://forum.arduino.cc/t/pc-arduino-comms-using-python-updated/574496

def waitForArduino():

    # wait until the Arduino sends 'Arduino is ready' - allows time for Arduino reset
    # it also ensures that any bytes left over from a previous message are discarded
    
    print("Waiting for Arduino to reset")
     
    msg = ""
    while msg.find("Arduino is ready") == -1:
        msg = recvLikeArduino()
        if not (msg == 'XXX'): 
            print(msg)


    # the program
# ============================================= END =====================================================

# Majority of this code was done by me

# Setup serial communication and minecraft listener
setupSerial(115200, "COM4")
mc = Minecraft.create(address="localhost", port=4711)

# Tick rate set so console just has to report based on ticks per sec
tickAccumulator = 0.0
lastTime = time.time()
TICKS_PER_SEC = 0.75

# This first part, simply send an initial position to the arduino for calibration.
x, y, z = mc.player.getPos()
send = str(math.floor(x)) + "," + str(math.floor(z))
print("Sending: " + send)
sendToArduino(send)

# This while loop will simply wait until the arduino responds with "done"
while True:
    arduinoRead = recvLikeArduino()
    if not (arduinoRead == 'XXX'):
        print(arduinoRead)
    if arduinoRead == "done":
        break

# This while loop will act as an updater and sends new position data. This is done as many times per second according to TICKS_PER_SEC
while True:
    # Track time since last tick
    deltaTime = time.time() - lastTime
    tickAccumulator += deltaTime
    lastTime = time.time()
    # If enough time has passed, send new data
    if tickAccumulator > TICKS_PER_SEC:
        tickAccumulator = 0.0
        x, y, z = mc.player.getPos()
        send = str(math.floor(x)) + "," + str(math.floor(z))
        sendToArduino(send)
    
    sleep