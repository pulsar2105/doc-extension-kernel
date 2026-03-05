# Documentation périphérique audio

## Lister les périphérique disponible

On peut lister les périphériques audio disponible sur la machine avec la commande `qemu-system-riscv64 -audio driver=help`.
Dans mon cas j'ai par exemple :  
`Available audio drivers:
none
alsa
dbus
jack
oss
pa
pipewire
sdl
spice
wav`

D'après mes recherche le drivers ALSA qui est utiliser sous linux me semble le plus accessible et on va l'ajouter en PCI.
Pour ajouter le périphériques audio on doit ajouter le device audio comme ce-ci :  
`-audiodev alsa,id=my_audiodev \
-device virtio-sound-pci,audiodev=my_audiodev,addr=0x90000000`

A noter qu'il ne faut pas d'espace après les virgules.

En précisant ainsi :

- le type de driver utilisé : `alsa`
- l'id du phériphérique : `id)my_audiodev`
- le type de périphérique : `virtio-sound-pci`
- le nom du périphérique : `my_audiodev`

Ces régles sont précisées dans `QEMUOPTSPRUNGS` dans le makefile. Ainsi on peut exécuter qemu en mode audio et video avec `make rungs`.

## PCI

En ajoutant ces options on ajoute un périphérique audio pci.  
Pour voir où il se trouve on exécute `make rung` et en suite `Ctrl-A + Ctrl-C` pour rentrer dans la console QEMU.
Ensuite on entre `info pci` et peut voir tous les périphériques PCI présent avec leur information réspéctive.  
Dans mon cas :  
`(qemu) info pci
  Bus  0, device   0, function 0:
    Host bridge: PCI device 1b36:0008
      PCI subsystem 1af4:1100
      id ""
  Bus  0, device   1, function 0:
    Display controller: PCI device 1234:1111
      PCI subsystem 1af4:1100
      BAR0: 32 bit prefetchable memory at 0x50000000 [0x50ffffff].
      BAR2: 32 bit memory at 0x40000000 [0x40000fff].
      BAR6: 32 bit memory at 0xffffffffffffffff [0x00007ffe].
      id ""
  Bus  0, device   2, function 0:
    Audio controller: PCI device 1af4:1059
      PCI subsystem 1af4:1100
      IRQ 0, pin A
      BAR1: 32 bit memory at 0xffffffffffffffff [0x00000ffe].
      BAR4: 64 bit prefetchable memory at 0xffffffffffffffff [0x00003ffe].
      id ""`

## Biblio

https://www.qemu.org/docs/master/system/devices/virtio/virtio-snd.html
