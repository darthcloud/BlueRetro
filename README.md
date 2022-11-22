# BlueRetro
![CI](https://github.com/darthcloud/BlueRetro/workflows/CI/badge.svg)

<img src="/static/PNGs/BRE_Logo_Color_Outline.png" width="800"/>
<br>
<img align="right" src="https://cdn.hackaday.io/images/5521711598645122765.jpg" width="400"/>
<p style='text-align: justify;'>BlueRetro is a multiplayer Bluetooth controllers adapter for various retro game consoles. Lost or broken controllers? Reproduction too expensive? Need those rare and obscure accessories? Just use the Bluetooth devices you already got! The project is open source hardware & software under the CERN-OHL-P-2.0 & Apache-2.0 licenses respectively. It's built for the popular ESP32 chip. All processing for Bluetooth and HID input decoding is done on the first core which makes it easy for other projects to use the Bluetooth stack within their own project by using the 2nd core. Wii, Switch, PS3, PS4, PS5, Xbox One, Xbox Series X|S & generic HID Bluetooth (BR/EDR & LE) devices are supported. Parallel 1P (NeoGeo, Supergun, JAMMA, Handheld, etc), Parallel 2P (Atari 2600/7800, Master System, etc), NES, PCE / TG16, Mega Drive / Genesis, SNES, CD-i, 3DO, Jaguar, Saturn, PSX, PC-FX, JVS (Arcade), Virtual Boy, N64, Dreamcast, PS2, GameCube & Wii extension are supported with simultaneous 4+ players using a single adapter.</p>

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
* PCE BT by [Humble Bazooka](https://twitter.com/humblebazooka): NEC PC-Engine dongle.\
  https://www.humblebazooka.com/product/pce-bt-pc-engine-bluetooth-adapter/
* Turbo BT by [Humble Bazooka](https://twitter.com/humblebazooka): NEC TurboGrafx-16 dongle.\
  https://www.humblebazooka.com/product/turbo-bt-turbografx-16-bluetooth-adapter/
  
  [<img src="https://i.imgur.com/qsCLN9R.png" width="400"/>](https://www.tindie.com/stores/retrotime/)
* N64 Bluetooth Controller Receiver by [bixxewoscht](https://twitter.com/bixxewoscht): N64 single port dongle.\
  https://8bitmods.com/n64-blueretro-bt-controller-receiver-with-memory-pak-original-grey/
  
  [<img src="https://static.wixstatic.com/media/ce8a57_fe2ccaa817704811aefee7a0bbd74098~mv2.png/v1/fill/w_445,h_60,al_c,q_85,enc_auto/RetroOnyx_logo_primary_grey_RGB60px.png" width="400"/>](https://www.retroonyx.com/)
* Virtual Boy BlueRetro Adapter by [RetroOnyx](https://twitter.com/mellott124): Virtual Boy dongle.\
  https://www.retroonyx.com/product-page/virtual-boy-blueretro-adapter
  
  [<img src="http://www.retroscaler.com/wp-content/uploads/2022/03/logo.png" width="400"/>](https://www.aliexpress.com/item/1005004114458491.html)
* RetroScaler BlueRetro Wireless Game Controllers Converter Adapter For PS1 PS2 by [RetroScaler](https://twitter.com/RetroScaler): PS1 & PS2 single port dongle.\
  https://www.aliexpress.com/item/1005004114458491.html
* RetroScaler BlueRetro Motor Wireless Game Controllers Adapter For Nintendo GameCube by [RetroScaler](https://twitter.com/RetroScaler): GameCube single port dongle.\
  https://www.aliexpress.us/item/1005004917765807.html
  
  [<img src="https://cdn.shopify.com/s/files/1/0550/1855/3556/files/mikegoble2ND_Logo_05_R1A_5f70d3e5-90bb-4e08-a4bc-1e91a60bd833.png" width="400"/>](https://www.laserbear.net/)
* GameCube Blue Retro Internal Adapter by [Laser Bear Industries](https://twitter.com/collingall): GameCube controller PCB replacement with integrated BlueRetro.\
  https://www.laserbear.net/products/gamecube-blue-retro-internal-adapter \
  https://8bitmods.com/internal-bluetooth-blueretro-adapter-for-gamecube/
  
  [<img src="https://pbs.twimg.com/profile_banners/1219272912999473153/1637604861" width="400"/>](https://www.tindie.com/stores/grechtech/)
* RetroRosetta by [GrechTech](https://twitter.com/GrechTech): BlueRetro universal core and cables.\
  https://www.tindie.com/stores/grechtech/

## Community Contribution
* BlueRetro PS1/2 Receiver by [mi213 ](https://twitter.com/mi213ger): 3D printed case & PCB for building DIY PS1/2 dongle.\
  https://github.com/Micha213/BlueRetro-PS1-2-Receiver
* N64 BlueRetro Mount by [reventlow64](https://twitter.com/reventlow): 3d printed mount for ESP32-DevkitC for N64.\
  https://www.prusaprinters.org/prints/90275-nintendo-64-blueretro-bluetooth-receiver-mount
* BlueRetro Adapter Case by [Sigismond0](https://twitter.com/Sigismond0): 3d printed case for ESP32-DevkitC.\
  https://www.prusaprinters.org/prints/116729-blueretro-bluetooth-controller-adapter-case
* BlueRetro AIO by [pmgducati](https://github.com/pmgducati): BlueRetro Through-hole base and cable PCBs.\
  https://github.com/pmgducati/Blue-Retro-AIO-Units

## General documentation
* [ESP32 RTOS + Bare Metal: Best of Both Worlds?](https://hackaday.io/project/170365/log/189836-esp32-rtos-bare-metal-best-of-both-worlds)
* [Learning Bluetooth Classic (BR/EDR) with HCI traces](https://hackaday.io/project/170365-blueretro/log/178249-learning-bluetooth-classic-bredr-with-hci-traces)
* [Xbox One Adaptive controller](https://hackaday.io/project/170365-blueretro/log/179869-xbox-one-adaptive-controller)
* [Evolution of SEGA's IO Interface from SG-1000 to Saturn](https://hackaday.io/project/170365-blueretro/log/180790-evolution-of-segas-io-interface-from-sg-1000-to-saturn)
* [Famicom & NES controller shift register: Parallel-in, Serial-out](https://hackaday.io/project/170365-blueretro/log/181368-famicom-nes-controller-shift-register-parallel-in-serial-out)
* [SNES 2P & Super Multitap](https://hackaday.io/project/170365-blueretro/log/181686-2020-08-04-progress-update-sfcsnes-support)
* [PlayStation & PlayStation 2 SPI interface](https://hackaday.io/project/170365-blueretro/log/186471-playstation-playstation-2-spi-interface)

<br><p align="center"><img src="https://cdn.hackaday.io/images/4560691598833898038.png" height="200"/></p>
