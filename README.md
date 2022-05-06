# BlueRetro
![CI](https://github.com/darthcloud/BlueRetro/workflows/CI/badge.svg)

<img align="right" src="https://cdn.hackaday.io/images/5521711598645122765.jpg" width="400"/>
<p style='text-align: justify;'>BlueRetro is a multiplayer Bluetooth controllers adapter for various retro game consoles. Lost or broken controllers? Reproduction too expensive? Need those rare and obscure accessories? Just use the Bluetooth devices you already got! The project is open source hardware & software under the CERN-OHL-P-2.0 & Apache-2.0 licenses respectively. It's built for the popular ESP32 chip. All processing for Bluetooth and HID input decoding is done on the first core which makes it easy for other projects to use the Bluetooth stack within their own project by using the 2nd core. Wii, Switch, PS3, PS4, PS5, Xbox One, Xbox Series X|S & generic HID Bluetooth (BR/EDR & LE) devices are supported. Parallel 1P (NeoGeo, Supergun, JAMMA, etc), Parallel 2P (Atari 2600/7800, Master System, etc), NES, PCE / TG16, Mega Drive / Genesis, SNES, CD-i, 3DO, Jaguar, Saturn, PSX, PC-FX, JVS (Arcade), Virtual Boy, N64, Dreamcast, PS2 & GameCube are supported with simultaneous 4+ players using a single adapter.</p>

## DIY build instructions
* [DIY ESP32 module flashing & wiring instructions](https://github.com/darthcloud/BlueRetro/wiki/BlueRetro-DIY-Build-Instructions)
* [Cables building instructions](https://github.com/darthcloud/BlueRetro/wiki/BlueRetro-Cables-Build-Instructions)

## Need help?
* [Open a GitHub discussion](https://github.com/darthcloud/BlueRetro/discussions)

## Project documentation
* [Demo on my Youtube channel](https://www.youtube.com/channel/UC9uPsTgDhUFKuS-9zxoLi9w/videos)
* [User manual](https://github.com/darthcloud/BlueRetro/wiki)
* [Roadmap](https://docs.google.com/spreadsheets/d/e/2PACX-1vTR9HpZM9DBp986BL8aVeUu7-rP161CXUoBpRx1uX2eSsB6fmjHF_v4mPWj_SDjaliEh6Rq6c2BL1qk/pubhtml)
* [Web-Bluetooth (BLE) configuration interface Documentation](https://hackaday.io/project/170365-blueretro/log/180020-web-bluetooth-ble-configuration-interface)
* [Web-Bluetooth (BLE) configuration interface Page](https://blueretro.io)
* [BlueRetro Legacy (pre-v1.3) mapping label reference](https://docs.google.com/spreadsheets/d/e/2PACX-1vRln_dhkahEIhq4FQY_p461r5qvLn-Hkl89ZtfyIOGAqdnPtQZ5Ihfsjvd94fRbaHX8wU3F-r2ODYbM/pubhtml)
* [BlueRetro v1.4+ mapping label reference](https://docs.google.com/spreadsheets/d/e/2PACX-1vT9rPK2__komCjELFpf0UYz0cMWwvhAXgAU7C9nnwtgEaivjsh0q0xeCEiZAMA-paMrneePV7IqdX48/pubhtml)
* [Development environment setup & building instruction](https://github.com/darthcloud/BlueRetroRoot)
* [Hardware files repository](https://github.com/darthcloud/BlueRetroHW)

## Commercial solution sponsoring BlueRetro FW development
Buying these commercial adapters help the continued development of the BlueRetro firmware!!\
Thanks to all sponsors!!

&emsp;&emsp;[<img src="https://www.willsconsolemodifications.co.uk/image/catalog/Logo.png" width="400"/>](https://www.willsconsolemodifications.co.uk/index.php?route=product/product&path=59_67&product_id=52)
* PSUnoRetro by [WillConsole](https://twitter.com/WillConsole): PSX & PS2 single port dongle.\
  https://www.willsconsolemodifications.co.uk/index.php?route=product/product&path=59_67&product_id=52

  [<img src="https://www.humblebazooka.com/images/HB_2021_white_rainbow_1200px.png" width="400"/>](https://www.humblebazooka.com)
* JagBT by [Humble Bazooka](https://twitter.com/humblebazooka): Atari Jaguar single port dongle.\
  https://www.humblebazooka.com/product/atari-jaguar-bluetooth-adapter/
* Neo Geo BT by [Humble Bazooka](https://twitter.com/humblebazooka): SNK Neo Geo single port dongle.\
  https://www.humblebazooka.com/product/neo-geo-bt-bluetooth-adapter/

## Community Contribution
* BlueRetro PS1/2 Receiver by [mi213 ](https://twitter.com/mi213ger): 3D printed case & PCB for building DIY PS1/2 dongle.\
  https://github.com/Micha213/BlueRetro-PS1-2-Receiver
* N64 BlueRetro Mount by [reventlow64](https://twitter.com/reventlow): 3d printed mount for ESP32-DevkitC for N64.\
  https://www.prusaprinters.org/prints/90275-nintendo-64-blueretro-bluetooth-receiver-mount
* BlueRetro Adapter Case by [Sigismond0](https://twitter.com/Sigismond0): 3d printed case for ESP32-DevkitC.\
  https://www.prusaprinters.org/prints/116729-blueretro-bluetooth-controller-adapter-case
* BlueRetro AIO by [pmgducati](https://github.com/pmgducati): BlueRetro Through-hole base and cable PCBs.\
  https://github.com/pmgducati/Blue-Retro-AIO-Units

## Others commercial solution using BlueRetro FW
* N64 Bluetooth Controller Receiver by [bixxewoscht](https://twitter.com/bixxewoscht): N64 single port dongle.\
  https://www.tindie.com/products/retrotime/nintendo-64-bluetooth-controller-receiver/
* BlueRetro adaptador universal by [GamesCare](https://twitter.com/MichelinFabio): BlueRetro universal base & cable.\
  https://gamescare.com.br/produto/blueretro-adaptador-universal/

## General documentation
* [ESP32 RTOS + Bare Metal: Best of Both Worlds?](https://hackaday.io/project/170365/log/189836-esp32-rtos-bare-metal-best-of-both-worlds)
* [Learning Bluetooth Classic (BR/EDR) with HCI traces](https://hackaday.io/project/170365-blueretro/log/178249-learning-bluetooth-classic-bredr-with-hci-traces)
* [Xbox One Adaptive controller](https://hackaday.io/project/170365-blueretro/log/179869-xbox-one-adaptive-controller)
* [Evolution of SEGA's IO Interface from SG-1000 to Saturn](https://hackaday.io/project/170365-blueretro/log/180790-evolution-of-segas-io-interface-from-sg-1000-to-saturn)
* [Famicom & NES controller shift register: Parallel-in, Serial-out](https://hackaday.io/project/170365-blueretro/log/181368-famicom-nes-controller-shift-register-parallel-in-serial-out)
* [SNES 2P & Super Multitap](https://hackaday.io/project/170365-blueretro/log/181686-2020-08-04-progress-update-sfcsnes-support)
* [PlayStation & PlayStation 2 SPI interface](https://hackaday.io/project/170365-blueretro/log/186471-playstation-playstation-2-spi-interface)

<br><p align="center"><img src="https://cdn.hackaday.io/images/4560691598833898038.png" height="200"/></p>
