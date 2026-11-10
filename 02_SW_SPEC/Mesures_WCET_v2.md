# Résultats des mesures temps réel – approche WCET

*Étape 6 – GREENHOUSE V2.1 (branche `feature/freertos`) – essais du 10/11/2026.*
*Traces brutes : `05_MESURES/E1…E6`. Procédure : `Procedure_Mesures_WCET_v2.md`. Conception : `Conception_TR_v2.md` §6.*

---

## 1. Validité des essais

| Essai | Durée | Activations | Anomalies (file pleine, mutex) | Valide |
|---|---|---|---|---|
| E1 nominal | 120 s | ACQ 1201, REG 1201, LCD 305, DIAG 241 | 0 / 0 | ✓ |
| E2 pire cas (banc 100 ms) | 120 s | IHM 1170, LCD 1378, REG 1669 | 0 / 0 | ✓ |
| E3 boutons réels | 120 s | IHM 311 appuis | 0 / 0 | ✓ |
| E4 isolation LCD (`PRIO_LCD 7`) | 120 s | LCD 1514 | 0 / 0 | ✓ (essai de laboratoire) |
| E5 oscilloscope | — | — | — | Non retenu |
| E6 chien de garde | — | — | — | ✓ |

**Contrôle de cohérence.** En E2, `T_REG` compte 1669 activations = 1201 mesures + 468 commandes. Le banc fait défiler 5 pages, et la régulation est réveillée à l'entrée et à la sortie de la page 4, soit 2 réveils tous les 5 appuis. 1170 appuis × 2/5 = 468 : le décompte tombe juste, ce qui valide aussi la logique de `T_IHM`.

## 2. Durées d'exécution mesurées (µs)

| Tâche | E1 nominal | E2 pire cas | E3 boutons | E4 LCD prio 7 | **Max observé (config. normale)** | **WCET retenu (× 1,2)** |
|---|---|---|---|---|---|---|
| T_ACQ | 1344 / 1347 / **1650** | 1338 / 1355 / 1643 | 1340 / 1353 / 1650 | 1339 / 1358 / 3882* | **1650** | **1980** |
| T_REG | 30 / 33 / 70 | 18 / 32 / **76** | 17 / 33 / 73 | 17 / 46 / 131* | **76** | **91** |
| T_IHM | — | 15 / 38 / 71 | 14 / 32 / **81** | 15 / 91 / 145* | **81** | **97** |
| T_LCD | 6 / 10736 / 28816 | 23052 / 28852 / 35566 | 7 / 22747 / **37077** | 8 / 27522 / **33267** | 37077 (avec préemptions) | **39 920** (sur C isolé = 33 267) |
| T_DIAG | 12 / 12 / 29 | 12 / 13 / 87 | 12 / 15 / **125** | 12 / 12 / 26 | **125** | **150** |

*min / moyenne / max. \* = E4 : priorités volontairement faussées, valeurs exclues du WCET (voir §5).*

**Lecture :**
- **`T_ACQ` est très stable** : 1,34 à 1,65 ms pour 32 conversions ADC, soit environ 42 µs par conversion `analogReadMilliVolts()`, étalonnage compris. L'écart de 0,3 ms entre min et max correspond aux défauts du cache flash et aux interruptions.
- **`T_LCD` a deux comportements très différents** : environ 10 µs quand rien n'a changé (pas de redessin, c'est le principe hérité de la V1), et 23 à 37 ms pour un redessin complet. Sa moyenne en E1 (10,7 ms) ne veut rien dire. Seul le pire cas compte.
- **`T_REG`, `T_IHM` et `T_DIAG` restent sous 130 µs.** Elles ne pèsent rien dans l'ordonnancement.

## 3. Comparaison mesure / analyse statique simplifiée (T_LCD)

**Modèle.** On lit le code de la librairie LCD_I2C. Un octet envoyé à l'écran = 4 transactions I2C (2 demi-octets × impulsion Enable) + 39 µs d'attentes codées. S'y ajoutent 41 µs après un caractère, 37 µs après un positionnement du curseur et 1600 µs après un `clear`. Une transaction I2C à 100 kHz dure environ 20 bits, soit 200 µs. On compte ensuite les octets de chaque page dans `lcd_page_manager.cpp` :

| Page | Octets (clear + curseurs + caractères) | Transactions I2C | Modèle statique |
|---|---|---|---|
| P3 Hum / Seuil | 1 + 2 + 20 | 92 | **21,8 ms** (min) |
| P4 Pompe / Ventil | 1 + 2 + 21 | 96 | 22,7 ms |
| P2 Temp / Seuil | 1 + 2 + 22 | 100 | 23,6 ms |
| P1 Temp / Hum + OK | 1 + 4 + 22 | 108 | 25,3 ms |
| P5 système OK | 1 + 2 + 25 | 112 | 26,2 ms |
| P5 en défaut | 1 + 2 + 27 | 120 | **28,0 ms** (max) |

