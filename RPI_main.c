import serial
import time
import drivers
import RPi.GPIO as GPIO
import argparse
import threading
import subprocess
from rpi_ws281x import *

display = drivers.Lcd()
display.lcd_clear()

display.lcd_display_string("Preparing", 1)
display.lcd_display_string("System...", 2)
time.sleep(5)

ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)

menuItems = [
    {"name": "Electric Fan", "options": ["Power 1", "Power 2", "Off"]},
    {"name": "Television", "options": ["On/Off", "Volume Up", "Volume Down", "Channel Up", "Channel Down"]},
    {"name": "RGB LED", "options": ["Movie Theater", "Rainbow", "Rainbow Fade", "Rainbow Blink", "Rainbow Theater", "Off"]},
    {"name": "Lock", "options": ["Lock", "Unlock"]},
    {"name": "Lamp", "options": ["On", "Off"]},
    {"name": "On All", "options": []},
    {"name": "Off All", "options": []}
]

currentSelection = 0
submenuSelection = 0
animation_thread = None
stop_animation = False
lcd_dirty = True
last_command = None
in_submenu = False

GPIO.setmode(GPIO.BCM)
GPIO.setup(17, GPIO.OUT)
GPIO.setup(27, GPIO.OUT)
GPIO.setup(22, GPIO.OUT)
GPIO.setup(25, GPIO.OUT)
GPIO.setup(24, GPIO.OUT)
GPIO.output(17, GPIO.HIGH)
GPIO.output(27, GPIO.HIGH)
GPIO.output(22, GPIO.HIGH)
GPIO.output(24, GPIO.HIGH)
GPIO.output(25, GPIO.HIGH)

ac_status = 0
tv_status = 0

LED_COUNT      = 60
LED_PIN        = 18
LED_FREQ_HZ    = 800000
LED_DMA        = 10
LED_BRIGHTNESS = 65
LED_INVERT     = False
LED_CHANNEL    = 0

FAN_PINS = {
    "Power 1": 17,
    "Power 2": 27,
}

idle_timeout = 30
last_activity_time = time.time()
is_idle = False

def send_ir_signal(remote_name, button_name):
    try:
        subprocess.run(["irsend", "SEND_ONCE", remote_name, button_name], check=True)
        print(f"Sent {button_name} to {remote_name}")
    except subprocess.CalledProcessError as e:
        print("Failed to send IR signal:", e)

def colorWipe(strip, color, wait_ms=50):
    for i in range(strip.numPixels()):
        strip.setPixelColor(i, color)
        strip.show()
        time.sleep(wait_ms/1000.0)

def theaterChase(strip, color, wait_ms=50, iterations=10):
    for j in range(iterations):
        for q in range(3):
            for i in range(0, strip.numPixels(), 3):
                strip.setPixelColor(i+q, color)
            strip.show()
            time.sleep(wait_ms/1000.0)
            for i in range(0, strip.numPixels(), 3):
                strip.setPixelColor(i+q, 0)

def wheel(pos):
    if pos < 85:
        return Color(pos * 3, 255 - pos * 3, 0)
    elif pos < 170:
        pos -= 85
        return Color(255 - pos * 3, 0, pos * 3)
    else:
        pos -= 170
        return Color(0, pos * 3, 255 - pos * 3)

def rainbow(strip, wait_ms=20, iterations=1):
    for j in range(256*iterations):
        for i in range(strip.numPixels()):
            strip.setPixelColor(i, wheel((i+j) & 255))
        strip.show()
        time.sleep(wait_ms/1000.0)

def rainbowCycle(strip, wait_ms=20, iterations=5):
    for j in range(256*iterations):
        for i in range(strip.numPixels()):
            strip.setPixelColor(i, wheel((int(i * 256 / strip.numPixels()) + j) & 255))
        strip.show()
        time.sleep(wait_ms/1000.0)

def theaterChaseRainbow(strip, wait_ms=50):
    for j in range(256):
        for q in range(3):
            for i in range(0, strip.numPixels(), 3):
                strip.setPixelColor(i+q, wheel((i+j) % 255))
            strip.show()
            time.sleep(wait_ms/1000.0)

def run_animation(target_func, *args):
    global animation_thread, stop_animation
    stop_animation = True
    if animation_thread and animation_thread.is_alive():
        animation_thread.join()
    stop_animation = False
    def animation_wrapper():
        target_func(*args)
    animation_thread = threading.Thread(target=animation_wrapper)
    animation_thread.start()

active_fan_pin = None
strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, LED_FREQ_HZ, LED_DMA, LED_INVERT, LED_BRIGHTNESS, LED_CHANNEL)
strip.begin()

def displayMenu():
    display.lcd_clear()
    if not in_submenu:
        title = menuItems[currentSelection]['name']
        next_index = (currentSelection + 1) % len(menuItems)
        next_title = menuItems[next_index]['name']
        display.lcd_display_string(f">{title}", 1)
        display.lcd_display_string(f" {next_title}", 2)
    else:
        submenu = menuItems[currentSelection].get('options', [])
        if not submenu:
            display.lcd_display_string("No options", 1)
            display.lcd_display_string("Press Like", 2)
        else:
            current_option = submenu[submenuSelection]
            next_index = (submenuSelection + 1) % len(submenu)
            next_option = submenu[next_index]
            display.lcd_display_string(f">{current_option}", 1)
            display.lcd_display_string(f" {next_option}", 2)

