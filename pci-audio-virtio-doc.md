# Documentation périphériques audio

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

## Point mémoire

ATTENTION : Comme le précise la documentation (spec 4.1.3.1 biblio 1) tous les accès mémoire doivent correspondre à la taille du champs lu et doivent être alignés.

## Info générales

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
Il faut pour ça mettre le bit `VIRTIO_STATUS_DRIVER_OK` du registre `device_status` à 1. Et à partir de ce moment le device est actif et peut envoyer des interruptions etc...

## VIRTIO - Configuration Virtqueue

Je recommande de créer une structure pour stocker toutes les informations utile à propos d'un device virtio :

```C
/* Virtqueue */
typedef struct {
    virtq_desc_audio descriptors[QUEUE_SIZE_AUDIO];
    virtq_avail_audio available_ring;
    virtq_used_audio used_ring;
} __attribute__((packed, aligned(4096))) virtq_audio;

/* Audio device structure */
typedef struct {
    cfg_virtio_device cfg_mem_map;
    virtq_audio controlq;
    virtq_audio eventq;
    virtq_audio txq;
    virtq_audio rxq;
    uint16_t last_used_idx_ctrl;
    uint16_t last_used_idx_tx;
    uint32_t notify_off;
} audio_virtio_device;
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
} __attribute__((packed, aligned(4))) virtq_desc_audio;
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
} __attribute__((packed, aligned(4))) virtq_avail_audio;
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
} __attribute__((packed, aligned(4))) virtq_used_elem_audio;

/* The device writes used elements into this ring. */
typedef struct {
    uint16_t flags;
    uint16_t idx; // ATOMIC INSTRUCT
    virtq_used_elem ring[QUEUE_SIZE];
    uint16_t avail_event; // Only if VIRTIO_F_EVENT_IDX
} __attribute__((packed, aligned(4))) virtq_used_audio;
```

La file d'attente « used » est l'endroit où le périphérique renvoie les buffers une fois qu'il en a fini avec eux : elle est uniquement écrite par le périphérique et lue par le driver.
Chaque entrée de la file d'attente est un couple : « id » indique l'entrée de tête de la chaîne de descripteurs décrivant le buffer (celle-ci correspond à une entrée placée précédemment dans la file d'attente « available » par l'invité), et « len » le nombre total d'octets écrits dans le buffer.

## Implémentation

Dans cette section nous allons voir comment utiliser les périphériques virtio audios.

### mémoire

Nous devons stocker les informations générales à propos de ces périphériques donc il faut définir les structures précédemment, plus les emplacements mémoires dédiés :

```C
extern volatile cfg_virtio_device cfg_types_found_audio;
extern volatile audio_virtio_device virtio_audio_dev;
extern volatile uint8_t virtio_audio_irq;
```

NB. J'ai mis dans le fichier `virtio.h` des structures de données, notemment `cfg_virtio_device` qui permettent de stocker les données des devices.

### Configuration initiale

On reprend le code principale à l'étape 7 et 8 en rajoutant la configuration de la virtqueue.

Il faut configurer les virtqueues `controlq`, `eventq`, `txq`, `rxq`. Pour cela on séléctionne cellel que l'on souhaite initialiser et on spécifie ça taille (128 éléments arbitrairement ici). Comme la méthode est répétitive, je vous montre pour configurer `controlq` et je vous laisse configurer les autres :

```C
/* Configure controlq (queue 0) */
cfg->queue_select = 0;
cfg->queue_size = (uint16_t)QUEUE_SIZE_AUDIO;
cfg->queue_desc = (uint64_t)&virtio_audio_dev.controlq.descriptors;
cfg->queue_driver = (uint64_t)&virtio_audio_dev.controlq.available_ring;
cfg->queue_device = (uint64_t)&virtio_audio_dev.controlq.used_ring;

/* Initialize control queue indices */
virtio_audio_dev.last_used_idx_ctrl = 0;
virtio_audio_dev.controlq.available_ring.idx = 0;

/* Queue activation */
__asm__ __volatile__("fence" ::: "memory");
cfg->queue_enable = 1;
```

Après ça on doit récupérer `notify multiplier` pour pouvoir notifier au device que l'on a envoyer un buffer par une virtqueue.

