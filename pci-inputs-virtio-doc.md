# Documentation périphériques inputs

Par Vincent Verdillon

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
- l'adresse mémoire pour l'utilisation, `BAR4` car prefetchable.

### Registres Bases address

Ce registre PCI permet d'indiquer les zones mémoire que les périphériques doivent utiliser pour leur fonctionnement.

Pour indiquer les adresses mémoires d'après les informations données par `info qtree` dans la console QEMU il faut :

- placer la première valeur dans le registre `BAR1`
- placer la seconde valeur dans le registre `BAR4`

Le tout est analogue à la première configuration de la carte graphique.
On doit ainsi obtenir dans la console QEMU à peu près le même résultat que j'ai donné au niveau des données dans les BAR.

J'ai choisi les adresses de manière arbitraire mais en gardant en tête que la zone mémoire requise n'est pas gargantuesque.
Donc, n'allez pas mettre `BAR1` à `0x600000000` et `BAR4` à `0x700000000`, c'est inutile, soyez frugale en termes de mémoire. Vous pouvez voir avec `info pci` l'adresse de fin qui est utilisée pour calculer la taille de la adresse nécessaire.

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

### Adresse de configuration

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

### Etape 5 : FEATURE_OK

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

Je recommande de créer une structure pour stocker toutes les informations utile à propos d'un device virtio :

```C
/* Inputs virtio device structure, include virtio_device */
typedef struct {
    cfg_virtio_device cfg_mem_map;             // cfg memory map
    virtq queue0;                              // input virtqueue
    virtio_input_event event_pool[QUEUE_SIZE]; // event pool
    uint16_t last_used_idx;
    uint32_t notify_off; // Multiplier from PCI notify capability (used to
                         // compute notify address)
    void (*handle_event)(virtio_input_event *event);
} input_virtio_device;
```

Pour pouvoir communiquer avec les périphériques virtio nous devons utiliser des virtqueue.  
Le mécanisme de transport de données sur les périphériques virtio porte le nom pompeux de « virtqueue ».
Chaque périphérique peut disposer d'une ou plusieurs virtqueues. Dans notre cas de périphérique input il n'y a que 2 queue : `eventq` et `statusq`.
Nous allons ici uniquement utiliser `eventq`.
Le pilote met les requêtes à la disposition du périphérique en ajoutant un buffer disponible à la file d'attente, c'est-à-dire en ajoutant un buffer décrivant la requête à une virtqueue, et en déclenchant éventuellement un événement de driver, c'est-à-dire en envoyant une notification de tampon disponible au périphérique.

Le périphérique traite les requêtes et, une fois celles-ci terminées, ajoute un buffer utilisé à la file d'attente, c'est-à-dire qu'il en informe le pilote en marquant le tampon comme utilisé. Le périphérique peut alors déclencher un événement de périphérique, c'est-à-dire envoyer une notification de tampon utilisé au pilote.

Chaque virtqueue comporte trois parties :

- Descriptor Area : sert à décrire les buffers
- Driver Area : données supplémentaires fournies par le pilote au périphérique
- Device Area : données supplémentaires fournies par le périphérique au pilote

Dans la suite nous allons supposer utiliser des `Split Virtqueue` (spec 2.7 bibli 1), au lieu de `Packed Virtqueue`. Les deux modes de fonctionnement sont tous les deux supportés et j'ai choisie d'utiliser le plus ancien.

### Virtqueue Descriptor

```C
/* This marks a buffer as continuing via the next field. */
#define VIRTQ_DESC_F_NEXT 1
/* This marks a buffer as device write-only (otherwise device read-only). */
#define VIRTQ_DESC_F_WRITE 2
/* This means the buffer contains a list of buffer descriptors. */
#define VIRTQ_DESC_F_INDIRECT 4

/* Virtqueue descriptors: 16 bytes.
 * These can chain together via "next". */
typedef struct {
    uint64_t addr;  // Address (guest-physical).
    uint32_t len;   // Length
    uint16_t flags; // The flags as indicated above.
    uint16_t next;  // Next field if flags & NEXT
} __attribute__((packed, aligned(4))) virtq_desc;
```

La table descriptor est la partie buffer du driver pour le périphérique. L'adresse `addr` réfère à un emplacement mémoire qui permet de stocker les messages envoyé et reçu par le périphérique et le driver, ici une case de `event_pool`.  
Ici nous voulons que le périphérique nous envoie des messages donc il aut mettre `flags` à `VIRTQ_DESC_F_WRITE`.  
Nous n'allons chainer la liste donc on ne touchera pas à `next`.