def show_idle_screen():
    global is_idle
    display.lcd_clear()
    display.lcd_display_string(" Welcome, Heart", 1)
    display.lcd_display_string("  Pose to Start", 2)
    is_idle = True

def executeCommand(main, sub):
    global active_fan_pin, tv_status
    print(f"Executing: {main} - {sub}")
    match main:
        case "Electric Fan":
            if sub in FAN_PINS:
                if active_fan_pin and active_fan_pin != FAN_PINS[sub]:
                    GPIO.output(active_fan_pin, GPIO.HIGH)
                active_fan_pin = FAN_PINS[sub]
                GPIO.output(active_fan_pin, GPIO.LOW)
                display.lcd_display_string(f"Fan: {sub}", 1)
            elif sub == "Off":
                if active_fan_pin:
                    GPIO.output(active_fan_pin, GPIO.HIGH)
                    display.lcd_display_string("Fan OFF", 1)
                    active_fan_pin = None
                else:
                    display.lcd_display_string("Fan Already OFF", 1)
        case "Lock":
            match sub:
                case "Lock":
                    GPIO.output(22, GPIO.LOW)
                    display.lcd_display_string("Locking", 1)
                case "Unlock":
                    GPIO.output(22, GPIO.HIGH)
                    display.lcd_display_string("Unlocking", 1)
        case "RGB LED":
            match sub:
                case "Movie Theater":
                    run_animation(theaterChase, strip, Color(127, 127, 127))
                    display.lcd_display_string("Movie Theater", 1)
                case "Rainbow":
                    run_animation(rainbow, strip)
                    display.lcd_display_string("Rainbow", 1)
                case "Rainbow Fade":
                    rainbow(strip, wait_ms=50, iterations=3)
                    display.lcd_display_string("Rainbow Fade", 1)
                case "Rainbow Blink":
                    rainbowCycle(strip)
                    display.lcd_display_string("Rainbow Blink", 1)
                case "Rainbow Theater":
                    theaterChaseRainbow(strip)
                    display.lcd_display_string("Rainbow Theater", 1)
                case "Off":
                    colorWipe(strip, Color(0, 0, 0))
                    display.lcd_display_string("Off/Clear", 1)
        case "Television":
            match sub:
                case "On/Off":
                    send_ir_signal("TV_REMOTE", "POWER_BUTTON")
                    tv_status = 1 - tv_status
                case "Volume Up":
                    send_ir_signal("TV_REMOTE", "VOL_UP")
                case "Volume Down":
                    send_ir_signal("TV_REMOTE", "VOL_DOWN")
                case "Channel Up":
                    send_ir_signal("TV_REMOTE", "CHANNEL_UP")
                case "Channel Down":
                    send_ir_signal("TV_REMOTE", "CHANNEL_DOWN")
        case "Lamp":
            match sub:
                case "On":
                    GPIO.output(25, GPIO.LOW)
                case "Off":
                    GPIO.output(25, GPIO.HIGH)
        case "On All":
            if active_fan_pin:
                GPIO.output(active_fan_pin, GPIO.HIGH)
            active_fan_pin = FAN_PINS["Power 1"]
            GPIO.output(active_fan_pin, GPIO.LOW)
            if tv_status == 0:
                send_ir_signal("TV_REMOTE", "POWER_BUTTON")
                tv_status = 1
            run_animation(rainbow, strip)
            GPIO.output(22, GPIO.HIGH)
            GPIO.output(25, GPIO.LOW)
        case "Off All":
            if active_fan_pin:
                GPIO.output(active_fan_pin, GPIO.HIGH)
                active_fan_pin = None
            if tv_status == 1:
                send_ir_signal("TV_REMOTE", "POWER_BUTTON")
                tv_status = 0
            colorWipe(strip, Color(0, 0, 0))
            GPIO.output(22, GPIO.LOW)
            GPIO.output(25, GPIO.HIGH)

show_idle_screen()

# --- Main Loop ---
while True:
    current_time = time.time()
    if ser.in_waiting:
        line = ser.readline().decode('utf-8').strip()
        print(f"Received: {line}")
        last_activity_time = current_time

        if is_idle:
            if line == "Love":
                is_idle = False
                display.lcd_clear()
                display.lcd_display_string("Menu System", 1)
                display.lcd_display_string("Loading...", 2)
                time.sleep(3)
            continue

        if not in_submenu:
            if line == "Swipe Left":
                currentSelection = (currentSelection - 1) % len(menuItems)
            elif line == "Swipe Right":
                currentSelection = (currentSelection + 1) % len(menuItems)
            elif line == "Tap Toward":
                options = menuItems[currentSelection].get("options", [])
                if options:
                    in_submenu = True
                    submenuSelection = 0
                else:
                    main_name = menuItems[currentSelection]["name"]
                    executeCommand(main_name, None)
        else:
            if line == "Swipe Left":
                submenuSelection = (submenuSelection - 1) % len(menuItems[currentSelection]["options"])
            elif line == "Swipe Right":
                submenuSelection = (submenuSelection + 1) % len(menuItems[currentSelection]["options"])
            elif line == "Tap Toward":
                main_name = menuItems[currentSelection]["name"]
                sub_name = menuItems[currentSelection]["options"][submenuSelection]
                executeCommand(main_name, sub_name)
            elif line == "Fist":
                in_submenu = False

    if not is_idle and (current_time - last_activity_time > idle_timeout):
        show_idle_screen()
        in_submenu = False
        currentSelection = 0

    if lcd_dirty:
        displayMenu()
        lcd_dirty = False

    if not is_idle:
        displayMenu()

    time.sleep(0.05)
