# Documentation périphériques inputs

## Conventions Oasis

### Types

u8, u16, u32, u64
Un entier non signé de taille en bits spécifique.
le16, le32, le64
Un entier non signé de taille en bits spécifique, format little-endian.
be16, be32, be64
Un entier non signé de taille en bits spécifique, format big-endian.

## Lister les périphérique disponible

On peut lister les périphériques clavier/souris disponibles sur la machine avec la commande `qemu-system-x86_64 -device help | grep <my_device>` (remplacer par `mouse` ou `keyboard`).
Dans mon cas, j'ai par exemple pour le clavier et la souris respectivement :

```
name "virtio-keyboard-device", bus virtio-bus
name "virtio-keyboard-pci", bus PCI, alias "virtio-keyboard"
```

```
name "usb-mouse", bus usb-bus
name "virtio-mouse-device", bus virtio-bus
name "virtio-mouse-pci", bus PCI, alias "virtio-mouse"
```

Pour ajouter les périphériques inputs (clavier/souris), on doit ajouter les devices comme ceci :

```
-device virtio-keyboard-pci \
-device virtio-mouse-pci
```

Ces régles sont précisées dans le makefile dans la rêgles : `DEVICE_KEYBOARD` et `DEVICE_MOUSE`.

## PCI

## Point mémoire

ATTENTION : Comme le précise la documentation (spec 4.1.3.1 biblio 1) tous les accès mémoire doivent correspondre à la taille du champ lu et doivent être alignés.
Donc en C il est OBLIGATOIRE d'ajouter ici `__attribute__((packed, aligned(4)))` aux structures mémoire utilisées.

- `packed` interdit l'ajout de padding entre les champs de la struct. La struct est "tassée".
- `aligned(4)` demande au compilo d'aligner la structure sur 4 octet.
  En résumé : les structures sont alignées sur 4 octets et bien tassées, donc sous réserve que chaque champs des structs soit aligné correctement entre eux, tous les accès mémoires réalisés seront alignés.
  Après vérification rapide, il semble que toutes les structs Virtio sont correctement alignées.

## Info générales

En ajoutant ces options à QEMU, on vient d'ajouter les périphériques clavier et souris PCI. Pas d'USB ici, Dieu m'en garde.
Pour voir où il se trouve, on exécute `make run` et ensuite `Ctrl-A + Ctrl-C` pour rentrer dans la console QEMU.
Ensuite on entre `info pci` et peut voir tous les périphériques PCI présents avec leur information réspéctive.

Dans mon cas :

```
  Bus  0, device   0, function 0:
    Host bridge: PCI device 1b36:0008
      PCI subsystem 1af4:1100
      id ""
  Bus  0, device   1, function 0:
    Keyboard: PCI device 1af4:1052
      PCI subsystem 1af4:1100
      IRQ 33, pin A
      BAR1: 32 bit memory at 0x62000000 [0x62000fff].
      BAR4: 64 bit prefetchable memory at 0x63000000 [0x63003fff].
      id ""
  Bus  0, device   2, function 0:
    Mouse: PCI device 1af4:1052
      PCI subsystem 1af4:1100
      IRQ 34, pin A
      BAR1: 32 bit memory at 0x64000000 [0x64000fff].
      BAR4: 64 bit prefetchable memory at 0x65000000 [0x65003fff].
      id ""
```

À noter ici qu'étant donné que j'ai déjà fait le programme, tous les champs PCI affichés (`BAR0`, `IRQ`) sont déjà configurés.

## Recherche du device PCI

Ce qui nous permet de déduire que le `vendor_id` et le `device_id` des devices inputs sont respectivement `0x1af4` et `0x1052`.
Nous allons nous servir de ça pour rechercher les adresses de configurations des périphériques PCI comme précédemment avec la carte graphique.
Le `device_id` est `0x1052` car le `device_id` est calculé en ajoutant à `0x1040` l'id du type device et le `vendor_id` est `0x1af4` parce que c'est un périphérique virtio (sec 4.1.2 et 5 biblio 1).
Ici le device est `Input` avec l'id 18 (base 10) et 0x1040 + 0x12 = 0x1052.

Les plus attentifs d'entre vous auront remarqué que les deux périphériques ont exactement le même `vendor_id` et `device_id` et donc il nous est pas possible (avant un bon moment) de différencier les deux périphériques. Pour des raisons de simplicité, on peut remarquer que l'ordre d'affichage des devices PCI dans la console de QEMU ne change pas et correspond à l'ordre de découverte dans la mémoire.
On assume donc dans la suite que le premier device est le clavier et la second la souris. À vous d'adapter si l'ordre est différent chez vous.

## Configuration PCI

D'après un article de blog de dev OS (cf biblio 2) le type de header PCI dépend du device utilisé. Ici les headers sont du type `0x0`, car info pci indique `BAR1`/`BAR4` pour nos deux pépriphériques.

Pour configurer le PCI, il est fortement recommandé de mapper le header avec une struct qui à bon goût d'indiquer la structure mémoire du header.

