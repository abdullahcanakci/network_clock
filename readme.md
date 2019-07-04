[TOC]

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

Can be found on Parts list.

##### Implementation

Implementation is mostly about wiring diagram and code. Both of these are done on breadboard and in minimum working order as of Jun 7, 2019. 

##### Prototyping

A prototype PCB has been sent for production in Jun 5, 2019, after receiving I will add required peripherals for interaction, better layout etc. and update the project.

By July 1, I received the boards and checked if everything was in working order. More can be found on [Prototype].

##### Finishing

Hopefully by this time everything works as expected and I wrap the project.

---

#### Parts list

|      | Name               | Description                          | Quantity |
| ---- | ------------------ | ------------------------------------ | -------- |
| 1    | ESP-12 E/F variant | uC with networking capabilities.     | 1        |
| 2    | MAX7219CWG         | Display driver with serial interface | 1        |
| 3    | AMS1117-3.3        | 3V3 voltage regulator                | 1        |
| 4    | E6-1100CUR1        | 7 Segment Display. 1" Digit height   | 4        |
| 5    | DTS-3 / STS-3      | Tactile Swich                        | 4        |
| 6    | Leds 0805(2012)    | Status leds.                         | 2        |
| 7    | 220 Ohm            | Current limit resistor for leds      | 2        |
| 8    | 10K Ohm            | Pull-up/down resistor                | 4        |
| 9    | 47K Ohm            | Max7219 current reference resistor   | 1        |
| 10   | 100nF 0805(2012)   | Decoupling capacitor                 | 2        |
| 11   | 10uF 2.5mm pitch   | Bulk capacitor for displays.         | 1        |
| 12   | 0.1"x6 header      | Serial programming header            | 1        |
| 13   |                    | Micro USB type b connector           |          |

1. ESP-12E/F for microcontroller. They are mostly same and can be interchangable.

2. MAX7219 is a serially interfaced led matrix driver. One plus of this chip is digital intensity configuration.

3. AMS1117-3.3 is a Linear voltage regulator we will use it to power our microcontroller. Display driver will use 5V direclty.

4. These displays are produced by "Strong Base Investment LTD". Datasheet can be found [here](https://img.ozdisan.com/ETicaret_Dosya/41608_1428544.pdf).  Any comparable display can be used. I left 3mm space around each display so a bit wider ones can also fit (Displays are 24mm wide, I put them inside 27mm.). As long as their pin position fits.

5. DTS-3 /STS-3 vs comparable switches can be used. Datasheet of a sample can be found [here](https://eu.mouser.com/ProductDetail/Diptronics/DTS-32N-V-B?qs=sGAEpiMZZMtFyPk3yBMYYD0iDqFNlT1wSP2u7Qqc2AE%3D).

11. 10uF bulk capacitor. Display driver is a bit power hungry. Can be substituted for no smaller than ~3uF capacitor. Board has place for 2.5mm pitch 5mm axial electrolytic capacitor. But also has place for it to lie down.
12. Regular 0.1" pitch header, pogo stick etc. can be used for programming. 
13. There are lots of Micro  USB type B connectors. I think it would be best to have a through hole one to have a better leverage against wear and tear. A comparable one can be found [here](https://eu.mouser.com/ProductDetail/Hirose-Connector/ZX62D-B-5PA830?qs=sGAEpiMZZMulM8LPOQ%252Byk6r3VmhUEyMLT8hu1C1GYL85FtczwhvFwQ%3D%3D). 

---

#### Prototype

Prototype board has been received and in working order. Final hardware design finished. Some updates  and changes from prototype board applied.

Changes made:

- Larger ground planes on both sides and more stiching.
- Thermal reliefs on all pads whether through hole or smt pad. It was hard to solder some pads on the prototype.
- Removed one of the commons connection from displays to ease routing.
- Larger board size. 42mm to 50mm.
- Prototype programming power source was 3V3 which wasn't enough for display driver. Changed to 5V.
- Better routing of signal traces.
- Wider trace width. 10 to 14 mil for regular, 15 to ~20mil for power traces.
- Removed Logic translator. No really need.
- Added status Leds.
- Added 4 buttons. WPS, REFRESH, OFFSET, RESET.

Schematic and the PCB gerber file can be found on the root.