### Virtqueue Available Ring

```C
/* The device writes available ring entries with buffer head indexes. */
typedef struct {
    uint16_t flags;
    uint16_t idx; // ATOMIC INSTRUCT
    uint16_t ring[QUEUE_SIZE];
    uint16_t used_event; // Only if VIRTIO_F_EVENT_IDX
} __attribute__((packed, aligned(4))) virtq_avail;
```

Le driver utilise l'available ring pour proposer des buffers au périphérique : chaque entrée de la liste correspond à la tête d'une chaîne de descripteurs. Elle est uniquement écrite par le pilote et lue par le périphérique.  
Le champ idx indique l'emplacement où le driver placerait la prochaine entrée de la chaîne dans la liste (modulo la taille de la file d'attente).
Il commence à 0 et augmente progressivement.

A noter que l'écriture de `idx` doit être "atomique". C'est à dire que les instructions d'écriture doivent être faite une et une seul fois par le processeur pour éviter que deux processus ou coeur n'écrivent en même temps ce champs. Dans notre cas particulier ce ne pose pas de problème car nous n'avons qu'un coeur. Mais pour faire les choses corrctement il faut utiliser ça avant l'instruction d'écriture d'`idx`:

```C
__asm__ __volatile__("fence" ::: "memory");
```

### Virtqueues used

```C
/* uint32_t is used here for ids for padding reasons. */
typedef struct {
    uint32_t id; // Index of start of used descriptor chain.
    /*
     * The number of bytes written into the device writable portion of
     * the buffer described by the descriptor chain.
     */
    uint32_t len;
} __attribute__((packed, aligned(4))) virtq_used_elem;

/* The device writes used elements into this ring. */
typedef struct {
    uint16_t flags;
    uint16_t idx; // ATOMIC INSTRUCT
    virtq_used_elem ring[QUEUE_SIZE];
    uint16_t avail_event; // Only if VIRTIO_F_EVENT_IDX
} __attribute__((packed, aligned(4))) virtq_used;
```

La file d'attente « used » est l'endroit où le périphérique renvoie les buffers une fois qu'il en a fini avec eux : elle est uniquement écrite par le périphérique et lue par le driver.
Chaque entrée de la file d'attente est un couple : « id » indique l'entrée de tête de la chaîne de descripteurs décrivant le buffer (celle-ci correspond à une entrée placée précédemment dans la file d'attente « available » par l'invité), et « len » le nombre total d'octets écrits dans le buffer.

## Implementation

Dans cette section nous allons voir comment utiliser les périphériques virtio inputs.

### Mémoire

Nous devons stocker les informations générales à propos de ces périphériques donc il faut définir les structures précédemment, plus les emplacements mémoires dédiés :

```C
``/* Virtio */
extern volatile input_virtio_device virtio_device_inputs[2];
extern volatile uint8_t virtio_input_irqs[2];

/* mouse buttons */
extern volatile uint8_t lb_status; // left mouse button status
extern volatile uint8_t rb_status; // right mouse button status
extern volatile uint8_t mb_status; // middle mouse button status

/* mouse position */
extern volatile int32_t mouse_x;
extern volatile int32_t mouse_y;
```

Les champs pour les souris seront utilisé un peu plus tard.

### Configuration initiale

On reprend le code principale à l'étape 7 et 8 en rajoutant la configuration de la virtqueue.

Il faut configurer la virtqueue 0, `eventq`, on l'a séléctionne et spécifie ça taille (128 éléments arbitrairement ici) :

```C
cfg->queue_select = 0;
cfg->queue_size = (uint16_t)QUEUE_SIZE;
```

On spécifie les adresse des trois éléments de la virtqueue dans la mémoire dédiés du périphérique :

```C
cfg->queue_desc = (uint64_t)&virtio_device_inputs[i].queue0.descriptors;
cfg->queue_driver = (uint64_t)&virtio_device_inputs[i].queue0.available_ring;
cfg->queue_device = (uint64_t)&virtio_device_inputs[i].queue0.used_ring;
```

Puis on les initialise eux aussi :

```C
/* initial filling of the queue */
for (uint16_t j = 0; j < QUEUE_SIZE; j++) {
    virtio_device_inputs[i].queue0.descriptors[j].addr = (uint64_t)&virtio_device_inputs[i].event_pool[j];
    virtio_device_inputs[i].queue0.descriptors[j].len = sizeof(virtio_input_event);
    virtio_device_inputs[i].queue0.descriptors[j].flags = VIRTQ_DESC_F_WRITE;
    virtio_device_inputs[i].queue0.available_ring.ring[j] = j;
}
```