```C
typedef struct {
    /* --- Common Header (offset 0x00 – 0x0F) --- */
    uint16_t vendor_id;      // 0x00 - 0xFFFF = invalid
    uint16_t device_id;      // 0x02
    uint16_t command;        // 0x04
    uint16_t status;         // 0x06
    uint8_t revision_id;     // 0x08
    uint8_t prog_if;         // 0x09 - Programming Interface
    uint8_t subclass;        // 0x0A
    uint8_t class_code;      // 0x0B
    uint8_t cache_line_size; // 0x0C - en unités de 32 bits
    uint8_t latency_timer;   // 0x0D
    uint8_t header_type;     // 0x0E - bit 7 = multi-function
    uint8_t bist;            // 0x0F - Built-In Self Test

    /* --- Type 0x0 Specific (offset 0x10 – 0x3F) --- */
    uint32_t bar[6]; // 0x10–0x27 - Base Address Registers

    uint32_t cardbus_cis_ptr;     // 0x28 - CardBus CIS Pointer
    uint16_t subsystem_vendor_id; // 0x2C
    uint16_t subsystem_id;        // 0x2E

    uint32_t expansion_rom_base; // 0x30

    uint8_t capabilities_ptr; // 0x34 - offset dans le config space
    uint8_t reserved0[3];     // 0x35–0x37
    uint32_t reserved1;       // 0x38

    uint8_t interrupt_line; // 0x3C - IRQ mappé par le BIOS/OS
    uint8_t interrupt_pin;  // 0x3D - 0x00 = pas d'IRQ, 0x01=INTA...
    uint8_t min_grant;      // 0x3E - burst period désiré (en 250ns)
    uint8_t max_latency;    // 0x3F - fréquence d'accès au bus voulue
} __attribute__((packed, aligned(4))) pci_header_t0x0;
```

Pour pouvoir configurer ces périphériques, il faut spécifier certaines choses dans les registres PCI :

### Registre Command

- les autorisations dans le registres `command`, ici 0b111 => bit 0 = 1 : autoriser les entrées sorties, bit 1 = 1 : autorise l'accès de notre part à la mémoire,
  bit 2 = 1 : autorise l'accès du device à la mémoire qui lui est attribué.
- l'adresse mémoire pour la configuration avec registres, `BAR1` car non-prefetchable.
- l'adresse mémoire pour l'utilisation, `BAR2` car prefetchable.

### Registres Bases address

Ce registre PCI permet d'indiquer les zones mémoire que les périphériques doivent utiliser pour leur fonctionnement.

Pour indiquer les adresses mémoires d'après les informations données par `info qtree` dans la console QEMU il faut :

- placer la première valeur dans le registre `BAR1`
- placer la seconde valeur dans le registre `BAR4`

Le tout est analogue à la première configuration de la carte graphique.
On doit ainsi obtenir dans la console QEMU à peu près le même résultat que j'ai donné au niveau des données dans les BAR.

J'ai choisi les adresses de manière arbitraire mais en gardant en tête que la zone mémoire requise n'est pas gargantuesque.
Donc, n'allez pas mettre `BAR1` à `0x600000000` et `BAR4` à `0x700000000`, c'est inutile, soyez frugale en termes de mémoire.

### Registre IRQ

On doit aussi configurer l'IRQ pour pouvoir obtenir des interruptions pour les événements des périphériques.
Ainsi on doit modifier le registre `Interrupt Line` par une valeur prédéterminée à l'avance.
Pour déterminer la valeur de l'IRQ, il faut préciser le pin d'interruption, 1 pour `INTA#` (cf biblio 3) et le numéro du device PCI.
Ainsi `IRQ = 32 + (device + pin)%4`. Pourquoi cette formule ? Après avoir demandé à une IA locale, ça a l'air de fonctionner.

### Registre Capabilities Pointer

Le `Capabilities Pointer` dans le header PCI est un pointer (dingue je sais) vers le début d'une liste chainée qui indique les espaces mémoire du device PCI.
Cet espace de configuration est une liste chaînée de capabilities où chaque élément a la structure suivante (sec 4.1.4, biblio 1) :

```C
struct virtio_pci_cap {
    u8 cap_vndr;    // Generic PCI field: PCI_CAP_ID_VNDR
    u8 cap_next;    // Generic PCI field: next ptr.
    u8 cap_len;     // Generic PCI field: capability length
    u8 cfg_type;    // Identifies the structure.
    u8 bar;         // Where to find it.
    u8 id;          // Multiple capabilities of the same type
    u8 padding[2];  // Pad to full dword.
    le32 offset;    // Offset within bar.
    le32 length;    // Length of the structure, in bytes.
};
```

Dans la suite nous allons devoir trouver l'élément de la liste dont `cap_vndr` est `0x09` (sec 4.1.4, biblio 1).
Ainsi nous pourrons accéder à l'espace de configuration du device qui se trouve dans l'espace mémoire `bar` + `offset`.

