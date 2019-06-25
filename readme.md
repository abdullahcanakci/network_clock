### What is this?

This is the second version of my clock with network time keeping with ESP12-E/F variant. On chip time keeping is good but as I experienced with DS3231 it drifts about a minute every two days. Which is not good for long time time keeping without interaction.

---

#### Design Limits

For this project I want to have a real PCB so I won't have to fiddle with protoboards and point-to-point soldering. Also replicating a device would be so easy with a PCB.

For PCB manufacturing I shopped around my local fabs, they wanted an arm and a leg. They have a lead time of 1 month and DHL shipping which I thought they really outsource their production.

For a cheaper alternative there is JLCPCB which is cheap for board smaller than 100mm on the long side. For I couldn't really found a comparable source I have to accept their restrictions.

For display I will use 7-Segment displays with separate digits. Just because most of them are sharing their pin out I can just change them with a equivalent one easily if need arises. Biggest displays I can fit into 100 mm board is 1" character height ones which has 24-25 mm width.

---

#### Road map

Creating this device will be 4 part endeavor.

1. Hardware Selection
2. Implementation
3. Prototyping
4. Finishing



##### Hardware Selection

For main micro controller I choose ESP12E/F variant, because it is cheap, widely available, has WiFi integration with on board antenna.

For display driver I will be using MAX7219. It is serially interface 8 digit display driver. The problem is this is a 5V CMOS compliant device, and ESP12 is 3V3 device and their V<sub>IH</sub> is not compatible.

|                | ESP-12E/F  | MAX7219    |
| -------------- | ---------- | ---------- |
| V<sub>IH</sub> | max = 3.3V | min = 3.7V |

As far as I tested on physical it didn't really created any problem, but there is no telling when or how a problem might occur.

To fix this I will be using 74VHCT50. This is a TTL to CMOS logic converter. TTL logic high min is around 2V which ESP-12 will satisfy and the outputs will satisfy MAX7219. 

For ease of use this has input and output on pins next to each other. One can not use this chip and actively use the same PCB just by bridging contacts.

##### Implementation

Implementation is mostly about wiring diagram and code. Both of these are done on breadboard and in minimum working order as of Jun 7, 2019. 

##### Prototyping

A prototype PCB has been sent for production in Jun 5, 2019, after receiving I will add required peripherals for interaction, better layout etc. and update the project.

##### Finishing

Hopefully by this time everything works as expected and I wrap the project.

---

#### Parts list

By the end of the project it will be somewhat different but the roadmap is set with these devices.

- ESP-12E/F variant
- MAX7219 Display Driver
- 74VHCT50 Logic Level Shifter
- 4 1" 7-segment displays
- Few buttons to interact with
- Few status leds, like network, error etc
- Resistors, diodes, connectors etc. will be decided on values, models shortly after prototpying.

---

#### Prototype

First prototype has been designed and send for fabrication. This doesn't have external components like buttons and leds to inform but a programming interface so constant development can be done on the device.
Schematic and the PCB can be found on inside Phsical device folder.