Comme nous travaillons avec des buffer "circulaire" il faut aussi initialiser l'index précédemment utilisé et le suivant. Modulo évidemment la taille du buffer.

```C
__asm__ __volatile__("fence" ::: "memory");

/* initialisation des index */
virtio_device_inputs[i].last_used_idx = 0;
virtio_device_inputs[i].queue0.available_ring.idx = QUEUE_SIZE;
```

La gestion des interruptions ce fera via le PLIC donc inutile il n'y a pas de champs interruptions. Cependant, par la suite il faudra créer des fonctions pour traiter ces interruptions, il est donc nécessaire des les spécifier :

```C
/* specifie event handler for each device */
if (i == 0) {
    virtio_device_inputs[i].handle_event = handle_keyboard_event;
} else {
    virtio_device_inputs[i].handle_event = handle_mouse_event;
}
```

A partir de maintenant la virtqueue est opérationnel, il ne reste plus qu'a l'activer et activer le périphérique pour conclure cette partie :

```C
cfg->queue_enable = 1;

/* --- Étape 8 : DRIVER_OK → le device est prêt --- */
cfg->device_status |= VIRTIO_STATUS_DRIVER_OK;
```

BRAVO ! Vous venez d'initiliser la virtqueue. Mais maintenant il faut la lancer elle aussi en envoyant une notification.

### Virtqueue First Kick

Pour démarrer la virtqueue il suffit d'écrire dans l'espace de notification du périphérique.  
Espace qui ce trouve à un addresse qu'il faut calculer comme ci-dessous :

```C
/* Premier kick: notifier la queue via la notify BAR */
volatile input_virtio_device *dev = &virtio_device_inputs[i];
uint64_t notify_base = dev->cfg_mem_map.notify_cfg;
uint64_t notify_addr = notify_base + ((uint64_t)cfg->queue_notify_off * (uint64_t)dev->notify_off);
```

Pour le forme on met à jour le dernier `idx` :

```
__asm__ __volatile__("fence" ::: "memory");
volatile virtq_used *used = &dev->queue0.used_ring;
uint16_t used_before = used->idx;
```

Si l'adresse calculer est bonne on notifie le périphérique qu'on est ok pour continuer :

```
/* Write notify (kick) */
if (notify_addr != 0) {
    *((volatile uint16_t *)notify_addr) = 0; // 0 = index de la queue
}
```

A partir de maintenant on va recevoir des interruptions depuis le PLIC qu'il va falloir gérer. Les parties suivantes vont traiter ces points.

### Keyboard handler

Pour recevoir les events provenant du clavier nous devons sépcifier un handler :

```C
void handle_keyboard_event(virtio_input_event *event);
```

Le handler est appelé depuis le `trap_handler` des interruptions et permet de gérer les events ad-hoc du clavier et de la souris.
Les events du clavier sont avec la structure suivantes :

```C
typedef enum { Sync = 0, Key = 1, Relative = 2, Absolute = 3 } input_event_type;

typedef struct __attribute__((packed)) {
    uint16_t event_type;
    uint16_t code;
    uint32_t value;
} virtio_input_event;
```

Le champ `code` permet de connaitre le code de la touche du clavier. J'ai remapper tous le clavier et dans le cas d'un AZERTY voilà la liste des code :

