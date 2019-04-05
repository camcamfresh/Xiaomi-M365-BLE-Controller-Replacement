I'm finishing up with my prototype box and wanted to provide some updates on it. The box contains almost everything needed to start riding.
All one needs to do is plug in and screw in the box to the M365 scooter, attach/setup the Particle Electron, and download the app. From the app, you
can upload the firmware to the electron and start riding.

It currently only supports the Particle Electron, includes the NEO-7M GPS module and boost convert along with all the other necessary
components (transistors, resistors, and connectors) inside a water-resistant box.


Hardware Updates: added voltage divider for the 5V halls sensors (although the pins are 5V tolerant, better and more accurate reading is
accomplished this way); moved headlight transistor to negative side of boost converter input (rather than the positive 5V input); changed
input voltage of buzzer from 3.3V to 5V which required an additional transitor to control.

Software Updates: message timing is identical to factory microcontroller; brake/throttle sensor minimum and maximum values are dynamically
set (allowing for the use of shorted/after-market sensors); provides brake/throttle connection status & scooter disconnection alerts; plus
a few additional small features. All these updates have drastically improved the Electron's response time to the cloud; responding to
commands usually under a second (with maximum delay of usually 3-5 seconds).

Android Application Updates: the Android application is taking a bit longer than expected. The SDK provided by Particle was too slow for my
liking, so I had to trash it and interact with the Cloud API myself - this added about 3 weeks. I'll attach a few screenshots, but there 
a few important features missing such as the recording GPS messages (in case the device goes offline), and tracking changes in the location
of the scooter in case it is locked but the alarm doesn't go off.
