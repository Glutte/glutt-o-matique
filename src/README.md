Structure des dossiers source
=============================

- *bsp* pilotes et lib essentielle pour le STM32F4
- *common* sources utilisées dans la glutt-o-logique et le simulateur
- *FreeRTOS* sources de l'OS FreeRTOS pour le STM32F4
- *FreeRTOS-Sim-master* sources de la variante de FreeRTOS pour le simulateur
- *glutt-o-logique* code applicatif spécifique au STM32F4
- *simulator* code applicatif spécifique au simulateur y.c. interface graphique
- *temperature* code d'exemple, pour illustrer l'utilisation des entrées analogiques du STM32

Compiler le Simulateur
======================

Le simulateur de glutt-o-matique 3000 permet de tester le fonctionnement de la
logique sur un PC sans matériel spécial. Il faut un PC sous linux avec les librairies
X11, GL, GLU et pulseaudio, et faire

    cd simulator
    make
    ./FreeRTOS-Sim

Compiler le projet STM32
========================

Il faut un compilateur pour arm-none-eabi-, qui doit être dans le $PATH. Il
faut openocd pour FLASHer le STM32F4-DISCOVERY.

    cd glutt-o-logique
    make
    make deploy