```C
/* Get notify multiplier from capability (preferred) */
if (cfg_types_found_audio.notify_off_multiplier != 0) {
    virtio_audio_dev.notify_off =
        cfg_types_found_audio.notify_off_multiplier;
} else {
    /* Fallback: read first dword at notify_cfg (legacy pattern) */
    if (virtio_audio_dev.cfg_mem_map.notify_cfg != 0) {
        virtio_audio_dev.notify_off =
            *((volatile uint32_t *)virtio_audio_dev.cfg_mem_map.notify_cfg);
    } else {
        AUDIO_DBG("[audio] WARNING: notify_cfg absent\n");
        virtio_audio_dev.notify_off = 4; /* safe default */
    }
}
```

## Jouer du son (beeeppp !)

Maintenant que le device est configuré nous allons pouvoir nous en servir pour jouer du son. Pour des raisons de simplicité nous nn'allons que jouer une note, un gros beep comme proof of concept.

Pour cela nous devons utiliser le système de `PCM Control Messages` spécifier dans la documentation Oasis 5.14.6.6. Voilà les structures de données que nous allons utiliser (qui viennent de Oasis) :

```C
/* VirtIO Sound control request header */
typedef struct {
    uint32_t code;
} __attribute__((packed)) virtio_snd_hdr;

/* VirtIO Sound PCM set params request */
typedef struct {
    virtio_snd_hdr hdr;
    uint32_t stream_id;
    uint32_t buffer_bytes;
    uint32_t period_bytes;
    uint32_t features;
    uint8_t channels;
    uint8_t format;
    uint8_t rate;
    uint8_t padding;
} __attribute__((packed, aligned(4))) virtio_snd_pcm_set_params;

/* VirtIO Sound simple request (for prepare, start, stop) */
typedef struct {
    virtio_snd_hdr hdr;
    uint32_t stream_id;
} __attribute__((packed, aligned(4))) virtio_snd_pcm_simple;

/* VirtIO Sound status response */
typedef struct {
    uint32_t status;
} __attribute__((packed, aligned(4))) virtio_snd_status;

/* VirtIO Sound PCM transfer header */
typedef struct {
    uint32_t stream_id;
} __attribute__((packed, aligned(4))) virtio_snd_pcm_xfer;
```

### Configuration PCM générale

Le device ne peut pas deviner les paramètres que l'on souhaite avoir pour jouer le morceau de son. Donc on doit lui dire avec une requete virtqueue.

On va suivre la structure des requetes PCM comme décris dans la documenation Oasis.

### Etape 1 : Set PCM parameters

La première étape est de spécifier le type de son que l'on va jouer :

NB. Le constantes viennent de la documentation, je ne les ai pas mise pour des questions de place (y a beaucoup de champs).

```C
/* Step 1: Set PCM parameters */
volatile virtio_snd_pcm_set_params set_params;
volatile virtio_snd_status status_response;

/* Calculate buffer size for 200ms at 44.1kHz stereo S16 (2 bytes) */
/* 48000 * 0.2 * 2 channels * 2 bytes = 38400 bytes */
uint32_t buffer_size = (SAMPLE_RATE * DURATION_MS * 2 * 2) / 1000;

set_params.hdr.code = VIRTIO_SND_R_PCM_SET_PARAMS;
set_params.stream_id = 0;
set_params.buffer_bytes = buffer_size;
set_params.period_bytes = buffer_size; // Single period
set_params.features = 0;
set_params.channels = 2;
set_params.format = VIRTIO_SND_PCM_FMT_S16;
set_params.rate = VIRTIO_SND_PCM_RATE_5512;
set_params.padding = 0;
```

Une fois les paramètres définies on doit les envoyers au device. Nous allons utiliser une fonction standard pour envoyer une requete virtqueue et recevoir une réponse. Je vous donne le code pour faire la requete :

