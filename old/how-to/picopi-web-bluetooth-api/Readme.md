# Introduction

Bluetooth LE has become easier to implement than ever. In this tutorial, you will follow a how-to guide to connect your Android mobile phone to a Raspberry Pi Pico W. Additionally, you'll learn how to create your own UI using HTML technology. Explore new technology and unlock creativity.

# Modified Pi Pico code
Recently MicroPyhton received Bluetooth support for the the Raspberry Pi Pico W. For this tutorial we will program Pico as described in the Bluetooth tutorial by [Abhilekh Das](TBD).

We only replace the main.py with a more interactive version and only use the onboard LED as feedback on the Pico:

'''
# Import necessary modules
from machine import Pin 
import bluetooth
from ble_simple_peripheral import BLESimplePeripheral

# Create a Bluetooth Low Energy (BLE) object
ble = bluetooth.BLE()

# Create an instance of the BLESimplePeripheral class with the BLE object
sp = BLESimplePeripheral(ble)

# Create a Pin object for the onboard LED, configure it as an output
led = Pin("LED", Pin.OUT)

# Initialize the LED state to 0 (off)
led_state = 0

# Define a callback function to handle received data
def on_rx(data):
    print("Data received: ", data)  # Print the received data
    sp.send(data) # Echo back to the client
    global led_state  # Access the global variable led_state
    led.value(not led_state)  # Toggle the LED state (on/off)
    led_state = 1 - led_state  # Update the LED state

# Start an infinite loop
while True:
    if sp.is_connected():  # Check if a BLE connection is established
        sp.on_write(on_rx)  # Set the callback function for data reception
'''

To verify that Bluetooth is active on the Pico, we'll use our mobile phone to scan for Bluetooth devices and ensure that the device 'mpy-uart' is visible in the list. In the same way as described by Abhilekh Das we can use the Serial Bluetooth Terminal app and check the options to send and receive data from our Pico app.

# HTML code

