# Smart-Home-Hand-Gesture-Control-System
This project is composed of two parts, first is using P-NUCLEO-F401RE with a micro lidar for detecting and labeling hand gesture and 2nd is a Raspberry Pi 4 Model B for the Smart Home user interface. I use a code as a base from bluemelony12 project and edited it.


To properly credited his work here is the repository link :https://github.com/bluemelony12/STM32_AI_HandPosture_Demo.git

main.c is the main file for STM32 board while RPI_main.c is the main file for Raspberry Pi 4 Model B

This project contains a gesture-controlled smart home interface using a Raspberry Pi and STM32 with a VL53L8CX micro-LiDAR sensor. The Python script handles serial communication, menu navigation, and device control via GPIO, IR, and NeoPixel LED effects.

Raspberry Pi Interface Menu:

LCD menu system navigated by hand gestures (Swipe, Tap, Fist, Love)
Device categories: Electric Fan, Television, RGB LED, Lock, Lamp
RGB LED animations (rainbow, theater chase, etc.)
"On All" / "Off All" group controls
Idle screen triggers after inactivity and resumes on "Love" gesture