ATTENTION : on ne peut utiliser ce registre uniquement si le bit 4 du registre `Status` est à 1 (cf biblio 2).

## VIRTIO - Configuration générale

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

```C
struct virtio_pci_common_cfg {
    /* About the whole device. */
    le32 device_feature_select; // read-write
    le32 device_feature;        // read-only for driver
    le32 driver_feature_select; // read-write
    le32 driver_feature;        // read-write
    le16 config_msix_vector;    // read-write
    le16 num_queues;            // read-only for driver
    u8 device_status;           // read-write
    u8 config_generation;       // read-only for driver

    /* About a specific virtqueue. */
    le16 queue_select;          // read-write
    le16 queue_size;            // read-write
    le16 queue_msix_vector;     // read-write
    le16 queue_enable;          // read-write
    le16 queue_notify_off;      // read-only for driver
    le64 queue_desc;            // read-write
    le64 queue_driver;          // read-write
    le64 queue_device;          // read-write
    le16 queue_notify_data;     // read-only for driver
    le16 queue_reset;           // read-write
};
```

Valeur utile pour la suite :

```C
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

### Etape 1 : Reset

Pour réinitialiser le périphérique nous devons mettre à `0` le registre `device_status` et vérifier que celui-ci soit bien à `0`.
Ainsi on reset de périphérique et le périphérique est bien réinitialiser (spec 2.4 biblio 1).

### Etape 2~3 : ACKNOWLEDGE et DRIVER

Avant d'utiliser d'aller plus loin dans la configuration du périphérique nous devons :

- Signaler au périphérique que l'OS l'a reconnus
  => bit `ACKNOWLEDGE` de `device_status` à 1.
- Signlaer au périphérique que l'OS sait comment utiliser le périphérique
  => bit `DRIVER` de `device_status` à 1

### Etape 4 : Feature bits

Ici dans le cas spécifique des Input device les bits de features sont sont pas utilisé. Je renvoi donc le lecteur vers la documentation Virtio pour plus de détails. (spec 3.1.1 biblio 1).

## Etape 5 : FEATURE_OK

Après avoir séléctionné les bits de features voulue, on doit dire au périphérique qu'on a fini.  
On doit donc mettre le bit `VIRTIO_STATUS_FEATURES_OK` du registre `device_status` à 1.
ATTENTION : Après cela il ne faut pas essayer de séléctionner de nouveaux bits de features.

### Etape 6 : Vérifier FEATURE_OK

Il est possible que les features demandées par le driver (nous) ne soient pas supportées par le périphérique.  
Ainsi il faut vérifier que le bit `VIRTIO_STATUS_FEATURES_OK` dans le registre `device_status` reste bien à 1 sinon le périphérique refuse notre demande de features.

### Etape 7 : Configuration spécifique du périphérique

Maintenant nous devons configurer le périphérique pour qu'il puisse communiquer avec nous.
Nous allons donc devoir mettre en place les Virtqueues (partie d'après).

### Etape 8 : Activation du périphérique

Une fois la configuration terminer on doit activer le périphérique.  
Il faut pour ça mettre le bit `VIRTIO_STATUS_DRIVER_OK` du registre `device_status` à 1.

## VIRTIO - Configuration Virtqueue

Pour pouvoir communiquer avec les périphériques virtio nous devons utiliser des virtqueue.  
Le mécanisme de transport de données sur les périphériques virtio porte le nom pompeux de « virtqueue ». Chaque périphérique peut
disposer d'une ou plusieurs virtqueues. Dans notre cas de périphérique input il n'y a que 2 queue : `eventq` et `statusq`.
Nous allons ici uniquement utiliser `eventq`.
Le pilote met les requêtes à la disposition du périphérique en ajoutant un tampon disponible à la file d'attente, c'est-à-dire en ajoutant un tampon décrivant la requête à une virtqueue, et en déclenchant éventuellement un événement de driver, c'est-à-dire en envoyant une notification de tampon disponible au périphérique.

Le périphérique traite les requêtes et, une fois celles-ci terminées, ajoute un tampon utilisé à la file d'attente, c'est-à-dire qu'il en informe le pilote en marquant le tampon comme utilisé. Le périphérique peut alors déclencher un événement de périphérique, c'est-à-dire envoyer une notification de tampon utilisé au pilote.

Chaque virtqueue comporte trois parties :

- Descriptor Area : sert à décrire les tampons
- Driver Area : données supplémentaires fournies par le pilote au périphérique
- Device Area : données supplémentaires fournies par le périphérique au pilote

## Biblio

Documentation générale des virtio :
1: [Documentation Virtio v1.2](https://docs.oasis-open.org/virtio/virtio/v1.2/csd01/virtio-v1.2-csd01.pdf)

Pour la configuration PCI :
2: [Configuration PCI](https://wiki.osdev.org/PCI)

Plus d'info sur les interruptions :
4: [Interruptions](https://tldp.org/HOWTO/Plug-and-Play-HOWTO-7.html)
