# Yume Nikki Fan Game for NES
Experimenting with the NES platform and simultaneously trying to create something playable.

Check it out online: [https://abandonedterminal.com/nesdev/](https://abandonedterminal.com/nesdev/index.html)

![Screenshot](https://github.com/smugd/nes-game/raw/master/prototype_1.png "Screenshot")

## How to build
[Download CC65 toolchain](https://cc65.github.io/)
```
ca65 boot.s -t nes
cc65 main.c -t nes -T -O -Oi -Or -Cl
ca65 main.s -t nes
cl65 boot.o main.o -o cartridge.nes -t nes
```
