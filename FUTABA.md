## DTM Futaba Remote Control Hardware & Build Information

![My Futaba1](img/total5.jpg)

Apart from CircuitSetup's [kit](https://circuitsetup.us/product/futaba-remote-stanley-display-wireless-control-kit/), the following additional parts are needed:

- [Unibox model 140](https://www.uniboxinfo.com/standard.html)
- "ON/OFF" Switch: Miyama MS 500 A-MF with black handle. Can by any toggle switch; has a black, flat handle in the movie.
- "UP/DOWN" Switch: [Here](https://www.aliexpress.com/item/4000512396094.html?spm=a2g0o.order_list.order_list_main.115.5d7f1c246zk58t). ON-ON version, with short handle.
- Buttons "O.O" and "RESET": [Here](https://www.aliexpress.com/item/4000638180002.html?spm=a2g0o.order_list.order_list_main.111.5d7f1c246zk58t). Used without the cap, and the stub cut shorter
- "HOLD" Switch (used as user buttons #1 and #2): [Here](https://www.aliexpress.com/item/4000512396094.html?spm=a2g0o.order_list.order_list_main.115.5d7f1c246zk58t). MOM-OFF-MOM version, with short handle.
- Pot cap on front of Unibox (Guitar/amp knob): [Here](https://www.aliexpress.com/item/33057240301.html). "Big size" version.
- Stop switch: Standard red-handle toggle switch. Google for "Plastic Toggle Switch Red".
- White wire to connect stop light and switch
- Battery clamps: [Here](https://my.rs-online.com/web/p/battery-holders/1913935?gb=s)
- Standard 9V battery adapters with open wire
- a small speaker
- a male [micro-USB plug with open wire cable](https://www.amazon.com/Maixbomr-Micro-Bare-Wire-Cable/dp/B0BZWMDB15) to turn the "tachometer" header into the charging port

Luckily, CircuitSetup's kit includes a "STOP" light (and ATM this is even an original Molex light/button). If you do your own: The Stop light is a Molex 1820. This is a bit hard to find, let alone a red one. Clear (white) ones are on ebay; go for the RL ("removable lamp") version, they are easier to modify inside. You can paint the lens using transparent red color, and put a "stop" sticker on top of it. Data for the sticker is in the Labels folder here.

![Parts](img/parts.jpg)
  
The Control Board, which is put in the grey box ("Unibox"), has the following connectors:
- [Throttle](throttle-connection) from Futaba
- [Power LED](#power-led) from Futaba
- [Battey Level Meter](#battery-level-meter) from Futaba
- ["Stop" light/switch](#stop-light-and-switch)
- Fake Power (switch)
- [Calibration](#calibration) (button)
- Buttons "O.O" and "RESET"
- 8 User Buttons (buttons or switches)
- Speaker
- LED segment display
- [LiOn battery](#battery-monitoring) for (real) battery monitoring

### Button/switch mapping

The firmware has the following minimum requirements:
- Fake Power switch (maintained)
- Buttons "O.O" and "RESET" (various functions; momentary)
- Calibration button (momentary)

Additionally, up to eight "User Buttons" can be installed, used for audio playback and/or MQTT messaging. These "buttons" can be momentary or maintained contacts. 

> _"Momentary switches"_ are switches which only remain in their _on_ state as long as they're being actuated (pressed, held), for example a push button. A _"maintained switch"_ changes state when actuated, and stays in that state until actuated again, for example a toggle switch. Maintained switches are specified using terms like "ON-OFF" or "ON-OFF-ON", while momentary switches use terms like "(ON)-OFF" or "(ON)-OFF-(ON)". The parentheses symbolize a momentary contact. ON-OFF means a 2 position switch, while ON-OFF-ON is a 3 position switch, with _off_ in the middle.

Fake power (labeled "ON/OFF"), "O.O" and "RESET" are on the Unibox. This leaves Calibration and, optionally, User Buttons to assign.

"UP/DOWN" and "HOLD" on the Unibox are free to use; however, one of those should be used as a real power switch, separating the battery from the Control Board. The other can, for instance, be equipped with a 3-position (ON)-OFF-(ON) or ON-OFF-(ON) flat handle mini toggle switch, serving as "Calibration" (on the momentary side) and one User Button. 

I used "UP/DOWN" for (real) power (ON-OFF type switch), and a (ON)-OFF-(ON) switch as "HOLD", serving as User Buttons #1 and #2.

If you are willing to modify your Futaba beyond connecting the throttle pot, you can also wire its "Fail Safe Set" button as "Calibration", and/or use the Futaba's toggle switches on the top and front as User Buttons.

The User Buttons/Switches in the following schematics are examples. 

![schematics](img/schematics1_6.png)

The switch for User Button 4 and 5 in above example must be a three-position switch with OFF in the middle.

### How to take apart the Futaba remote control

The following instructions only apply to models FP-T8SGA-P and FP-T8SGH-P.

![part1](img/htof1.jpg)

1) Remove the cover of the adjustment panel [1] on the back
2) Remove the transmitter module [2]; it is held by two clips at top and bottom
3) Remove the four silver screws [3] on the back and remove back cover

![part2](img/htof2.jpg)

4) Remove four black screws [4] on the adjustment panel, and remove the bezel

![part3](img/htof3.jpg)

5) Remove two black screws [5] on the front, in the outer corners of the lever controls, and note which screw goes into which hole (they are different). Some versions have two additional such screws further up; if present, those need to be unscrewed as well.

![part4](img/htof4.jpg)

6) Remove four black screws [6] on the bottom

![part5](img/htof5.jpg)

7) Pull out side parts; the small switch panels can be pulled out of the side parts easily.

![part6](img/htof6.jpg)

9) Remove four silver screws [7] around the antenna, and pull out the antenna as far as the wire permits; you could as well cut off that wire since the antenna isn't used.

![part7](img/htof7.jpg)

10) Remove two silver screws [8] on the top of the main PCB
11) Unplug five ribbon cables [9] on the main PCB
12) Lift out the main PCB; at this point, it is held only by pin sockets that connect it to other PCBs. 

Now you can access the throttle potentiometer to patch in black-green-red wires in order to connect this pot to the kit's Control Board, as well as all switches and buttons you may want to connect to said Control board.

Even if you do not plan on putting a battery for the mod kit inside the Futaba, it is recommended to remove the Futaba's original battery. It is heavy, removing it makes the finished prop easier to handle. Also, since the the Remote, after the modifications, can't be used normally anyway, there is no point in keeping a possibly leaking battery in it.

### Throttle connection

![pot](img/pot.jpg)

Desolder the red/green/black wires from the pot, located top left hand side (looking at it from the back), and attach new ones to connect the pot to the Control Board. The connector on the Control Board is marked R, G, Bk, matching the original wire colors.

Please note that the kit electronics are designed for models FP-T8SGA-P and FP-T8SGH-P, which use a 5k pot and resistancies of 1.8k through 3k for the throttle. If you are using a different model, the electronics might need modifications. Please contact us for more information.

### Button/switch connections

#### Toggle switches

Note that using the Futaba's original front and top toggle switches is entirely optional, and doing so is not trivial. They would serve as User Buttons for audio playback and MQTT messages.

If you want to wire the toggle switches from the Futaba remote control to the Control Board, it is recommended to use a common GND wire. This common GND wire connects all center pins of all switches to each other and to one of the GND screw connectors on the Control Board.

Some of the toggle switches on the Futaba are interconnected through their PCBs, such as the ones on top, left hand side (when viewed from the back). Check the traces on the PCBs, desoldering or cutting-off components or wire bridges might be required. The switches must be working independently, otherwise proper operation might be hindered.

Important: Do not re-connect the two widest ribbon cables (top two ones) to the main PCB when re-assembling the remote; other parts of the original circuit might interfere with toggle switch operation.

#### Calibration

The "Fail Safe Set" button on the back makes an excellent "Calibration" button. In order to use it as such, desolder the green and yellow wires, and connect your new wires to the Control Board in their place:

![FailSafeSet](img/fss.jpg)

### Power LED

Connecting the Control Board to the Futaba's power LED is optional, and this LED is a bit tricky to access, Its solder points are on the center PCB, below the power meter, under a "flying" pot that looks like a white square. You can bend up that pot to reach the solder points. Left is GND, right is VCC (when viewed from the back). You need to cut the traces on that PCB that lead away from the LED's solder points because the remaining circuit might interfere with LED operation.

Don't forget to bend the "flying pot" back after soldering, otherwise you can't put in the antenna.

![PowerLED](img/pled.jpg)

### Battery Level Meter

Connecting the Control Board to the Futaba's level meter is, of course, optional. As far as I could make out, it is never showing anything but "zero" in the movie. 

If you want to connect it to the Control Board, solder two wires to the meter's contact like shown below.

![PowerLED](img/blvl1.jpg)

![PowerLED](img/blvl2.jpg)

### Wires

The wires need to lead out of the Futaba's body. 

Since I needed some space for my battery's charging electronics (see below), I decided to leave out the main PCB entirely. This has one caveat: The antenna needs unobstructed space for movement, which means you need to take care of all the wires that might come in the way, in absence of the "protection" attached to the main PCB.

![womp1](img/womp1.jpg)

![womp2](img/womp2.jpg)

![womp3](img/womp3.jpg)

![womp4](img/womp4.jpg)

#### The way out

The final step is to take apart the transmission module, and only insert the lid into the slot.

![wires](img/wires3.jpg)

### Power supply and monitoring

The prop is powered by a 5V battery, connected to the USB port of the ESP32 board on top of the Control Board. It should be able to deliver 1A, and have a capacity of >= 2500mAh.

Depending on the shape of the battery, it can be mounted under the Unibox (instead of the second set of 9V batteries), or inside the Futaba, in place of the Futaba's original battery. In the latter case, access to the battery for charging has to be provided, since the Futaba's battery compartment is not accessible without major disassembly. One way to do this is to use two pins of the existing charging connector, or the "tachometer" connector.

I used this battery, which I sourced from [AliExpress](https://www.aliexpress.com/item/1005005621203243.html) and is the very same that CircuitSetup offers as part of their kit:

![battery](img/battery.jpg)

This is a dual-cell 3.7-4.2V LiPo battery with a MH-CD42 booster board, which shifts the voltage to 5V and provides charging protection. With a simple modification, this battery has the perfect measurements to fit into the original battery compartment. The booster board, which is inside the blue shrink-wrap, needs to be put elsewhere, otherwise the battery is too thick: Remove the blue shrink-wrap, the tape below that, and the booster board is free. I taped it on top of the battery compartment:

![battelec](img/battelec.jpg)

The charging wire is - through a micro-USB plug, center bottom of above picture - connected to the "tachometer" connector on the Futaba. A JST XH or Dupont 3-pin connector fits fine, so it's easy to make a charging adapter for any kind of power supply. (I used the center pin for + and the two outer pins for -, that way orientation of the plug does not matter. The black wire on the small Futaba PCB is +, yellow and orange are -.

The actual power cable for the kit electronics is lead outside, together with the other wires from the switches.

#### Battery monitoring

Control Boards "1.6M" and >=1.7 feature a Battery Monitor circuit for LiPo batteries. Battery monitoring requires a **direct connection to the LiPo cells** - _not the power output of the booster board_ - through either the JST PH connector on the bottom of the CB (CB1.6M only), or the "BATT+" header next to the STOP headers. When using a battery with a MH-CD32 booster board, the wires can be soldered on in parallel to the wires that lead to the LiPo cells. Watch out for shorts while soldering, you are working with live power! (Since GND is common, a wire to the battery's "+" suffices.)

![BatMonConnection](img/batmon1.jpg)

Battery header for monitoring on top of the Control Board: (Only + needed, GND is common)

![BatMonConnection](img/battmon2.jpg)

Battery connector for monitoring on bottom of Control Board 1.6M (not available on version 1.7 and later):

![BatMonConnection](img/battmon1.jpg)

**Important**: The Control Board must be connected to the LiPo battery when said battery is idle, ie not being discharged or charged. This means it must be connected while the ESP32's USB is unplugged.

### "Stop" light and switch

The Molex 1820 "Stop" light from CircuitSetup's kit is ready for use; if you do your own, it needs to be modified. A white LED needs to be fitted instead of the lamp. Since those lights are glued shut, you need an exacto knife to pry open the top cover. Then remove the lens (by pulling it out), remove the lamp socket and put a 5mm LED where the lamp would be, and push its wires next to the copper contacts leading to the rear.

![stop3](img/stoplight3.jpg)

Many people use tape (and tape alone) to attach the Molex light and the switch. Given the small area of contact and the Futaba's checkered surface, I regarded tape alone as insufficient and went for tape plus M2 screws.

The left screw is a machine screw with a nut, the other is a plastic screw (as there is no room for a nut inside the light).

![stop1](img/stoplight1.jpg)

![stop2](img/stoplight2.jpg)

Attaching the switch that way requires disassembling it, removing the small bulb, and cutting out the black inner body to make room for the screw heads:

![stops1](img/stopswitch1.jpg)

![stops2](img/stopswitch2.jpg)

The position of the Stop switch in the movie: (There were several Futabas used, all slightly different. This is the one used for the later close-ups.)

![stopspos](img/stopswitchpos.jpg)

### Mounting the Unibox on the Futaba

Since the transmitter module's lid is slightly loose and therefore isn't a properly stable base, I decided to attach the Unibox using 6mm PCB standoffs:

![standoffs](img/boxstandoffs.jpg)

The vertical position in the movie:

![boxpos](img/boxpos.png)

### The Unibox

Apart from the "UP/DOWN" switch (which I use as a real power switch), all switches and buttons have a common ground (black). The two red wires are from the "UP/DOWN" switch and will be connected to + of the ESP32, and + from the battery.

[<img src="img/box1.jpg">](img/box1l.jpg)

The small holes on the right hand side are for the speaker. Additional holes were made for the stop-switch/light (bottom right hand side), and the 9V battery cables (right hand side, near top; bottom, next to hole for stop-wires).

![box2top](img/box2t.jpg)

![box2](img/box2.jpg)

The switches are positioned and rotated as in the movie, as far as I could make out:

![boxm2](img/switchesm2.jpg)

![boxm1](img/switchesm1.jpg)

With everything wired up; it looks more chaotic than it is. The black square on the right hand side is the speaker. (Click for hi-res image)

[<img src="img/box3.jpg">](img/box3l.jpg)

Here are some details of the mostly finished product (cable ties aren't final on those pics though):

![det1](img/det1.jpg)

![det2](img/det2.jpg)

![det3](img/det3.jpg)

![det4](img/det4.jpg)

## And here it is

(Click for hi-res images)

[<img src="img/total3.jpg">](img/total3l.jpg)

[<img src="img/total4.jpg">](img/total4l.jpg)

[<img src="img/total5.jpg">](img/total5l.jpg)

[<img src="img/back1.jpg">](img/back1l.jpg)

[<img src="img/back2.jpg">](img/back2l.jpg)

_Text & images: (C) Thomas Winischhofer ("A10001986"). See LICENSE._ Source: https://remote.out-a-ti.me

