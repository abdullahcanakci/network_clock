### What is this?

This is the second version of my clock with network time keeping with ESP8266 variant, a serial to parallel chip like 595 and a pcb design to pack it all together. Apart from the last time this will be as barebones as can be for the external components.

---

#### Parts list

By the end of the project it will be somewhat different but the roadmap is set with these devices.

- ESP8266 variant
- 595 type Serial-2-Paraller interface
- Multiple N-Mos for digit selecting
- 4 7-segment displays
- Few buttons to interact with
- Few status leds, like network, error etc



---

#### Design Limits

While designing the circuit and the PCB everything as changable as can be. There should not be need for searching a specific mosfet. A logic level sot23 should do. Or 7-segment displays. Most of the solo ones share their pinouts. But if we want to use groups of 2 or 4, it gets hairy on the pinout. 

For PCB I want it to be shorter than 100 mm on the long side. Just so I can use those cheap PCB services. This condition made my choice for 7-segment size. anything larger than 25mm on the short side was out. This left me with displays, that their character sizes were smaller than 25.4mm. I choose 1" displays. But at the moment it is subject to change.



