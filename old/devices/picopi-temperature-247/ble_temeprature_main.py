# Import necessary modules
from machine import Pin 
import bluetooth
from ble_temperature_peripheral import BLETemperaturePeripheral
import onewire, ds18x20
import time

from collections import deque


power_pin = 0
power = machine.Pin(power_pin, machine.Pin.OUT)
power.value(1)  # Setting the pin high


sensor_pin = machine.Pin(1)
temperature_sensor = ds18x20.DS18X20(onewire.OneWire(sensor_pin))
roms = temperature_sensor.scan()
print('Found DS devices: ', roms)


# Create a Bluetooth Low Energy (BLE) object
ble = bluetooth.BLE()

# Create an instance of the BLESimplePeripheral class with the BLE object
sp = BLETemperaturePeripheral(ble)

# Create a Pin object for the onboard LED, configure it as an output
led = Pin("LED", Pin.OUT)

# Start an infinite loop
print("Starting")


temperature_24hours = []
len_24hours = 24*12
update_24hours = 5 * 60 * 1000 * 1000 * 1000
# update_24hours = 20 * 1000 * 1000 * 1000 # speed-up time
last_update_24hours = time.time_ns() - update_24hours

temperature_7days = []
len_7days = 7*24*2
update_7days = 30  * 60 * 1000 * 1000 * 1000
# update_7days = 20 * 1000 * 1000 * 1000 # speed-up time
last_update_7days = time.time_ns() - update_7days


def encodeTemperatures(temperatures):
    encoded = bytearray() # [0xFF] * len)
    for temperature in temperatures:
        value = int(round((temperature + 20.) * 2.))
        encoded.append(value & 0xff)
    return encoded    

while True:
    temperature_sensor.convert_temp()
    time.sleep(1)
    for rom in roms:
        temperature = temperature_sensor.read_temp(rom)
        print(temperature, str(type(temperature)))
    
    if sp.is_connected():  # Check if a BLE connection is established            
        sp.update_temperature(str(temperature))  # Set the callback function for data reception
        encoded = encodeTemperatures(temperature_24hours)
        # print(encoded.hex())
        sp.write_temperature_24hours(encoded)
        encoded = encodeTemperatures(temperature_7days)
        # print(encoded.hex())
        sp.write_temperature_7days(encoded)
        led.toggle()        
        #time.sleep(10)
        
    if time.time_ns() > last_update_24hours + update_24hours: # Update 24 hours history
        last_update_24hours = time.time_ns()
        temperature_24hours.append(temperature)
        if len(temperature_24hours) > len_24hours:
            temperature_24hours.pop(0)

    if time.time_ns() > last_update_7days + update_7days:
        last_update_7days = time.time_ns()
        temperature_7days.append(temperature)
        if len(temperature_7days) > len_7days:
            temperature_7days.pop(0)
        
    time.sleep(9)

