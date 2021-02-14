# Cassandra
Experimenting with the NES platform and simultaneously trying to create something playable. Big inspiration by Yume Nikki.

This is the story of a girl who, as a member of a destructive cult, brought herself to a state of delirium. You has to find some memorable objects that still connect some part of the girl's consciousness with her past.

Play it online: [https://oeoe.moe/nesdev/](https://oeoe.moe/nesdev/)

![Screenshot](https://github.com/smugd/nes-game/raw/master/prototype_1.png "Screenshot")

## Build instructions
#### Requirements
- [Download CC65 toolchain](https://cc65.github.io/)
#### Compile commands
```
cc65 main.c -t nes -T -O -Oi -Or -Cl
ca65 main.s -t nes
ca65 boot.s -t nes
cl65 boot.o main.o -t nes -o cartridge.nes
```
