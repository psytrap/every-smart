#!/usr/bin/python3

"""Copyright (c) 2019, Douglas Otwell

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
"""

import collections
import dbus

try:
  from gi.repository import GLib
except ImportError:
    import gobject as GLib

from advertisement import Advertisement
from service import Application, Service, Characteristic, Descriptor
from gpiozero import CPUTemperature

GATT_CHRC_IFACE = "org.bluez.GattCharacteristic1"
NOTIFY_TIMEOUT = 5000
TEMP_HISTORY_RATE = 6 * 1000 # every minute

temperature_buffer = collections.deque(maxlen=60)

# UUID 4 no XXXXXXXX-XXXX-1000-8000-00805F9B34FB
# HTML: FFC7BEB1-1836-4F63-99E0-2E7587C385A9
# Version: 430BC9B3-4FE9-4BA3-990C-F92CD64B9DCA
# Version1E567E97-2902-485E-B6ED-935D203A0F8F
# 
EVERY_SMART_SVC_UUID = "26BB33E9-705C-4191-BF2A-DE4EA169DBFC"
EVERY_SMART_VERSION_DESCRIPTOR_UUID = "430BC9B3-4FE9-4BA3-990C-F92CD64B9DCA"
EVERY_SMART_VERSION_UUID = "430BC9B3-4FE9-4BA3-990C-F92CD64B9DCB"
EVERY_SMART_HTML_DESCRIPTOR_UUID = "FFC7BEB1-1836-4F63-99E0-2E7587C385A9"
EVERY_SMART_HTML_UUID = "FFC7BEB1-1836-4F63-99E0-2E7587C385AA"
EVERY_SMART_IF_VERSION = 0

class ThermometerAdvertisement(Advertisement):
    def __init__(self, index):
        Advertisement.__init__(self, index, "peripheral")
        self.add_local_name("Thermometer")
        self.include_tx_power = True


class EverySmartService(Service):

    def __init__(self, index):

        Service.__init__(self, index, EVERY_SMART_SVC_UUID, True)
        self.add_characteristic(VersionCharacteristic(self))
        self.add_characteristic(HtmlCharacteristic(self))


class VersionDescriptor(Descriptor):
    VERSION_DESCRIPTOR_VALUE = "Provide Every Smart interface version"

    def __init__(self, characteristic):
        Descriptor.__init__(
                self, EVERY_SMART_VERSION_DESCRIPTOR_UUID,
                ["read"],
                characteristic)

    def ReadValue(self, options):
        value = []
        desc = self.VERSION_DESCRIPTOR_VALUE

        for c in desc:
            value.append(dbus.Byte(c.encode()))

        return value


class VersionCharacteristic(Characteristic):

    def __init__(self, service):
        self.notifying = False

        Characteristic.__init__(
                self, EVERY_SMART_VERSION_UUID,
                ["read"], service)
        self.add_descriptor(VersionDescriptor(self))

    def get_version(self):
        value = []
        for c in str(EVERY_SMART_IF_VERSION):
            value.append(dbus.Byte(c.encode()))
        return value

    def ReadValue(self, options):
        value = self.get_version()
        return value


class HtmlDescriptor(Descriptor):
    HTML_DESCRIPTOR_VALUE = "Provide Every Smart HTML app"

    def __init__(self, characteristic):
        Descriptor.__init__(
                self, EVERY_SMART_HTML_DESCRIPTOR_UUID,
                ["read"],
                characteristic)

    def ReadValue(self, options):
        value = []
        desc = self.HTML_DESCRIPTOR_VALUE

        for c in desc:
            value.append(dbus.Byte(c.encode()))

        return value


class HtmlCharacteristic(Characteristic):

    def __init__(self, service):
        self.html = None

        Characteristic.__init__(
                self, EVERY_SMART_HTML_UUID,
                ["read"], service)
        self.add_descriptor(HtmlDescriptor(self))

    def get_html(self):
        value = []
        if self.html is None:
            with open('raspi_cputemp.html', 'r') as file:
                self.html = file.read()
        segment = self.html[:200]
        self.html = self.html[200:]
        if not segment:
            self.html = None
        for c in segment:
            value.append(dbus.Byte(c.encode()))
        print("read segment")
        return value

    def ReadValue(self, options):
        value = self.get_html()
        return value


class ThermometerService(Service):
    THERMOMETER_SVC_UUID = "00000001-710e-4a5b-8d75-3e5b444bc3cf"

    def __init__(self, index):
        self.farenheit = True

        Service.__init__(self, index, self.THERMOMETER_SVC_UUID, True)
        self.add_characteristic(TempCharacteristic(self))
        self.add_characteristic(TempHistoryCharacteristic(self))
        self.add_characteristic(UnitCharacteristic(self))

    def is_farenheit(self):
        return self.farenheit

    def set_farenheit(self, farenheit):
        self.farenheit = farenheit

