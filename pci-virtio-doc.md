# Documentation périphérique audio

## Conventions Oasis

### Types

u8, u16, u32, u64  
 Un entier non signé de taille en bits spécifique.  
le16, le32, le64  
 Un entier non signé de taille en bits spécifique, format little-endian.  
be16, be32, be64  
 Un entier non signé de taille en bits spécifique, format big-endian.

## Lister les périphérique disponible

On peut lister les périphériques audio disponible sur la machine avec la commande `qemu-system-riscv64 -audio driver=help`.  
Dans mon cas j'ai par exemple :

```
Available audio drivers:
none
alsa
dbus
jack
oss
pa
pipewire
sdl
spice
wav
```

D'après mes recherche le drivers ALSA qui est utiliser sous linux me semble le plus accessible et on va l'ajouter en PCI.
Pour ajouter le périphériques audio on doit ajouter le device audio comme ce-ci :

```
-audiodev alsa,id=my_audiodev \
-device virtio-sound-pci,audiodev=my_audiodev,addr=0x90000000
```

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

```
(qemu) info pci
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
      id ""
```

## Recherche du device PCI

Ce qui nous permet de déduire que le `vendor_id` et le `device_id` de la carte audio sont respectivement `0x1af4` et `0x1059`.
Nous allons nous servir de ça pour rechercher l'adresse de configuration du périphérique PCI comme précédement avec la carte graphique.

Le `device_id` est `0x1059` car le `device_id` est calculé en ajouter à `0x1040` l'id du type device et le `vendor_id` est `0x1af4` car c'est un périphérique virtio (sec 4.1.2¹ et 5¹).  
Ici le device est `Sound device` avec l'id 25 (base 10) et 0x1040 + 0x19 = 0x1059.

## Configuration PCI

D'après un article de blog de dev OS (cf 3) le type de header PCI dépend du device utilisé. Ici pour le device son le header est du type `0x0`, car info pci indique `BAR1`/`BAR4`.

Pour pouvoir configurer ce périiphérique il faut spécifier certaines chose :

### Command

- les autorisations dans le registres `command`, ici 0b111 => bit 0 = 1 : autoriser les entrées sorties, bit 1 = 1 : autorise l'accès de notre part à la mémoire,
  bit 2 = 1 : autorise l'accès du device à la mémoire qui lui est attribué.
- l'adresse mémoire pour la configuration avec registres, `BAR1` car non-prefetchable.
- l'adresse mémoire pour l'utilisation, `BAR2` car prefetchable.

### Bases address

Pour indiquer les adresses mémoires d'après les informations données par `info qtree` dans la console QEMU il faut :

- placer la première valeur `BAR1` à l'adresse `PCI_AUDIO + 0x14`
- placer la seconde valeur `BAR4` à l'adresse `PCI_ADUIO + 0x20`

Le tout est analogue à la première configuration de la carte graphique et doit impérativement respecter l'alignement de 4 octet de la zone de spécification.
On doit ainsi obtenir dans la console QEMU :

```
class Audio controller, addr 00:02.0, pci id 1af4:1059 (sub 1af4:1100)
bar 1: mem at 0x60000000 [0x60000fff]
bar 4: mem at 0x61000000 [0x61003fff]
bus: virtio-bus
type virtio-pci-bus
```

J'ai choisie les adresses de manières arbitraire mais en gardant en tête que la partie configuration aura besoin généralement de moins de mémoire.

### IRQ

On doit aussi configurer l'IRQ pour pouvoir obtenir des interruptions propre par la suite.
Ainsi on doit modifier le registre `Interrupt Line` par une valeur prédéterminé à l'avance, ici `33`.
Cette valeur d'après le code source de QEMU doit être entre 32 et 35 (cf Biblio).

### Capabilities Pointer

Le `Capabilities Pointer` situé à un offset de `0x34` dans le header PCI est un pointer vers l'espace de configuration du device PCI.  
Cet espace de configuration est une liste de capabilities où chaque élément à la structure suivante (sec 4.1.4, biblio 1) :

```
struct virtio_pci_cap {
    u8 cap_vndr;    /* Generic PCI field: PCI_CAP_ID_VNDR */
    u8 cap_next;    /* Generic PCI field: next ptr. */
    u8 cap_len;     /* Generic PCI field: capability length */
    u8 cfg_type;    /* Identifies the structure. */
    u8 bar;         /* Where to find it. */
    u8 id;          /* Multiple capabilities of the same type */
    u8 padding[2];  /* Pad to full dword. */
    le32 offset;    /* Offset within bar. */
    le32 length;    /* Length of the structure, in bytes. */
};
```