```C
volatile uint8_t key_to_ascii_std[126] = {
    0x1B, 0,    '&',   0xE9, '"', '\'',  '(', '-', 0xE8, '_', 0xE7, 0xE0, ')',
    '=',  127,  ENTER, 'a',  'z', 'e',   'r', 't', 'y',  'u', 'i',  'o',  'p',
    0,    '$',  '\r',  0,    'q', 's',   'd', 'f', 'g',  'h', 'j',  'k',  'l',
    'm',  0xF9, 0xB2,  0,    '*', 'w',   'x', 'c', 'v',  'b', 'n',  ',',  ';',
    ':',  '!',  0,     '*',  0,   ' ',   0,   0,   0,    0,   0,    0,    0,
    0,    0,    0,     0,    0,   0,     '7', '8', '9',  '-', '4',  '5',  '6',
    '+',  '1',  '2',   '3',  0,   '.',   0,   0,   '<',  0,   0,    0,    0,
    0,    0,    0,     0,    0,   ENTER, 0,   '/', 0,    0,   0,    0,    0,
    0,    0,    0,     0,    0,   0,     0,   0,   0,    0,   0,    0,    0,
    0,    0,    0,     0,    0,   0,     0,   0,   SUPER};
volatile uint8_t key_to_ascii_maj[126] = {
    0x1B,  0,   '1', '2',   '3',  '4',  '5', '6',  '7', '8',      '9', '0',
    0xB0,  '+', 127, ENTER, 'A',  'Z',  'E', 'R',  'T', 'Y',      'U', 'I',
    'O',   'P', 0,   0xA3,  '\r', 0,    'Q', 'S',  'D', 'F',      'G', 'H',
    'J',   'K', 'L', 'M',   0x25, 0x7E, 0,   0xB5, 'W', 'X',      'C', 'V',
    'B',   'N', '?', '.',   '/',  0xA7, 0,   '*',  0,   ' ',      0,   0,
    0,     0,   0,   0,     0,    0,    0,   0,    0,   VERR_NUM, 0,   '7',
    '8',   '9', '-', '4',   '5',  '6',  '+', '1',  '2', '3',      0,   '.',
    0,     0,   '>', 0,     0,    0,    0,   0,    0,   0,        0,   0,
    ENTER, 0,   '/', 0,     0,    0,    0,   0,    0,   0,        0,   0,
    0,     0,   0,   0,     0,    0,    0,   0,    0,   0,        0,   0,
    0,     0,   0,   0,     0,    SUPER};
volatile uint8_t key_to_ascii_alt_gr[126] = {
    0x1B,  0,    0xB9, 0x7E,  '#',  '{',  '[',  '|',  '`',  '\\', '^', '@',
    ']',   '}',  127,  ENTER, 0xE6, 0xAB, 0x80, 0xB6, 0,    0,    0,   0,
    0xD8,  0xDE, 0,    0xA4,  '\r', 0,    '@',  0xDF, 0xD0, 0,    0,   0,
    0,     0,    0,    0xB5,  0,    0xAC, 0,    '*',  'w',  'x',  'c', 'v',
    'b',   'n',  ',',  ';',   ':',  '!',  0,    '*',  0,    ' ',  0,   0,
    0,     0,    0,    0,     0,    0,    0,    0,    0,    0,    0,   '7',
    '8',   '9',  '-',  '4',   '5',  '6',  '+',  '1',  '2',  '3',  0,   '.',
    0,     0,    '<',  0,     0,    0,    0,    0,    0,    0,    0,   0,
    ENTER, 0,    '/',  0,     0,    0,    0,    0,    0,    0,    0,   0,
    0,     0,    0,    0,     0,    0,    0,    0,    0,    0,    0,   0,
    0,     0,    0,    0,     0,    SUPER};
```

Après libre à vous de trouver un moyen de gérer les touches spéciales, Maj, Alt, Altg, Verr_maj etc...

### Mouse handler

Dans le cas du handler souris la structure est à peut près la même :

```C
void handle_mouse_event(virtio_input_event *event);
```

Le champ `event_type` décris le type d'event qui arrive depuis la souris.

- `Relative` indique que l'event est la position de la souris OU la roulette de la souris. Et dans ce cas là le champ value stocke le delta entre l'ancienne position et la nouvelle de la souris OU de la souris. NB. Ce delta peut être négatif.
- `Pressed` indique que l'event est un bouton préssé. Et ainsi le champ code décris le bouton qui a été modifié. NB.

Je vous donne les valeurs qui peuvent être soit retrouver dans la documentation Oasis ou retrouvée en testant la souris à la main.

```C
typedef enum { Sync = 0, Key = 1, Relative = 2, Absolute = 3 } input_event_type;

typedef enum { X = 0, Y = 1, Wheel = 8 } input_event_code_relative;

typedef enum {
    LB = 272,
    RB = 273,
    MB = 274,
    WHEEL_DOWN = 336,
    WHEEL_UP = 337
} input_event_code_abs;

typedef enum {
    Released = 0,
    Pressed = 1,
    Automatic_Repetition = 2,
} input_event_code_value;

typedef enum {
    BUTTON_UP = 0,
    BUTTON_DOWN = 1,
} mouse_button_status;
```

## Conclusion

J'ai décris les principaux mécanismes pour communiquer et utiliser les devices de type inputs. Le reste est à votre appréciation. AMusez vous bien.

## Biblio

Documentation générale des virtio :
1: [Documentation Virtio v1.2](https://docs.oasis-open.org/virtio/virtio/v1.2/csd01/virtio-v1.2-csd01.pdf)

Pour la configuration PCI :
2: [Configuration PCI](https://wiki.osdev.org/PCI)

Plus d'info sur les interruptions :
4: [Interruptions](https://tldp.org/HOWTO/Plug-and-Play-HOWTO-7.html)

```

```
