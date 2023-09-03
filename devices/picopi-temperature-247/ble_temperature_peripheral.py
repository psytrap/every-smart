
# This example demonstrates a UART periperhal.

import bluetooth
import random
import struct
import time
from ble_temperature_advertising import advertising_payload

from micropython import const

_IRQ_CENTRAL_CONNECT = const(1)
_IRQ_CENTRAL_DISCONNECT = const(2)

_FLAG_READ = const(0x0002)
_FLAG_WRITE_NO_RESPONSE = const(0x0004)
_FLAG_WRITE = const(0x0008)
_FLAG_NOTIFY = const(0x0010)

_TEMPERATURE_UUID = bluetooth.UUID("00000001-710e-4a5b-8d75-3e5b444bc3cf")

_TEMPERATURE_CURRENT = (
    bluetooth.UUID("00000002-710e-4a5b-8d75-3e5b444bc3cf"),
    _FLAG_NOTIFY,
)
_TEMPERATURE_24_HOURS = (
    bluetooth.UUID("00000003-710e-4a5b-8d75-3e5b444bc3cf"),
    _FLAG_READ,
)

_TEMPERATURE_7DAYS = (
    bluetooth.UUID("00000004-710e-4a5b-8d75-3e5b444bc3cf"),
    _FLAG_READ,
)

_TEMPERATURE_SERVICE = (
    _TEMPERATURE_UUID,
    (_TEMPERATURE_CURRENT, _TEMPERATURE_24_HOURS, _TEMPERATURE_7DAYS),
)


class BLETemperaturePeripheral:
    def __init__(self, ble, name="pico-247"):
        self._ble = ble
        self._ble.active(True)
        self._ble.irq(self._irq)
        ((self._handle_temperature, self._handle_temperature_24_hours, self._handle_temperature_7days),) = \
                self._ble.gatts_register_services((_TEMPERATURE_SERVICE,))
        self._connections = set()
        self._payload = advertising_payload(name=name, services=[_TEMPERATURE_UUID])
        self._advertise()

    def _irq(self, event, data):
        # Track connections so we can send notifications.
        if event == _IRQ_CENTRAL_CONNECT:
            conn_handle, _, _ = data
            print("New connection", conn_handle)
            self._connections.add(conn_handle)
        elif event == _IRQ_CENTRAL_DISCONNECT:
            conn_handle, _, _ = data
            print("Disconnected", conn_handle)
            self._connections.remove(conn_handle)
            # Start advertising again to allow a new connection.
            self._advertise()

    def update_temperature(self, data):
        for conn_handle in self._connections:
            self._ble.gatts_notify(conn_handle, self._handle_temperature, data)

    def write_temperature_24hours(self, data):
        for conn_handle in self._connections:
            self._ble.gatts_write(self._handle_temperature_24_hours, data)

    def write_temperature_7days(self, data):
        for conn_handle in self._connections:
            self._ble.gatts_write(self._handle_temperature_7days, data)

    def is_connected(self):
        return len(self._connections) > 0

    def _advertise(self, interval_us=500000):
        print("Starting advertising")
        self._ble.gap_advertise(interval_us, adv_data=self._payload)


def demo():
    ble = bluetooth.BLE()
    p = BLETemperaturePeripheral(ble, "seppi")

    i = 0
    while True:
        if p.is_connected():
            # Short burst of queued notifications.
            for _ in range(3):
                data = str(i) + "_"
                print("TX", data)
                #p.send(data)
                i += 1
        time.sleep_ms(100)


if __name__ == "__main__":
    demo()