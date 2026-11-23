# Projet Réseau de Capteurs — guide de mise en route

```
PROJET_FINAL/
├── WORKSPACE_RESCAPT_STM32CUBEIDE/rescapt   firmware (un seul projet, 3 cartes)
│   ├── src/main.c          ← CHOIX DE LA CARTE (3 #define en haut)
│   ├── inc/can_protocol.h  ← identifiants CAN + format des trames
│   ├── src/lps22hb.c, src/hts221.c, src/MadgwickAHRS.c  (nouveaux)
│   └── Makefile.cli        ← build en ligne de commande (optionnel)
└── WORKSPACE_QT/QT_RESCAPT
    ├── ihm_rescapt/        IHM Qt (Qt5 ou Qt6) : ihm_rescapt.pro
    └── tools/simulate_boards.py   simulateur des 3 cartes (tests sans matériel)
```

## 1. Architecture / protocole CAN (500 kbit/s, octets poids fort en premier)

| Carte | ID | Sens | Contenu |
|---|---|---|---|
| 1 Moteur + anémomètre | 0x11 | carte→PC, 500 ms | vent km/h×100 (u16), impulsions (u16), mode moteur, en rotation, consigne (s16) |
|  | 0x12 | PC→carte, 200 ms | mode (0 arrêt, 1 manuel, 2 auto), vitesse manuelle (s16, unité Dynamixel 0.229 tr/min), seuil vent km/h×100 (u16) |
| 2 VL6180X + IKS01A2 | 0x21 | carte→PC, 200 ms | mode (0 distance, 1 luminosité), valeur mm/lux (u32) |
|  | 0x22 | carte→PC, 500 ms | pression hPa×100 (u32), température °C×100 (s16) — LPS22HB |
|  | 0x23 | carte→PC, 500 ms | humidité %×100 (u16), température °C×100 (s16) — HTS221 |
|  | 0x24 | PC→carte, 200 ms | mode VL6180X (0 distance, 1 luminosité) |
| 3 MPU9250 | 0x31 | carte→PC, 20 ms | quaternion Madgwick q0..q3 (4×s16, ×10000) |
|  | 0x32 / 0x33 | carte→PC, 100 ms | accélération (mg) / gyroscope (0.1 °/s), 3×s16 |
|  | 0x34 | PC→carte | 1 = remise à zéro de l'orientation |

Chaque carte filtre en réception son seul ID de commande (filtre matériel CAN).

Comment chaque objectif est couvert :
- **Un ID CAN par carte** : 0x1x, 0x2x, 0x3x.
- **Toutes les données affichées en même temps, au moins en texte** : une fenêtre unique avec 3 panneaux, des valeurs en texte et des LED d'état par carte (en ligne / hors ligne).
- **Échanges périodiques** : les cartes émettent avec une période fixe, et l'IHM renvoie les commandes toutes les 200 ms.
- **Widgets** : jauges à aiguille (pression, distance/luminosité, humidité, température, vent) et courbes d'évolution.
- **Orientation en OpenGL + Madgwick** : le filtre de Madgwick tourne sur la carte à 100 Hz. L'IHM affiche le quaternion en 3D et le convertit en angles roulis/tangage/lacet.
- **Moteur commandable depuis l'IHM** : modes Arrêt / Manuel (curseur de vitesse) / bouton STOP.
- **Moteur qui tourne au-delà d'un seuil de vent** : mode Automatique, seuil réglable dans l'IHM (marqueur rouge sur la jauge), hystérésis de 10 %.
- **Basculer luminosité / distance depuis l'IHM** : boutons radio. Le switch rouge du shield fonctionne toujours : c'est la dernière action (switch ou IHM) qui l'emporte.
- **Pression / température / humidité traitées** : altitude barométrique, point de rosée (formule de Magnus), min/max et courbes.

## 2. Firmware STM32 (une Nucleo F103RB par carte)

### Avec STM32CubeIDE (méthode du TP)
1. *File → Import → General → Existing Projects into Workspace*, puis choisir le dossier `WORKSPACE_RESCAPT_STM32CUBEIDE` et cocher `rescapt`.
2. Dans `src/main.c`, mettre **un seul** `#define` à 1 :
   ```c
   #define VL6180X_PRESS_HUM_TEMP 1   // carte 2 : shields 6180XA1 + IKS01A2 empilés
   #define MPU9250                0   // carte 3
   #define DYN_ANEMO              0   // carte 1 : carte adaptateur DXL/anémomètre
   ```
   (Si vous mettez 0 ou plus d'un `#define` à 1, la compilation s'arrête volontairement sur une erreur.)
3. Lancer *Project → Build*, puis *Run* (ou *Debug*) avec la carte branchée en USB.
4. Recommencer les étapes 2 et 3 pour chacune des trois cartes.

