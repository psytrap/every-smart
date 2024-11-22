
console.log("every-smart.js");

var UART_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E".toLowerCase(); // TODO ES_SERVICE_UUID
var UART_CHARACTERISTIC_UUID_RX = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E".toLowerCase();
var UART_CHARACTERISTIC_UUID_TX = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E".toLowerCase();

var bluetooth = {
  device: null,
  server: null,
}

var chunk = '';

let asyncQueue = [];
let isQueueBusy = false;

function queueAsyncOperation(operation) {
  asyncQueue.push(operation);
  processAsyncQueue();
}

async function processAsyncQueue() {
  if (isQueueBusy || asyncQueue.length === 0) return;

  isQueueBusy = true;
  const currentOperation = asyncQueue.shift();

  try {
    await currentOperation();
  } catch (error) {
    console.error("GATT operation failed:", error);
  } finally {
    isQueueBusy = false;
    processAsyncQueue(); // Recursion to Process the next operation
  }
}

const es_root = document.getElementById('es-root');
async function handleConnectDeviceClick() {
  if (bluetooth.device !== null) {
    disconnectFromDevice(bluetooth.device);
    bluetooth.device = null;
  }
  requestDevice();
}
const connect_button = document.getElementById('connect_device');
connect_button.addEventListener('click', handleConnectDeviceClick);

async function handleDisconnectDeviceClick() {
  if (bluetooth.device !== null) {
    disconnectFromDevice(bluetooth.device);
    bluetooth.device = null;
  }
  let disconnect = document.getElementById("disconnect_device");
  disconnect.disabled = true;
}
const disconnect_button = document.getElementById('disconnect_device');
disconnect_button.addEventListener('click', handleDisconnectDeviceClick);


async function connectionFailed() {
  let status = document.getElementById("device_status");
  status.setAttribute("value", "Connection failed")
  // sleep 2 seconds
  await new Promise(resolve => setTimeout(resolve, 3 * 1000));
  let connect = document.getElementById("connect_device");
  connect.disabled = false;
  status.setAttribute("value", "Disconnected");

}

async function requestDevice() {
  let connect = document.getElementById("connect_device");
  connect.disabled = true;
  let status = document.getElementById("device_status");
  status.setAttribute("value", "Connecting");

  try {
    var device = await navigator.bluetooth.requestDevice({
      // filters: [{ services: [UART_SERVICE_UUID] }], 
      acceptAllDevices: true,
      optionalServices: [UART_SERVICE_UUID] // TODO
    });
    device.addEventListener('gattserverdisconnected', onDisconnected);
    bluetooth.device = device;
    bluetooth.server = await device.gatt.connect();
    let disconnect = document.getElementById("disconnect_device");
    disconnect.disabled = false;
  } catch (error) {
    console.log(error);
    connectionFailed();
    return;
  }
  const service = await bluetooth.server.getPrimaryService(UART_SERVICE_UUID);
  const characteristic_tx = await service.getCharacteristic(UART_CHARACTERISTIC_UUID_TX);

  await characteristic_tx.startNotifications();

  characteristic_tx.addEventListener('characteristicvaluechanged', async (event) => {
    const value = decode(event.target.value);
    // console.log('Notification:', value);
    if (value.charAt(0) === 'X') {
      // TODO Handshake??
      return;
    }
    chunk += value.substring(1)
    if (value.charAt(0) === '.') {
      return;
    }
    console.log(chunk);
    const jsonCommand = JSON.parse(chunk);
    chunk = '';
    if (jsonCommand.command === "page") {
      while (es_root.firstElementChild) {
        es_root.removeChild(es_root.lastElementChild);
      }
      parsePagePayload(jsonCommand.payload, es_root);
    }
    if (jsonCommand.command === "set_value") {
      let id = jsonCommand.payload.id;
      let element = es_root.querySelector(`#${id}`);
      element.value = jsonCommand.payload.value;
    }
    if (jsonCommand.command === "request_value") {
      let id = jsonCommand.payload.id;
      let element = es_root.querySelector(`#${id}`);
      let value = element?.value;
      const service = await bluetooth.server.getPrimaryService(UART_SERVICE_UUID);
      const characteristic_rx = await service.getCharacteristic(UART_CHARACTERISTIC_UUID_RX);
      queueAsyncOperation(async () => {
        await characteristic_rx.writeValueWithoutResponse(encode(`#{"event": "requested_value", "payload": {"id": "${id}", "value": "${value}"} }`));
      });
    }
    if (jsonCommand.command === "set_image") {
      let id = jsonCommand.payload.id;
      let element = es_root.querySelector(`#${id}`);
      element.setAttribute("src", jsonCommand.payload.src);

    }
  });

  const characteristic_rx = await service.getCharacteristic(UART_CHARACTERISTIC_UUID_RX);
  queueAsyncOperation(async () => {
    await characteristic_rx.writeValueWithoutResponse(encode("#{\"event\": \"ready\"}"));
  });
  console.log("Device connected");
  status.setAttribute("value", "Connected");
}

