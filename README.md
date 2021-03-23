# BlueRetro
![CI](https://github.com/darthcloud/BlueRetro/workflows/CI/badge.svg)

BlueRetro is a multiplayer Bluetooth controllers adapter for various retro game consoles. Lost or broken controllers? Reproduction too expensive? Need those rare and obscure accessories? Just use the Bluetooth devices you already got! The project is open source hardware & software under the CERN-OHL-P-2.0 & Apache-2.0 licenses respectively. It's built for the popular ESP32 chip. All processing for Bluetooth and HID input decoding is done on the first core which makes it easy for other projects to use the Bluetooth stack within their own project by using the 2nd core. Wii, Switch, PS3, PS4, PS5, Xbox One & generic HID Bluetooth devices are supported. Parallel 1P (NeoGeo, Supergun, JAMMA, etc), Parallel 2P (Atari 2600/7800, Master System, etc), NES, Mega Drive / Genesis, SNES, 3DO, Saturn, PSX, PC-FX, JVS (Arcade), N64, Dreamcast, PS2 & GameCube are supported with simultaneous 4+ players using a single adapter. Soon PCE / TG16, CD-i...

<p align="center"><img src="https://cdn.hackaday.io/images/5521711598645122765.jpg" width="600"/></p>

## DIY build instructions
* [DIY ESP32 module flashing & wiring instructions](https://github.com/darthcloud/BlueRetro/wiki/BlueRetro-DIY-Build-Instructions)
* [Cables building instructions](https://github.com/darthcloud/BlueRetro/wiki/BlueRetro-Cables-Build-Instructions)

## Project documentation
* [Main hackaday.io page](https://hackaday.io/project/170365-blueretro)
* [User manual](https://github.com/darthcloud/BlueRetro/wiki)
* [Roadmap](https://docs.google.com/spreadsheets/d/e/2PACX-1vTR9HpZM9DBp986BL8aVeUu7-rP161CXUoBpRx1uX2eSsB6fmjHF_v4mPWj_SDjaliEh6Rq6c2BL1qk/pubhtml)
* [Web-Bluetooth (BLE) configuration interface Documentation](https://hackaday.io/project/170365-blueretro/log/180020-web-bluetooth-ble-configuration-interface)
* [Web-Bluetooth (BLE) configuration interface Page](https://blueretro.io)
* [BlueRetro mapping label reference](https://docs.google.com/spreadsheets/d/e/2PACX-1vRln_dhkahEIhq4FQY_p461r5qvLn-Hkl89ZtfyIOGAqdnPtQZ5Ihfsjvd94fRbaHX8wU3F-r2ODYbM/pubhtml)
* [Cables building instructions](https://github.com/darthcloud/BlueRetro/wiki/BlueRetro-Cables-Build-Instructions)
* [Development environment setup & building instruction](https://github.com/darthcloud/BlueRetroRoot)
* [Hardware files repository](https://github.com/darthcloud/BlueRetroHW)

<p align="center"><img src="https://cdn.hackaday.io/images/7104601597769319104.png" width="600"/></p>

## General documentation
* [ESP32 RTOS + Bare Metal: Best of Both Worlds?](https://hackaday.io/project/170365/log/189836-esp32-rtos-bare-metal-best-of-both-worlds)
* [Learning Bluetooth Classic (BR/EDR) with HCI traces](https://hackaday.io/project/170365-blueretro/log/178249-learning-bluetooth-classic-bredr-with-hci-traces)
* [Xbox One Adaptive controller](https://hackaday.io/project/170365-blueretro/log/179869-xbox-one-adaptive-controller)
* [Evolution of SEGA's IO Interface from SG-1000 to Saturn](https://hackaday.io/project/170365-blueretro/log/180790-evolution-of-segas-io-interface-from-sg-1000-to-saturn)
* [Famicom & NES controller shift register: Parallel-in, Serial-out](https://hackaday.io/project/170365-blueretro/log/181368-famicom-nes-controller-shift-register-parallel-in-serial-out)
* [SNES 2P & Super Multitap](https://hackaday.io/project/170365-blueretro/log/181686-2020-08-04-progress-update-sfcsnes-support)
* [PlayStation & PlayStation 2 SPI interface](https://hackaday.io/project/170365-blueretro/log/186471-playstation-playstation-2-spi-interface)

<br><p align="center"><img src="https://cdn.hackaday.io/images/5943961602443215535.png" height="200"/><img src="https://cdn.hackaday.io/images/4560691598833898038.png" height="200"/></p>
