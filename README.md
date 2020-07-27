# BlueRetro
BlueRetro is a multiplayer Bluetooth controllers adapter for various retro game consoles.
![BlueRetro](https://cdn.hackaday.io/images/5806801590105510986.jpg)
Lost or broken controllers? Reproduction too expensive? Need those rare and obscure accessories? Just use the Bluetooth devices you already got! The project is open source hardware & software. It's built for the popular ESP32 chip. All processing for Bluetooth and HID input decoding is done on a single core which makes it easy for other projects to use the Bluetooth stack within their own project as long the application is running on the other core. Wii, Switch, PS3, PS4, Xbox One & generic HID Bluetooth devices are supported. Saturn, N64, GameCube & Dreamcast are supported with simultaneous 4+ players using a single adapter. Soon Atari 5200, NES, SNES, Genesis/Megadrive, PSX, PS2, PCE....
This is my wildcard entry to the #2020HACKADAYPRIZE

# BlueRetro logs entries
## Project documentation
* [Main hackaday.io page](https://hackaday.io/project/170365-blueretro)
* [Web-Bluetooth (BLE) configuration interface](https://hackaday.io/project/170365-blueretro/log/180020-web-bluetooth-ble-configuration-interface)
## General documentation
* [Learning Bluetooth Classic (BR/EDR) with HCI traces](https://hackaday.io/project/170365-blueretro/log/178249-learning-bluetooth-classic-bredr-with-hci-traces)
* [Xbox One Adaptive controller](https://hackaday.io/project/170365-blueretro/log/179869-xbox-one-adaptive-controller)
* [Evolution of SEGA's IO Interface from SG-1000 to Saturn](https://hackaday.io/project/170365-blueretro/log/180790-evolution-of-segas-io-interface-from-sg-1000-to-saturn)
* [Famicom & NES controller shift register: Parallel-in, Serial-out](https://hackaday.io/project/170365-blueretro/log/181368-famicom-nes-controller-shift-register-parallel-in-serial-out)
## Project updates
* [2020-05-22 - Project background](https://hackaday.io/project/170365-blueretro/log/177934-background-and-current-status)
* [2020-05-27 - Wiimote & extensions support](https://hackaday.io/project/170365-blueretro/log/178223-2020-05-27-progress-update)
* [2020-06-04 - Generic HID Gamepad support](https://hackaday.io/project/170365-blueretro/log/178734-2020-06-04-progress-update)
* [2020-06-21 - Xbox adaptive & Atari 5200 early WIP](https://hackaday.io/project/170365-blueretro/log/179590-2020-06-21-progress-update)
* [2020-06-23 - ESP32 IDF upgrade](https://hackaday.io/project/170365-blueretro/log/179680-2020-06-23-progress-update)
* [2020-07-02 - HID Keyboard support](https://hackaday.io/project/170365-blueretro/log/180112-2020-07-02-progress-update)
* [2020-07-19 - SEGA Saturn support](https://hackaday.io/project/170365/log/181004-2020-07-19-progress-update-sega-saturn-support)
# Other links
* [Software files repository](https://github.com/darthcloud/BlueRetro)
* [BlueRetro mapping label reference](https://docs.google.com/spreadsheets/d/e/2PACX-1vRln_dhkahEIhq4FQY_p461r5qvLn-Hkl89ZtfyIOGAqdnPtQZ5Ihfsjvd94fRbaHX8wU3F-r2ODYbM/pubhtml)
* [Development environment setup & building instruction](https://github.com/darthcloud/BlueRetroRoot)
* [Hardware building instruction](https://github.com/darthcloud/BlueRetro/wiki)
* [Hardware files repository](https://github.com/darthcloud/BlueRetroHW)
* [Advance configuration web interface](https://darthcloud.github.io/samples/web-bluetooth/blueretro.html)
* [Preset configuration web interface](https://darthcloud.github.io/samples/web-bluetooth/blueretro_presets.html)
* [Wireless traces repository](https://github.com/darthcloud/bt_traces)