```C
/* Helper to send a control command to the audio device */
static void send_control_command(volatile void *request, uint32_t request_len,
                                 volatile void *response,
                                 uint32_t response_len) {
    volatile virtio_pci_common_cfg *cfg =
        (volatile virtio_pci_common_cfg *)
            virtio_audio_dev.cfg_mem_map.common_cfg;

    uint16_t idx =
        virtio_audio_dev.controlq.available_ring.idx % QUEUE_SIZE_AUDIO;
    uint16_t desc_idx = idx;

    /* First descriptor: request (device reads) */
    virtio_audio_dev.controlq.descriptors[desc_idx].addr = (uint64_t)request;
    virtio_audio_dev.controlq.descriptors[desc_idx].len = request_len;
    virtio_audio_dev.controlq.descriptors[desc_idx].flags = VIRTQ_DESC_F_NEXT;
    virtio_audio_dev.controlq.descriptors[desc_idx].next =
        (desc_idx + 1) % QUEUE_SIZE_AUDIO;

    /* Second descriptor: response (device writes) */
    uint16_t resp_idx = (desc_idx + 1) % QUEUE_SIZE_AUDIO;
    virtio_audio_dev.controlq.descriptors[resp_idx].addr = (uint64_t)response;
    virtio_audio_dev.controlq.descriptors[resp_idx].len = response_len;
    virtio_audio_dev.controlq.descriptors[resp_idx].flags = VIRTQ_DESC_F_WRITE;
    virtio_audio_dev.controlq.descriptors[resp_idx].next = 0;

    /* Add to available ring */
    virtio_audio_dev.controlq.available_ring.ring[idx] = desc_idx;

    __asm__ __volatile__("fence" ::: "memory");

    virtio_audio_dev.controlq.available_ring.idx++;

    /* Notify the device (queue 0) */
    cfg->queue_select = 0;
    uint64_t notify_base = virtio_audio_dev.cfg_mem_map.notify_cfg;
    uint64_t notify_addr =
        notify_base + ((uint64_t)cfg->queue_notify_off *
                       (uint64_t)virtio_audio_dev.notify_off);

    __asm__ __volatile__("fence" ::: "memory");

    if (notify_addr != 0) {
        *((volatile uint16_t *)notify_addr) = 0;
    }

    /* Wait for response */
    volatile virtq_used_audio *used = &virtio_audio_dev.controlq.used_ring;
    while (virtio_audio_dev.last_used_idx_ctrl == used->idx) {
        /* busy wait */
    }

    virtio_audio_dev.last_used_idx_ctrl = used->idx;
}
```

On n'a plus qu'a utiliser cete fonction pour envoyer `set_params`, récupérer la réponse et vérifier que la réponse est bien égale à `VIRTIO_SND_OK`.

### Etape 2 : Prepapre Stream

En suite il faut dire au device de préparer le stream. C'est à dire envoyer un requete PCM simple :

```C
volatile virtio_snd_pcm_simple prepare_cmd;
prepare_cmd.hdr.code = VIRTIO_SND_R_PCM_PREPARE;
prepare_cmd.stream_id = 0;
```

### Etape 3 : Start Stream

Maintenant que l'on a préparer le stream il faut le démmarrer pour commencer à jouer le son. Le manoeuvre est similaire à la précédente :

```C
volatile virtio_snd_pcm_simple start_cmd;
start_cmd.hdr.code = VIRTIO_SND_R_PCM_START;
start_cmd.stream_id = 0;
```

### Etape 4 : Génerer le sample à jouer et l'envoyer

Le périphérique audio joue un sample en amplitude et non en fréquence. Donc nous devons lui envoyer le graphe en amplitude de notre son. J'ai choisi à La3 440Hz rectangulaire. LE code ci-dessous permet de créer le graphe et le stocker dans un buffer.

```C
volatile int16_t audio_buffer[NUM_SAMPLES * 2] __attribute__((aligned(16)));

/* Generate a simple square wave */
for (int i = 0; i < NUM_SAMPLES; i++) {
    /* Simple square wave for testing - audible volume */
    int16_t sample;
    int phase = (i * FREQUENCY) / SAMPLE_RATE;

    if (phase % 2 == 0) {
        sample = SND_VOLUME;
    } else {
        sample = -SND_VOLUME; /* Negative */
    }

    audio_buffer[i * 2] = sample;     /* Left channel */
    audio_buffer[i * 2 + 1] = sample; /* Right channel */
}
```

En suite, il faut l'envoyer au device via la virtqueue TX (l'entrée) suivant le même mécanisme que les requete utilisé plus haut :

