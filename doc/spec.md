##Généralités

En mode secours, la glutt-o-matique doit être entièrement désactivée. Cela est
garanti en connectant le signal `SECOURS` sur l'entrée `RESET` du
microcontrôleur.

##Limites de tension pour QRP

Avec hysterèse:

 - en dessous de 12V5, passer en QRP
 - en dessus de 13V, passer en QRO

##Lettres CW

 * K : tout va bien (priorité 0)
 * G : relais en petite puissance (priorité 2)
 * U : RX trop haut en référence à 145'125 MHz (priorité 3)
 * D : RX trop bas 145'125 MHz (priorité 3)
 * S : mode SSTV (sans filtre 1725 Hz, priorité 1)
 * R : ROS sur antenne (priorité 4)

##Première phase : ouverture du relais
 * 1750 Hz pendant 200 ms, puis après relâchement de la porteuse :
 * TX ON, -> pause de 200ms, puis :
 * K,G,U,D,R (selon la situation du moment present du relais)

##Ensuite :

 * Pas de QSO, -> TX OFF après 6 s -> impossible de réentendre K, G, U, D, R pendant 15 s. Réouverture possible cependant du relais, pendant ce laps de temps de 15 s.
 * QSO -> le relais compte le temps d’un utilisateur, si plus de 5 min avec le même utilisateur, alors anti-bavard -> modulation OFF, et texte CW : HI HI, puis stop relais pendant 10 s, réouverture impossible avant 10 s
 * QSO normal de moins de 5 min. -> si plus d’action de PTT, fermeture du relais après 5 s, sans texte CW, puis idem :
 * QSO normal dépassant 5 min ->  texte CW : *73*, puis fermeture.
 * QSO normal dépassant 10 min ->  texte CW : *HB9G*, puis fermeture.
 * QSO normal dépassant 15 min ou plus -> texte CW au hasard, soit *HB9G JN36BG* ou *HB9G 1628 M*, puis fermeture

##Relais stand-by :
 * Pendant toutes les minutes que le relais n’est pas utilisé, après 20 minutes de silence consécutif, il TX une balise, au hasard, soit : *HB9G*, ou *HB9G JN36BK*, ou *HB9G 1628 M*, ou *HB9G JN36BK 1628 M*. Pendant ces balises, si un utilisateur utilise son PTT, il peut utiliser le relais comme si il avait fait un 1750 Hz, le relais réagit comme une ouverture normale.
 * Timer de balise : Commence à compter lors de l'arrêt du relais. Si qso, arrêt de timer avec mémoire du temps écoulé, puis reprise du comptage dès la fin du qso. Si qso plus long que 10 min. -> timer RAZ. Si timer = 20 min TX balise, puis RAZ
 * Si le relais en QRP ou ROS, plus aucune de ces balise sont en fonction
 * Une fois entrée en mode ROS, seul une coupure de courant ou un passage en SECOURS peut faire passer en mode normal.

##Toutes les heures paires :

Le relais, seulement s'il n'y a pas de QSO en cours, (sinon il attend la fin du QSO), dès qu’il a la possibilité, il prend la main sur toutes autres activités possible, (modulation off, et 1750Hz) puis il TX une balise.

A partir d'octobre 2020:

*HB9G JN36BK U 13V5  1650 AH + T 12  73*

Tendance: + - ou = selon l'évolution de la capacité en Ah. T par pas de 1 degré C. *73* si éolienne dans le vent, *SK*
si repliée.

En QRP, *HB9G U 12V5  1010 AH 73*


Jusqu'en octobre 2020:

*HB9G JN36BK 1628 M U 13V5* (un pas de 0,5V est suffisant) puis la tendance à la charge ou décharge ou égale (representés par +/-/=), *T* (température) par pas de 2 degrés, puis *73* si la queue de l’éolienne est dans le vent, ou *SK* si elle est repliée.

Ou, si le relais en en QRP, ou ROS, alors :

*HB9G U 12V5 73* ou *SK*