Dans la suite nous allons devoir trouver l'élément de la liste dont `cap_vndr` est `0x09` (sec 4.1.4, biblio 1).  
Ainsi nous pourrons accéder à l'espace de configuration du device qui se trouve dans l'espace mémoire `bar` + `offset`.

ATTENTION : on ne peut l'utiliser que si le bit 4 du registre `Status` est à 1.

## VIRTIO

### Initilisation

#### Adresse de configuration

Pour pouvoir utiliser un périphérique Virtio nous devons premièrement l'initiliser (sec 3.1 biblio 1).  
Pour ce faire nous devons accéder à l'adresse de configuration du périphérique et nous allons utiliser les renseignements données par le `Capabilities Pointer`.
En effet, il suffit de :

- parcourir la liste chainée de capabilities et cherche l'élément où `cfg_type` est `VIRTIO_PCI_CAP_COMMON_CFG` (sec 4.1.4 biblio 1).
- récupérer l'adresse de base `bar` et l'`offset` associé à cette adresse
- masquer le permier octet de `bar` qui indique le type de mémoire (https://wiki.osdev.org/PCI#Base_Address_Registers)
- calculer l'adresse (`bar` masqué) + `offset`

Un fois configurer nous allons nous servir de la structure suivante pour accéder au paramètre et initiliser l'audio (sec 4.1.4.3, biblio 1) :

```
struct virtio_pci_common_cfg {
    /* About the whole device. */
    le32 device_feature_select; /* read-write */
    le32 device_feature;        /* read-only for driver */
    le32 driver_feature_select; /* read-write */
    le32 driver_feature;        /* read-write */
    le16 config_msix_vector;    /* read-write */
    le16 num_queues;            /* read-only for driver */
    u8 device_status;           /* read-write */
    u8 config_generation;       /* read-only for driver */

    /* About a specific virtqueue. */
    le16 queue_select;          /* read-write */
    le16 queue_size;            /* read-write */
    le16 queue_msix_vector;     /* read-write */
    le16 queue_enable;          /* read-write */
    le16 queue_notify_off;      /* read-only for driver */
    le64 queue_desc;            /* read-write */
    le64 queue_driver;          /* read-write */
    le64 queue_device;          /* read-write */
    le16 queue_notify_data;     /* read-only for driver */
    le16 queue_reset;           /* read-write */
};
```

Valeur utile pour la suite :

```
#define VIRTIO_STATUS_ACKNOWLEDGE 1
#define VIRTIO_STATUS_DRIVER 2
#define VIRTIO_STATUS_DRIVER_OK 4
#define VIRTIO_STATUS_FEATURES_OK 8
#define VIRTIO_STATUS_FAILED 128
```

L'initilisation consiste à "prévenir" le device qu'on va l'utliser, en 8 étapes (spec 3.1 biblio 1) :

- 1: Reset du device
- 2: Activer le bit status `ACKNOWLEDGE`: l'OS a vu le device, "coucou"
- 3: Activer le bit `DRIVER` : l'OS sait comment utiliser le device
- 4: Lire les bits de fonctionnalité and écrire les fonctionnalité reconnus par l'OS et le driver. Il est recommander de lire avant les fonctionnalité supporté dans l'espace de configuration spécifique du device.
- 5: Activer le bit de `FEATURE_OK`. Et le driver ne doit pas accepter de nouveau bits de fonctionnalité après cela.
- 6: Re-lire le `device status` pour vérifier que le bit `FEATURE_OK` est toujours à 1: autrement le device ne supporte pas les fonctionnalités demandé et n'est pas utilisable.
- 7: Effectuer la configuration spécifique au device, y compris la découverte des files d'attente virtuelles, virtqueues, pour le device, la configuration facultative par bus, la lecture et éventuellement l'écriture de l'espace de configuration virtio du device, et le remplissage des files d'attente virtuelles.
- 8: Activer le bit d'état DRIVER_OK. À ce stade, le périphérique est "actif".

Chaque étape demande ça propre partie.

## Biblio

Documentation générale des virtio :
1: https://docs.oasis-open.org/virtio/virtio/v1.2/csd01/virtio-v1.2-csd01.pdf

Configuration de QEMU :
2: https://www.qemu.org/docs/master/system/devices/virtio/virtio-snd.html

Pour la configue PCI :
3: https://wiki.osdev.org/PCI
