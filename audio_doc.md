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

## Recherche du device PCI

Ce qui nous permet de déduire que le `vendor_id` et le `device_id` de la carte audio sont respectivement `0x1af4` et `0x1059`.
Nous allons nous servir de ça pour rechercher l'adresse de configuration du périphérique PCI comme précédement avec la carte graphique.

Le `device_id` est `0x1059` car selon la doc Oasis section 4.1.2.1 et 5 le `device_id` est calculé en ajouter à `0x1040` l'id du type device.
Ici le device est `Sound device` avec l'id 25 (base 10) et 0x1040 + 0x19 = 0x1059.

## Configuration PCI

D'après la page wikipédia qui est basé sur un article d'un blog de dev OS (cf biblio) pour configurer le device PCI nous devons indiquer plusieurs chose :

- les autorisations dans le registres `command`, ici 0b111 => bit 0 = 1 : autoriser les entrées sorties, bit 1 = 1 : autorise l'accès de notre part à la mémoire,
  bit 2 = 1 : autorise l'accès du device à la mémoire qui lui est attribué.
- l'adresse mémoire pour la configuration
- l'adresse mémoire pour l'utilisation

Pour indiquer les adresses mémoires d'après les informations données par `info qtree` dans la console QEMU il faut :

- placer la première valeur `BAR 1` à l'adresse `PCI_AUDIO + 0x14`
- placer la seconde valeur `BAR 4` à l'adresse `PCI_ADUIO + 0x20`

Le tout est analogue à la première configuration de la carte graphique et doit impérativement respecter l'alignement de 4 octet de la zone de spécification.  
On doit ainsi obtenir dans la console QEMU :  
`class Audio controller, addr 00:02.0, pci id 1af4:1059 (sub 1af4:1100)
bar 1: mem at 0x70000000 [0x70000fff]
bar 4: mem at 0x60000000 [0x60003fff]
bus: virtio-bus
type virtio-pci-bus
dev: virtio-sound-device, id ""`

J'ai choisie les adresses de manières arbitraire.

On doit aussi configurer l'IRQ pour pouvoir obtenir des interruptions propre par la suite.  
Ainsi on doit modifier le registre `Interrupt Line` par une valeur prédéterminé à l'avance, ici `33`.
Cette valeur d'après le code source de QEMU doit être entre 32 et 35 (cf Biblio).

## Biblio

Configuration de QEMU :  
https://www.qemu.org/docs/master/system/devices/virtio/virtio-snd.html

Documentation générale des virtio :  
https://docs.oasis-open.org/virtio/virtio/v1.3/csd01/virtio-v1.3-csd01.pdf

Pour la configue PCI :  
https://en.wikipedia.org/wiki/PCI_configuration_space#Standardized_registers
https://wiki.osdev.org/PCI#Command_Register