function parsePagePayload(payload, dom) {
  const type = payload.type;
  if (type === "button") {
    let element = document.createElement("div");
    let button = document.createElement("button");
    button.setAttribute("id", payload.properties.id);
    button.setAttribute("class", "btn btn-primary");
    button.textContent = payload.properties.label;
    button.addEventListener('click', async function () {
      const service = await bluetooth.server.getPrimaryService(UART_SERVICE_UUID);
      const characteristic_rx = await service.getCharacteristic(UART_CHARACTERISTIC_UUID_RX);
      queueAsyncOperation(async () => {
        await characteristic_rx.writeValueWithoutResponse(encode("#{\"event\": \"click\", \"payload\": {\"id\": \"" + this.id + "\"} }"));
      });

      console.log("Button clicked: " + this.id);
    });
    element.append(button);
    dom.append(element);
  }
  if (type === "text" || type === "number" || type === "time") {
    let element = document.createElement("div");
    element.setAttribute("class", "form-group");
    let label = document.createElement("label");
    label.setAttribute("for", payload.properties.id);
    label.textContent = payload.properties.label;
    let input = document.createElement("input");
    input.setAttribute("type", type);
    input.setAttribute("id", payload.properties.id);
    input.setAttribute("class", "form-control");
    input.setAttribute("value", payload.properties.value);
    if (payload.properties.readonly === "true") {
      input.readOnly = true;
    }
    if (type === "number") {
      input.setAttribute("step", payload.properties.step);
    }
    if (payload?.properties?.update !== "false") {
      input.addEventListener('input', async function () {
        const service = await bluetooth.server.getPrimaryService(UART_SERVICE_UUID);
        const characteristic_rx = await service.getCharacteristic(UART_CHARACTERISTIC_UUID_RX);
        queueAsyncOperation(async () => {
          await characteristic_rx.writeValueWithoutResponse(encode("#{\"event\": \"input\", \"payload\": {\"id\": \"" + this.id + "\", \"value\": \"" + this.value + "\"} }"));
        });
        console.log("Value input: " + this.id);
      });
    }
    element.append(label, input)
    dom.append(element);
  }
  if (type === "image") {
    let element = document.createElement("div");
    let image = document.createElement("img");
    image.setAttribute("id", payload.properties.id);
    image.setAttribute("src", payload.properties.src);
    element.append(image);
    dom.append(element);
  }
  if (type === "layout") {
    let element = document.createElement("div");
    element.setAttribute("id", payload.properties.id);
    element.setAttribute("style", `display: grid; gap: 10px; grid-auto-flow: ${payload.properties.flow};`);
    for (const layout of payload.layout) {
      parsePagePayload(layout, element);
    }
    dom.append(element);
  }
}

async function getService(service_id) {
  var service = await bluetooth.server.getPrimaryService(service_id);
  bluetooth.service.service = service;
  bluetooth.service.id = service_id;
}


// TODO Write Guard Delay better handshake/Acknoledge
// TODO Mutex


// /* TODO no clue about this */
// clear / grey out page
async function onDisconnected() {
  console.log('> Bluetooth Device disconnected');
  let status = document.getElementById("device_status");
  status.setAttribute("value", "Disconnected");
  let connect = document.getElementById("connect_device");
  connect.disabled = false;
  let disconnect = document.getElementById("disconnect_device");
  disconnect.disabled = true;

  bluetooth.device = null;
}


async function disconnectFromDevice(device) {
  if (device.gatt.connected) {
    chunk = '';
    try {
      await device.gatt.disconnect();
      console.log('Disconnected from Bluetooth device!');
      // Optionally, update UI elements or perform other actions after disconnection
    } catch (error) {
      console.error('Error disconnecting:', error);
      // Handle the disconnection error, e.g., display an error message
    }
  } else {
    console.log('Bluetooth device is already disconnected.');
  }
}

function encode(str) {
  const encoder = new TextEncoder();
  return encoder.encode(str).buffer; // Encode string and get its buffer
}

function decode(encoded) {
  var decoder = new TextDecoder('utf-8');
  var decoded_value = decoder.decode(encoded);
  return decoded_value; // JSON.stringify(decoded_value);
}