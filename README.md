# BlueRetro
![BlueRetro](https://cdn.hackaday.io/images/5521711598645122765.jpg)

BlueRetro is a multiplayer Bluetooth controllers adapter for various retro game consoles.

![BlueRetro](https://cdn.hackaday.io/images/7104601597769319104.png)
BlueRetro is a multiplayer Bluetooth controllers adapter for various retro game consoles. Lost or broken controllers? Reproduction too expensive? Need those rare and obscure accessories? Just use the Bluetooth devices you already got! The project is open source hardware & software under the CERN-OHL-P-2.0 & Apache-2.0 licenses respectively. It's built for the popular ESP32 chip. All processing for Bluetooth and HID input decoding is done on the first core which makes it easy for other projects to use the Bluetooth stack within their own project by using the 2nd core. Wii, Switch, PS3, PS4, PS5, Xbox One & generic HID Bluetooth devices are supported. Parallel 1P (NeoGeo, Supergun, JAMMA, etc), Parallel 2P (Atari 2600, Master System, etc), NES, Mega Drive / Genesis, SNES, Saturn, JVS (Arcade), PSX, N64, Dreamcast, PS2 & GameCube are supported with simultaneous 4+ players using a single adapter. Soon PCE / TG16, CD-i, 3DO, PC-FX...\

## Project documentation
* [Main hackaday.io page](https://hackaday.io/project/170365-blueretro)
* [User manual](https://github.com/darthcloud/BlueRetro/wiki)
* [Web-Bluetooth (BLE) configuration interface](https://hackaday.io/project/170365-blueretro/log/180020-web-bluetooth-ble-configuration-interface)
* [Hardware Design](https://hackaday.io/project/170365-blueretro/log/182054-blueretro-hardware-design)
* [Software Design](https://hackaday.io/project/170365-blueretro/log/182603-blueretro-software-design)
* [Roadmap](https://docs.google.com/spreadsheets/d/e/2PACX-1vTR9HpZM9DBp986BL8aVeUu7-rP161CXUoBpRx1uX2eSsB6fmjHF_v4mPWj_SDjaliEh6Rq6c2BL1qk/pubhtml)
* [DevKit PCB assembly checklist](https://hackaday.io/project/170365-blueretro/log/183903-pcb-assembly-checklist)
* [Input and output specifications](https://hackaday.io/project/170365-blueretro/log/184162-input-and-output-specifications)
* [Testing BlueRetro](https://hackaday.io/project/170365-blueretro/log/184300-testing-blueretro)

## General documentation
* [Learning Bluetooth Classic (BR/EDR) with HCI traces](https://hackaday.io/project/170365-blueretro/log/178249-learning-bluetooth-classic-bredr-with-hci-traces)
* [Xbox One Adaptive controller](https://hackaday.io/project/170365-blueretro/log/179869-xbox-one-adaptive-controller)
* [Evolution of SEGA's IO Interface from SG-1000 to Saturn](https://hackaday.io/project/170365-blueretro/log/180790-evolution-of-segas-io-interface-from-sg-1000-to-saturn)
* [Famicom & NES controller shift register: Parallel-in, Serial-out](https://hackaday.io/project/170365-blueretro/log/181368-famicom-nes-controller-shift-register-parallel-in-serial-out)
* [SNES 2P & Super Multitap](https://hackaday.io/project/170365-blueretro/log/181686-2020-08-04-progress-update-sfcsnes-support)

## Other links
* [Software files repository](https://github.com/darthcloud/BlueRetro)
* [BlueRetro mapping label reference](https://docs.google.com/spreadsheets/d/e/2PACX-1vRln_dhkahEIhq4FQY_p461r5qvLn-Hkl89ZtfyIOGAqdnPtQZ5Ihfsjvd94fRbaHX8wU3F-r2ODYbM/pubhtml)
* [Development environment setup & building instruction](https://github.com/darthcloud/BlueRetroRoot)
* [DIY building instruction](https://github.com/darthcloud/BlueRetro/wiki/BlueRetro-DIY-Build-Instructions)
* [Cables building instruction](https://github.com/darthcloud/BlueRetro/wiki/BlueRetro-Cables-Build-Instructions)
* [Hardware files repository](https://github.com/darthcloud/BlueRetroHW)
* [Advance configuration web interface](https://blueretro.io/blueretro.html)
* [Preset configuration web interface](https://blueretro.io/blueretro_presets.html)
* [Wireless traces repository](https://github.com/darthcloud/bt_traces)