In the next step we'll explore how to access our Pico app using the Web Bluetooth API. Currently only the Chrome browsers provides decent support for Bluetooth. For better debugging we'll start using it on the Desktop PC we use to develop this tutorial. As a first step the experimental flag [#enable-experimental-web-platform-features](chrome://flags/#enable-experimental-web-platform-features) needs to be enabled on Chrome. Then we go to use a [sample app](https://googlechrome.github.io/samples/web-bluetooth/discover-services-and-characteristics-async-await.html) provided by the Google Chrome team to check Chrome can access our Pico app using the UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e".

To better understand what these UUIDs are about, check out this [Bluetooth introduction](https://learn.adafruit.com/introduction-to-bluetooth-low-energy) by adafruit.


But now it's time to dive into the details of HTML and JS. Let's create a HTML file in the local file system and open it with Chrom. It contains:
* A button to initiate connecting with a Bluetooth device
* An input field containing a message being sent to the device
* A button to initiate sending data to the device
* A text area for logging information

'''
<!DOCTYPE html>
<html>
<head>
    <title>Bluetooth Echo</title>
    <style>
        #logging-info {
            width: 100%;
    box-sizing: border-box;
            
            height: 200px;
            overflow: auto; /* Enable scrolling if the content is larger than the div */
            border: 1px solid #000;
            padding: 10px;
            margin: 2px;
            margin-bottom: 10px;

            
        }
        #logging-info p {
          margin-bottom: 0.25em; /* Adjust this value as needed */
          margin-top: 0.25em; /* Adjust this value as needed */
        }
    </style>
</head>
<body>
  <button id="connect_device" onclick="connectButton()">Connect to device</button>

  <h1>Bluetooth Echo</h1>
  <input id="log-message-input" type="text" placeholder="Enter a message" />
  <br/>
  <button onclick="sendMessage()">Send message</button>
  <div id="logging-info">
        <!-- Log messages will be added here -->
  </div>
</body>
<script>
  var bluetooth = null;
  const UART_SERVICE_ID = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E'.toLowerCase();
  const CHARACTERISTIC_TX = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E'.toLowerCase();
  const CHARACTERISTIC_RX = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E'.toLowerCase();

</script>
</html>
'''

Let's add the logic for connecting to a device by adding the JS code below to the script section of the HTML file. Relaod the file in Chrome and press the "Connect to device" button and the dialog for selecting a device appears. 'Press Control+Shift+I' and check the Console logs

'''
function connectButton()
{
  requestDevice();
}

async function requestDevice() {
  console.log('Requesting any Bluetooth Device...');
  var device = await navigator.bluetooth.requestDevice({
   // filters: [...] <- Prefer filters to save energy & show relevant devices.
      acceptAllDevices: true,
      optionalServices: [ UART_SERVICE_ID ] // TODO
    });
   await connectDevice(device);
   console.log("Device connected");
}

async function onDisconnected() {
  console.log('> Bluetooth Device disconnected');
}

async function connectDevice(device) {
  // filled later
}
'''

To send something to our Pico app we'll extend the JS section with the code below. After reloading we connect again to the device, enter some text in the input field and press "Send message". When everything works as expected the onboard LED on the Pico will toggle with every message being sent.


'''
async function sendMessage() {
    var inputField = document.getElementById('log-message-input');
    let textToWrite = inputField.value;  // Your string here
    var service = await bluetooth.getPrimaryService(UART_SERVICE_ID);
    console.log("Getting characteristic");            
    var characteristic = await service.getCharacteristic(CHARACTERISTIC_RX);
    console.log("Write value");    
    let encoder = new TextEncoder(); // Encode string to bytes
    let data = encoder.encode(textToWrite);    
    await characteristic.writeValue(data);
}

'''

Communicating in the other direction requires two steps. First we will register handler for notification after requesting the device by adding code the 'connectDevice()' function left empty in earlier steps. When ever the Pico app sends data the handler callback 'handleNotifications()' is invoked to process the received data in our JS. Now when we press the "Send message" button the LED toogles and short after the message sent echoes back and gets added to the log area.


'''
async function connectDevice(device) {
  device.addEventListener('gattserverdisconnected', onDisconnected);
  bluetooth = await device.gatt.connect();
  return new Promise(async (resolve) => {
    console.log("Getting UART service");      
    var service = await bluetooth.getPrimaryService(UART_SERVICE_ID);
    console.log("Getting characteristic");            
    var characteristic_tx = await service.getCharacteristic(CHARACTERISTIC_TX);
    await characteristic_tx.startNotifications();
    characteristic_tx.addEventListener('characteristicvaluechanged', handleNotifications);    
  });
}

function handleNotifications(event) {
    let value = event.target.value;
    let decoder = new TextDecoder('utf-8');
    let str = decoder.decode(value);

    console.log(`Received notification: ${str}`);
    var newLogEntry = document.createElement("p");
    newLogEntry.textContent = str;    
    var loggingDiv = document.getElementById('logging-info');
    loggingDiv.appendChild(newLogEntry);    
}

function decode(encoded)
{
    var decoder = new TextDecoder('utf-8');
    var decoded_value = decoder.decode(encoded);
    return JSON.stringify(decoded_value)
}

'''

Checkout the [Web Bluetooth API documentation](https://developer.mozilla.org/en-US/docs/Web/API/Web_Bluetooth_API) for all ways to access a device.

# Deploy to Android

To progressively debug your code, you can send the HTML file with a chat program to your Android device and open it there with the Chrome browser. Make sure the experimental flag is set their as well. For more convenient deployment you might setup a webpage hosting your HTML file. Git Hub pages offers a convenient way to hosting your HTML code along with your Pico code. To make it even more convenient, convert it to a Progressive Web App which can be added to the Android home screen and use off-line.



Links

What is BLE -> https://learn.adafruit.com/introduction-to-bluetooth-low-energy

MicroPython -> https://www.raspberrypi.com/documentation/microcontrollers/micropython.html
MicroPython Bluetooth -> https://github.com/micropython/micropython/tree/master/examples/bluetooth

PicoPi -> https://electrocredible.com/raspberry-pi-pico-w-bluetooth-ble-micropython/
RasPi -> https://github.com/Douglas6/cputemp



Web Bluetooth API -> https://developer.mozilla.org/en-US/docs/Web/API/Bluetooth
Examples -> https://googlechrome.github.io/samples/web-bluetooth/

Host with GitHub pages -> docs folder


Outlook PWA

