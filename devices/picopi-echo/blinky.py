from machine import Pin
import time

led = Pin("LED", Pin.OUT)
while True:
    led.toggle()
    time.sleep(1)
    print("toogle")
    
    
    
    
    
    
    
    