```C
volatile virtio_snd_pcm_xfer xfer_hdr;
xfer_hdr.stream_id = 0;

uint16_t idx = virtio_audio_dev.txq.available_ring.idx % QUEUE_SIZE_AUDIO;
uint16_t desc_idx = idx;

/* First descriptor: transfer header */
virtio_audio_dev.txq.descriptors[desc_idx].addr = (uint64_t)&xfer_hdr;
virtio_audio_dev.txq.descriptors[desc_idx].len = sizeof(xfer_hdr);
virtio_audio_dev.txq.descriptors[desc_idx].flags = VIRTQ_DESC_F_NEXT;
virtio_audio_dev.txq.descriptors[desc_idx].next =
    (desc_idx + 1) % QUEUE_SIZE_AUDIO;

/* Second descriptor: audio data (device reads) */
uint16_t data_idx = (desc_idx + 1) % QUEUE_SIZE_AUDIO;
virtio_audio_dev.txq.descriptors[data_idx].addr = (uint64_t)audio_buffer;
virtio_audio_dev.txq.descriptors[data_idx].len =
    NUM_SAMPLES * 2 * sizeof(int16_t);
virtio_audio_dev.txq.descriptors[data_idx].flags =
    VIRTQ_DESC_F_NEXT; /* device reads */
virtio_audio_dev.txq.descriptors[data_idx].next =
    (data_idx + 1) % QUEUE_SIZE_AUDIO;

/* Third descriptor: status (device writes) */
static virtio_snd_status xfer_status;
uint16_t status_idx = (data_idx + 1) % QUEUE_SIZE_AUDIO;
xfer_status.status = 0;
virtio_audio_dev.txq.descriptors[status_idx].addr = (uint64_t)&xfer_status;
virtio_audio_dev.txq.descriptors[status_idx].len = sizeof(xfer_status);
virtio_audio_dev.txq.descriptors[status_idx].flags = VIRTQ_DESC_F_WRITE;
virtio_audio_dev.txq.descriptors[status_idx].next = 0;

/* Add to available ring */
virtio_audio_dev.txq.available_ring.ring[idx] = desc_idx;

__asm__ __volatile__("fence" ::: "memory");

virtio_audio_dev.txq.available_ring.idx++;

/* Notify the device (queue 2) */
cfg->queue_select = 2;
uint64_t notify_base = virtio_audio_dev.cfg_mem_map.notify_cfg;
uint64_t notify_addr =
    notify_base + ((uint64_t)cfg->queue_notify_off *
                    (uint64_t)virtio_audio_dev.notify_off);

__asm__ __volatile__("fence" ::: "memory");

if (notify_addr != 0) {
    write_mem16(notify_addr, 0, 2);
}
```

L'envoie des données est sous la forme d'une liste chainé ou l'on spécifie la case du buffer suivante dans le champs `next` de `txq`. Après avoir ajouté le buffer audio il faut évidement notifier le device de cet ajout pour qu'il puisse le lire. Pour cela on écrit dans le champ de notification de le queue 2 càd `txq`.

A partir de maintenant le device est normalement en train de jouer le sample. Du point de vue d'un OS c'est TRES long (plusieurs ms). Donc il faut attendre qu'il ai fini pour terminer la transaction correctemnt.

```C
/* wait for used ring update with timeout */
volatile virtq_used_audio *used = &virtio_audio_dev.txq.used_ring;
uint16_t start_used = used->idx;

while (used->idx == start_used) {
    hlt();
}
```

### Etape 5 : Stop Stream

A partir de là on considère que le device a terminé de jouer le sample et nous pouvons terminer la transaction :

```C
/* Step 5: Stop stream */
static virtio_snd_pcm_simple stop_cmd;
stop_cmd.hdr.code = VIRTIO_SND_R_PCM_STOP;
stop_cmd.stream_id = 0;
```

### Etape 6 : Release Stream

Et en fin il est nécessaire de relacher le stream que l'on utilisait pour le rendre accéssible.

```C
/* Step 6: Release stream */
static virtio_snd_pcm_simple release_cmd;
release_cmd.hdr.code = VIRTIO_SND_R_PCM_RELEASE;
release_cmd.stream_id = 0;
```

## Conclusion

Mes félicitations ! Normalement si vous arrivé jusque l'a c'est que vous vous êtes accroché et que vous avez réussi à attendre un joli beep depuis votre ordinateur. L'étape d'après et de jouer de la musique mais je laisse les lecteurs motivesr et ambitieux continuer cette grande aventure pour qui sait devenir le prochain LInux Torvals du monde.

## Biblio

Documentation générale des virtio :
1: https://docs.oasis-open.org/virtio/virtio/v1.2/csd01/virtio-v1.2-csd01.pdf

Configuration de QEMU :
2: https://www.qemu.org/docs/master/system/devices/virtio/virtio-snd.html

Pour la configue PCI :
3: https://wiki.osdev.org/PCI
