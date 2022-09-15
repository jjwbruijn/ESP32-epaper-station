# How To Build an ESP32 e-paper-station.

You'll need
- ESP32
- Old solum pricetag, 1.54, 2.9 preferably one with a dodgy screen
- Some thin wiress

<img src="needed.jpg>

- Disassemble the picetag using a small (dull) knife or your nails and separate all the components. Shouldn't be too hard. Throw the batteries out or save them, whatever works for you. Be careful around children with lithium cells

<img src="disassembled.jpg">

- Attach small wires to the points that are exposed through the little window in the back. You can use a pogo programmer too, whatever makes you happy. I like enameled-copper wire, I get mine from an old solenoid, you'll do years with a small spool. 

<img src="290_154_flash_pinout.jpg">
- All these pins (except for the Test/P1.0 pin) need to be hooked up to an ESP32. Make notes while you make the connections, so you can later check your work.

- Take your solder, use the tip or your iron and keep small bead of solder in place. If you put half a mm in the bead of solder, you'll easily burn of the enamel, and that allows you to solder it to pads or anything like that

<img src="soldered1.jpg">

- Be careful with the PCB: it's quite literally built down to a price, it's very lightweight and pads can come off sorta easily. That'd be bad.

- Now guide the small copper wires though the hole in the battery bay, and guide the wires all the way to the top side and maybe close to an ESP32.
- Keep track of what you hook-up to what pads.


<img src="antenna.jpg">
The zigbee antenna is indicated in red here. It sits to the side. If you can avoid placing the ESP32 directly over the battery, I'd strongly suggest you do so. Putting a wifi transmitter near the zigbee radio may cause unintended behaviour. At the very least it'll desensitize the front-end, leading to poor connections, packet loss and other issues.

<img src="assembled">
This is what it looks like when it is done!
