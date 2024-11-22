



export const ES = {
  DEBUG_HTML : false, // false, // if true it tries to load the debug.html from the local filesystem
  
  EVERY_SMART_IF_VERSION : 0,

  SERVICE_UUID : "26bb33e9-705c-4191-bf2a-de4ea169dbfc",
  VERSION_DESCRIPTOR_UUID : "430bc9b3-4fe9-4ba3-990c-f92cd64b9dca",
  VERSION_UUID : "430bc9b3-4fe9-4ba3-990c-f92cd64b9dcb",
  HTML_DESCRIPTOR_UUID : "ffc7beb1-1836-4f63-99e0-2e7587c385a9",
  HTML_UUID : "ffc7beb1-1836-4f63-99e0-2e7587c385aa",

  receiveDeviceRequestedAndConnected : (data) => {
    var bluetooth_id = data.bluetooth_id;
    var msg = {
      request : (ES.DEVICE.REQUESTED_AND_CONNECTED + ES.RESPONSE),
      call_id : data.call_id,
      bluetooth_id : bluetooth_id
    }
    window.parent.postMessage(msg, window.location.origin);  
    return new ES.BluetoothDevice(bluetooth_id);
  },

  BluetoothDevice : class {
    constructor(bluetooth_id) {
      this.id = bluetooth_id;
    }
    
    async getPrimaryService(service_id) {
      return new Promise((resolve) => {
        var call_id = self.crypto.randomUUID();
        window.parent.postMessage({
          request : (ES.GATT_SERVER.GET_SERVICE + ES.REQUEST),
          call_id : call_id,
          device_id : this.id,
          service_id : service_id
        }, '*');
        var event = 'message';
        const listener = (e) => {	
          if (e.data.call_id === call_id) { // TODO error handling
            window.removeEventListener(event, listener); // Clean up the event listener
            resolve(new ES.BluetoothService(e.data.service_id));
          }
        };
        window.addEventListener(event, listener);
        
      });

    }
  },
  BluetoothService : class {
    constructor(service_id) {
      this.id = service_id;
    }
    async getCharacteristic(charactersitic_id) {
      return new Promise((resolve) => {
        var call_id = self.crypto.randomUUID();
        window.parent.postMessage({
          request : (ES.SERVICE.GET_CHARACTERISTIC + ES.REQUEST),
          call_id : call_id,
          service_id : this.id,
          characteristic_id : charactersitic_id
        }, '*');
        var event = 'message';
        const listener = (e) => {	
          if (e.data.call_id === call_id) { // TODO error handling
            window.removeEventListener(event, listener); // Clean up the event listener
            resolve(new ES.BluetoothCharacteristic(e.data.characteristic_id));
          }
        };
      window.addEventListener(event, listener);
      });
    }
  },
  BluetoothCharacteristic : class {
    constructor(charactersitic_id) {
      this.id = charactersitic_id;
    }
    async readValue() {
      return new Promise((resolve) => {
        var call_id = self.crypto.randomUUID();
        window.parent.postMessage({
          request : (ES.CHARACTERISTIC.READ_VALUE + ES.REQUEST),
          call_id : call_id,
          characteristic_id : this.id,
        }, '*');
        var event = 'message';
        const listener = (e) => {	
          if (e.data.call_id === call_id) { // TODO error handling
            window.removeEventListener(event, listener); // Clean up the event listener
            resolve(e.data.read_value);
          }
        };
        window.addEventListener(event, listener);
      });  
    }
  },
  DISCOVERY_SERVICE : {
    ID : "2e4eab42-f317-4c43-9c3e-3ec3e66ad170",
    CHARACTERISTICS : {
      HTML : "2e4eab42-f317-4c43-9c3e-3ec3e66ad171"
    }
  },
  

  ES : "es-",
  DEVICE : {
    ID : "device",
    REQUESTED : "device-requested",
    REQUESTED_AND_CONNECTED : "device-requested-and-connected",
    REQUESTED_AND_CONNECTED_AND_LOADED : "device-requested-and-connected-and-loaded",
    ON_GATT_DISCONNECT : "device-on-gatt-disconnect",
  },
  GATT_SERVER : {
    ID : "gatt",
    CONNECT : "gatt-connect",
    GET_SERVICE : "gatt-server-get-service",
    RELEASE : "gatt-server-release"
  },
  SERVICE : {
    ID : "service",
    GET_CHARACTERISTIC : "service-get-chracteristic",
    RELEASE : "charactersitic-release"
  },
  CHARACTERISTIC : {
    ID : "charactersitic",
    READ_VALUE : "charactersitic-read-value",
    WRITE_VALUE_WITH_RESPONSE : "charactersitic-write-value-with-response",
    START_NOTIFY : "charactersitic-start-notify",
    STOP_NOTIFY : "characteristic-stop-notify",
    LISTENER_VALUE_CHANGED : "characteristic-listener-value-changed",
    RELEASE : "charactersitic-release"
  },

  RESULT : "-result",
  ERROR : "-error",
  REQUEST : "-request",
  RESPONSE : "-response",
  REJECTED : "-reject",

  request(id) {
    return ES.ES + id;
  },
  response(id) {
    return ES.ES + id + ES.RESULT;
  },
  error(id) {
    return ES.ES + id + ES.ERROR;
  },/*,
  Bluetooth : {
    GattServer
  }*/
  decode(encoded)
  {
    var decoder = new TextDecoder('utf-8');
    var decoded_value = decoder.decode(encoded);
    return decoded_value; // JSON.stringify(decoded_value);
  }

}




// req = ES.request(ES.GATT_SERVER.CONNECT);
// resp = ES.response(ES.GATT_SERVER.CONNECT);