class TempCharacteristic(Characteristic):
    TEMP_CHARACTERISTIC_UUID = "00000002-710e-4a5b-8d75-3e5b444bc3cf"

    def __init__(self, service):
        self.notifying = False

        Characteristic.__init__(
                self, self.TEMP_CHARACTERISTIC_UUID,
                ["notify", "read"], service)
        self.add_descriptor(TempDescriptor(self))

    def get_temperature(self):
        value = []
        unit = "C"

        cpu = CPUTemperature()
        temp = cpu.temperature
        if self.service.is_farenheit():
            temp = (temp * 1.8) + 32
            unit = "F"

        strtemp = str(round(temp, 1)) + " " + unit
        #strtemp = "<html><p>TEST</p></html>"
        print(strtemp)
        for c in strtemp:
            value.append(dbus.Byte(c.encode()))
        return value


    def set_temperature_callback(self):
        if self.notifying:
            value = self.get_temperature()
            self.PropertiesChanged(GATT_CHRC_IFACE, {"Value": value}, [])

        return self.notifying

    def StartNotify(self):
        if self.notifying:
            return

        self.notifying = True

        value = self.get_temperature()
        self.PropertiesChanged(GATT_CHRC_IFACE, {"Value": value}, [])
        self.add_timeout(NOTIFY_TIMEOUT, self.set_temperature_callback)

    def StopNotify(self):
        self.notifying = False

    def ReadValue(self, options):
        value = self.get_temperature()

        return value

class TempDescriptor(Descriptor):
    TEMP_DESCRIPTOR_UUID = "2901"
    TEMP_DESCRIPTOR_VALUE = "Provide CPU Temperature"

    def __init__(self, characteristic):
        Descriptor.__init__(
                self, self.TEMP_DESCRIPTOR_UUID,
                ["read"],
                characteristic)

    def ReadValue(self, options):
        value = []
        desc = self.TEMP_DESCRIPTOR_VALUE

        for c in desc:
            value.append(dbus.Byte(c.encode()))

        return value



class TempHistoryCharacteristic(Characteristic):
    TEMP_HISTORY_CHARACTERISTIC_UUID = "00000003-710e-4a5b-8d75-3e5b444bc3cf"

    def __init__(self, service):
        self.notifying = False

        Characteristic.__init__(
                self, self.TEMP_HISTORY_CHARACTERISTIC_UUID,
                ["read"], service)
        self.add_descriptor(TempHistoryDescriptor(self))

    def get_temperature(self):
        value = []
        cpu = CPUTemperature()
        output = ""
        for item in temperature_buffer:
            output += str( round(item) ) + " "
        output += str(round( cpu.temperature ) )
        print(output)
        for c in output:
            value.append(dbus.Byte(c.encode()))
        return value

    def ReadValue(self, options):
        value = self.get_temperature()
        return value


class TempHistoryDescriptor(Descriptor):
    TEMP_DESCRIPTOR_UUID = "2901"
    TEMP_DESCRIPTOR_VALUE = "Provide CPU Temperature History"

    def __init__(self, characteristic):
        Descriptor.__init__(
                self, self.TEMP_DESCRIPTOR_UUID,
                ["read"],
                characteristic)

    def ReadValue(self, options):
        value = []
        desc = self.TEMP_DESCRIPTOR_VALUE

        for c in desc:
            value.append(dbus.Byte(c.encode()))

        return value



class UnitCharacteristic(Characteristic):
    UNIT_CHARACTERISTIC_UUID = "00000004-710e-4a5b-8d75-3e5b444bc3cf"

    def __init__(self, service):
        Characteristic.__init__(
                self, self.UNIT_CHARACTERISTIC_UUID,
                ["read", "write"], service)
        self.add_descriptor(UnitDescriptor(self))

    def WriteValue(self, value, options):
        val = str(value[0]).upper()
        if val == "C":
            self.service.set_farenheit(False)
        elif val == "F":
            self.service.set_farenheit(True)

    def ReadValue(self, options):
        value = []

        if self.service.is_farenheit(): val = "F"
        else: val = "C"
        value.append(dbus.Byte(val.encode()))

        return value

class UnitDescriptor(Descriptor):
    UNIT_DESCRIPTOR_UUID = "2901"
    UNIT_DESCRIPTOR_VALUE = "Configure Temperature Units (F or C)"

    def __init__(self, characteristic):
        Descriptor.__init__(
                self, self.UNIT_DESCRIPTOR_UUID,
                ["read"],
                characteristic)

    def ReadValue(self, options):
        value = []
        desc = self.UNIT_DESCRIPTOR_VALUE

        for c in desc:
            value.append(dbus.Byte(c.encode()))

        return value

app = Application()
app.add_service(EverySmartService(0))
app.add_service(ThermometerService(1))
app.register()

adv = ThermometerAdvertisement(0)
adv.register()




def TemperatureTick():
    cpu = CPUTemperature()
    v = cpu.temperature
    temperature_buffer.append(v)
    output = ""
    for item in temperature_buffer:
        output += " " + str( round(item) )
    #print(output)
    GLib.timeout_add(TEMP_HISTORY_RATE, TemperatureTick)
GLib.timeout_add(TEMP_HISTORY_RATE, TemperatureTick)


try:
    app.run()
except KeyboardInterrupt:
    app.quit()
