from mcpi.minecraft import Minecraft
from time import sleep
import time 

mc = Minecraft.create(address="localhost", port=4711)

# Tick rate set so console just has to report based on ticks per sec
tickAccumulator = 0.0
lastTime = time.time()
TICKS_PER_SEC = 0.75

# Constantly grab the player's position and create
# a new stone block underneath him/her
while True:
    deltaTime = time.time() - lastTime
    tickAccumulator += deltaTime
    lastTime = time.time()
    if tickAccumulator > TICKS_PER_SEC:
        tickAccumulator = 0.0
        x, y, z = mc.player.getPos()
        a, b, c = mc.player.getDirection()

		# Print Positions
        print("Position | x: {}, y: {}, z: {}".format(x,y,z))
        # Print RHS Rotations
        print("RHS - Rotation | +X: {}, +Y: {}, +Z: {}\n".format(a,b,c))
    
    sleep