| | Modèle statique | Mesure (E4, C isolé) |
|---|---|---|
| Redessin le plus court | 21,8 ms | 23,1 ms (min E2) |
| Redessin le plus long | 28,0 ms | **33,3 ms** |

**Interprétation, à présenter à l'oral :**
1. **L'ordre de grandeur est confirmé.** Le modèle simple explique 85 à 95 % du temps mesuré. **Environ 85 % de ce temps est du temps de bus I2C** et seulement 3,4 à 4 ms sont des attentes actives codées dans la librairie.
2. **La mesure dépasse le modèle de 1 à 7 ms.** Le modèle ignore le surcoût logiciel du driver I2C de l'ESP32 (préparation de chaque transaction, interruption de fin, réveil de la tâche), soit environ 15 à 60 µs par transaction selon les cas. **Un modèle « statique » incomplet n'est donc pas une borne sûre.** C'est exactement pourquoi la vraie analyse statique exige un modèle complet du matériel et du logiciel, et pourquoi on retient ici la mesure, avec une marge de 20 % qui couvre largement cet écart.
3. On encadre ainsi le WCET : **28,0 ms (modèle) ≤ 33,3 ms (max observé) ≤ 39,9 ms (WCET retenu)**.

`T_ACQ` n'a pas d'équivalent : la documentation ne donne pas de borne exploitable pour la conversion « one-shot » avec étalonnage (verrou du driver + calcul de correction). On s'en tient à la mesure, très stable.

## 4. Vérification des exigences

### 4.1 Charge processeur (SR-GH-028)

| Tâche | WCET retenu | T (ms) | U |
|---|---|---|---|
| T_ACQ | 1,98 ms | 100 | 1,98 % |
| T_REG | 0,091 ms | 100 (+ commandes ≥ 200) | 0,14 % |
| T_IHM | 0,097 ms | 200 | 0,05 % |
| T_LCD | 39,9 ms | 100 (pire cas) | 39,9 % |
| T_DIAG | 0,15 ms | 500 | 0,03 % |
| **Total** | | | **≈ 42,1 % ≤ 69 % ✓** |

Cette charge est **pessimiste** : les 39,9 ms du LCD sont surtout de l'attente du bus I2C, pendant laquelle le processeur est libre (§5). La charge réelle est nettement plus faible.

### 4.2 Temps de réponse pire cas (RTA) avec les WCET mesurés

| Tâche | Calcul | R | D | Marge |
|---|---|---|---|---|
| T_ACQ | 1,98 + B 0,01 | **1,99 ms** | 100 ms | 98 % |
| T_REG | 0,09 + 0,01 + 1,98 | **2,08 ms** | 100 ms | 98 % |
| T_IHM | 0,10 + 0,01 + 1,98 + 0,09 | **2,18 ms** | 100 ms | 98 % |
| T_LCD | 39,9 + 0,01 + 2,07 + 0,10 | **42,1 ms** | 200 ms | 79 % |
| T_DIAG | 0,15 + 2,07 + 0,10 + 39,9 | **42,2 ms** | 500 ms | 92 % |

Toutes les marges dépassent les 20 % de SR-GH-029 ✓. Les valeurs estimées à la conception (R_LCD = 36,2 ms) étaient du bon ordre. `T_ACQ` avait été sous-estimée (1,0 ms estimée contre 1,65 ms mesurée), sans conséquence.

### 4.3 Bilan par exigence

| Exigence | Critère | Mesuré | Calculé (borne) | Verdict |
|---|---|---|---|---|
| SR-GH-021 gigue acquisition | ≤ 1 ms | **0,22 ms** (E1-E4) | — | ✓ |
| SR-GH-022 mesure → actionneur | ≤ 500 ms | — | R_REG = 2,1 ms | ✓ |
| SR-GH-023 appui → prise en compte | < 100 ms | **1,05 ms** (E3, interruption comprise) | R_IHM = 2,2 ms | ✓ |
| SR-GH-023 appui → écran à jour | < 200 ms | **37,2 ms** (E3) | R_IHM + R_LCD = 44,3 ms | ✓ |
| SR-GH-028 charge | ≤ 69 % | — | 42 % (pessimiste) | ✓ |
| SR-GH-029 WCET et marge | ≥ 20 % | 1000+ activations, sauf T_DIAG (241, justifié) | marge min 79 % | ✓ |
| SR-GH-030 chien de garde | Redémarrage si blocage > 5 s | Déclenché, `T_DIAG (CPU 1)` identifiée, `rst:0xc (SW_CPU_RESET)`. Trace capturée au 2e cycle de redémarrage (≈ 2 × 15 s, d'où l'horodatage de 30,4 s) | — | ✓ |
| Files et mutex | 0 anomalie | 0 / 0 sur tous les essais | — | ✓ |