### En ligne de commande (optionnel)
```bash
cd WORKSPACE_RESCAPT_STM32CUBEIDE/rescapt
export PATH=~/st/stm32cubeide_*/plugins/*gnu-tools-for-stm32*/tools/bin:$PATH   # adapter le chemin
make -f Makefile.cli BOARD=VL      # ou MPU / ANEMO  → build/<BOARD>/rescapt.bin
make -f Makefile.cli BOARD=VL flash
```
Cette méthode passe outre les `#define` de `main.c`.

### Logs série (utile pour vérifier une carte)
`minicom -D /dev/ttyACM0 -b 115200` (ou `screen /dev/ttyACM0 115200`). Au démarrage, chaque carte affiche :
- carte 2 : `id=180`, puis `LPS22HB OK, HTS221 OK` ;
- carte 3 : `WhoAmI = 0x71` ;
- carte 1 : la vitesse du vent, toutes les 500 ms.

### Points d'attention matériels
- **MPU9250** : laisser la carte **immobile environ 2 s après le reset**, le temps de la calibration du gyroscope.
- **Anémomètre** : son signal arrive sur PA8 (TIM1). Le coefficient `WIND_KMH_PER_HZ` (2.4 km/h par Hz) est une **valeur à vérifier sur la datasheet Somfy** ; il se règle en haut de `main.c`. La vitesse utilisée en mode auto se règle avec `AUTO_VELOCITY`.
- **Dynamixel** : ID 1, 57600 bauds, protocole 2.0, liaison USART1 (PA9) avec PA10 comme signal de direction. C'est la configuration fournie par l'enseignant.
- Le bus CAN doit avoir des **terminaisons de 120 Ω** et toutes les cartes doivent être à **500 kbit/s**.

## 3. IHM Qt sur le PC hôte (Linux)

### Dépendances
- Fedora : `sudo dnf install qt6-qtbase-devel mesa-libGL-devel gcc-c++ make can-utils`
- Ubuntu/Debian : `sudo apt install qt6-base-dev libgl1-mesa-dev build-essential can-utils`
  (avec Qt5 : `qtbase5-dev`, le même `.pro` fonctionne)

### Compilation
- **Qt Creator** : ouvrir `WORKSPACE_QT/QT_RESCAPT/ihm_rescapt/ihm_rescapt.pro`, puis *Build* et *Run*.
- **En ligne de commande** :
  ```bash
  cd WORKSPACE_QT/QT_RESCAPT/ihm_rescapt
  qmake6 && make -j        # ou qmake (Qt5)
  ```

### Activer l'interface CAN (adaptateur USB-CAN SocketCAN, par ex. PEAK)
```bash
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up
candump can0          # optionnel : vérifier que les trames 0x11 / 0x21 / 0x31... arrivent
```

### Lancement
```bash
./ihm_rescapt          # utilise can0 par défaut
./ihm_rescapt can1     # autre interface
```
La barre d'état affiche une LED par carte (vert = trames reçues depuis moins de 1,5 s) ainsi que les compteurs RX/TX.

### Tester l'IHM sans le matériel
```bash
sudo modprobe vcan
sudo ip link add dev vcan0 type vcan && sudo ip link set up vcan0
python3 ../tools/simulate_boards.py vcan0 &
./ihm_rescapt vcan0
```

## 4. Dépannage
| Symptôme | Cause probable |
|---|---|
| « Impossible d'ouvrir can0 » | interface absente ou non montée (`ip link`) |
| LED d'une carte rouge | mauvais `#define` flashé, débit CAN différent, câblage CANH/CANL ou terminaison |
| Rien sur le bus avec une seule carte | normal : sans un 2e nœud pour acquitter (ACK), les trames sont réémises en boucle ; brancher l'adaptateur PC |
| Pression/humidité absentes | le log série affiche `NOT FOUND` : vérifier que le shield IKS01A2 est bien enfoncé |
| Le cube 3D dérive | la carte a bougé pendant la calibration : appuyer sur reset, puis sur « Réinitialiser l'orientation » |

## 5. Modifications apportées au code fourni
- `main.c` réécrit : sélection de la carte, envois périodiques, réception des commandes (l'interruption CAN copie la trame, le traitement se fait dans la boucle principale), commande du moteur, mode du VL6180X piloté par CAN.
- Nouveaux pilotes LPS22HB et HTS221 (avec la calibration usine), filtre de Madgwick, `can_protocol.h`.
- `anemo.c` : période de TIM1 passée de 200 à 0xFFFF (le compteur repassait à zéro au-delà de 200 impulsions).
- `mpu9250.c` : `mpu9250_Step()` rend désormais ses mesures disponibles (`imu_acc`, `imu_gyr`).
- `drv_uart.h` : ajout du prototype manquant de `uart1_Init` (erreur avec GCC ≥ 14).
- `socketcan_cpp.h` : ajout de `#include <cstdint>` (erreur avec les compilateurs récents).
- Visualisation OpenGL portée de `QGLWidget` (supprimé dans Qt6) vers `QOpenGLWidget`.