**Toutes les mesures sont sous les bornes calculées**, ce qui est attendu et cohérent : par exemple 1,05 ms mesurée contre 2,2 ms calculée pour la prise en compte d'un appui. La latence de 1,05 ms correspond à un appui tombé pendant une acquisition : `T_IHM` attend que `T_ACQ` (1,3 à 1,65 ms) se termine, comme prévu par l'analyse.

## 5. Le résultat inattendu de E4 – à exploiter à l'oral

**Prévision :** avec `T_LCD` au-dessus de `T_ACQ`, un redessin de 33 ms devait retarder l'acquisition de plusieurs dizaines de ms, et donc faire exploser sa gigue.

**Mesure :** la gigue reste à **0,22 ms**. En revanche, la durée de `T_ACQ` passe de 1,65 à **3,88 ms**, et celles de `T_REG` et `T_IHM` doublent.

**Explication.** Pendant un échange I2C, la tâche LCD **se bloque** en attendant l'interruption de fin de transaction du driver. Le processeur est alors libre et `T_ACQ` démarre à l'heure. À chaque fin de transaction, `T_LCD` (priorité 7) redevient prête et **préempte** `T_ACQ` pendant quelques dizaines de µs, d'où l'allongement de 1,65 à 3,88 ms au lieu d'un retard au démarrage. Le fait que `T_ACQ` ait été préemptée *en cours d'exécution* le prouve : `T_LCD` ne peut redevenir prête à ce moment-là qu'à la fin d'une transaction I2C qu'elle avait lancée avant.

**Ce qu'on en retient :**
- **Les 33 ms du LCD sont surtout de l'attente d'entrée-sortie, pas du calcul.** Compter 33 ms de charge processeur est très pessimiste : la vraie consommation processeur est de l'ordre de 4 à 9 ms (attentes actives + surcoût du driver).
- **L'inversion des priorités dégrade quand même le système** (l'acquisition dure plus de 2 fois plus longtemps), mais moins que prévu grâce aux entrées-sorties bloquantes.
- **Point de méthode :** la durée mesurée « début → fin » d'une tâche mélange calcul, attente d'entrées-sorties et préemptions. Pour l'analyse d'ordonnançabilité, il faudrait le temps processeur pur. Les statistiques d'exécution de FreeRTOS (`vTaskGetRunTimeStats`) le donneraient : c'est une piste d'amélioration à citer.
- **Une limite du banc de stimulation :** il a la même période que l'acquisition (100 ms) et reste calé sur la même phase. Chaque redessin démarre donc juste après une acquisition, ce qui réduit les chevauchements. Une période sans rapport avec 100 ms (par exemple 97 ms) ferait balayer toutes les phases. C'est une bonne pratique à mentionner.

## 6. Piles

| Tâche | Allouée | Libre min | Utilisée | + 25 % | Proposition |
|---|---|---|---|---|---|
| T_ACQ | 3072 | 2104 | 968 | 1210 | 2048 |
| T_REG | 3072 | 2368 | 704 | 880 | 2048 |
| T_IHM | 3072 | 2408 | 664 | 830 | 2048 |
| T_LCD | 4096 | 2432 | 1664 | 2080 | 3072 |
| T_DIAG | 4096 | 2032 | 2064 (rapport `printf` compris) | 2580 | 3072 |

Toutes les piles sont largement dimensionnées. La réduction proposée économiserait environ 5 ko de RAM, mais n'est pas nécessaire : l'ESP32 dispose de plus de 300 ko de RAM. **Recommandation : conserver les valeurs actuelles**, la marge étant un gage de robustesse, et citer ces chiffres dans le rapport.

## 7. Conclusion

L'architecture FreeRTOS respecte **toutes les exigences temporelles avec des marges supérieures à 79 %**. Les WCET mesurés confirment l'analyse de conception. L'écart mesure / modèle sur le LCD illustre les limites d'une analyse statique incomplète. L'essai E4 met en évidence l'effet des entrées-sorties bloquantes sur l'ordonnancement, une nuance que la théorie à calcul pur ne montre pas.
