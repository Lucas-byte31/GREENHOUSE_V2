# Procédure de mesures temps réel – approche WCET

*Étape 6 – GREENHOUSE V2.1 (branche `feature/freertos`) – 10/11/2026.*
*Références : System_spec_v2 (SR-GH-021 à 030), Conception_TR_v2 (§6 : valeurs estimées à confirmer).*

---

## 1. Objectif

Remplacer les durées d'exécution **estimées** de la conception par des valeurs **mesurées** sur la cible, puis vérifier par le calcul et par la mesure les exigences temporelles :

| Exigence | Grandeur vérifiée | Critère |
|---|---|---|
| SR-GH-021 | Gigue d'activation de `T_ACQ` | ≤ 1 ms |
| SR-GH-022 | Mesure → commande des actionneurs | ≤ 500 ms |
| SR-GH-023 | Appui → prise en compte / appui → écran à jour | < 100 ms / < 200 ms |
| SR-GH-028 | Charge U = Σ C/T sur le cœur 1 | ≤ 69 % |
| SR-GH-029 | WCET mesuré, temps de réponse R ≤ D | Marge ≥ 20 % |
| SR-GH-030 | Chien de garde | Redémarrage si une tâche est bloquée > 5 s |

## 2. La démarche WCET, à savoir expliquer

Le **WCET** (*Worst-Case Execution Time*) est le temps d'exécution maximal d'une tâche, quelles que soient ses données d'entrée et l'état du matériel. On ne le connaît jamais exactement. On l'encadre :

```
BCET  ≤  temps observés  ≤  max observé (MOET)  ≤  WCET réel  ≤  borne calculée (analyse statique)
```

| Approche | Principe | Avantage | Limite |
|---|---|---|---|
| **Analyse statique** | Modèle du code et du processeur (pipeline, cache), recherche du chemin le plus long | Borne **garantie** (aéronautique, DO-178C) | Outils coûteux (aiT, Rapita…), modèle du processeur nécessaire, pessimiste |
| **Mesure** (retenue) | On exécute le vrai code sur la vraie cible et on relève le maximum | Simple, réaliste, accessible | Donne le max **observé** : rien ne garantit qu'on a provoqué le vrai pire cas |
| Hybride | Mesure par blocs de code + combinaison par analyse du chemin | Compromis | Hors périmètre du projet |

**Pourquoi la mesure ici.** Il n'existe pas d'outil d'analyse statique accessible pour le cœur Xtensa de l'ESP32. Par ailleurs, la serre n'a que des contraintes **fermes et molles** : un dépassement exceptionnel ne met personne en danger.

**Comment rendre la mesure crédible :**
1. **Provoquer le pire cas** plutôt que l'attendre : redessins complets du LCD, appuis en rafale, capteurs débranchés, commutation des actionneurs (§5).
2. **Beaucoup d'échantillons** : au moins 1000 activations par tâche (SR-GH-029).
3. **Conserver les démarrages à froid.** Le code s'exécute depuis la flash à travers un cache de 32 ko : la première exécution d'une fonction est plus lente (défauts de cache). On ne retire pas ces valeurs, elles font partie du pire cas.
4. **Ajouter une marge** au maximum observé : WCET retenu = MOET × 1,2. Elle couvre ce que les essais n'ont pas provoqué, comme une interruption tombée au pire moment ou une configuration de cache défavorable.

**Sources de variabilité sur l'ESP32 :**
- défauts du cache flash ;
- interruptions : tick FreeRTOS à 1 kHz, I2C, GPIO, UART ;
- bus mémoire partagé avec le cœur 0 ;
- chemins différents dans le code (LCD qui redessine ou non, pompe en rampe ou non) ;
- attentes actives à l'intérieur de la librairie LCD_I2C (`delayMicroseconds`).

## 3. Matériel et outils

- Serre complète câblée, ESP32 relié en USB.
- Le **moniteur série de l'Arduino IDE** (mode d'emploi au §4.1).
- Un éditeur de texte (Bloc-notes) pour coller les rapports.
- Dossier de rangement des traces brutes : `GREENHOUSE_V2/05_MESURES/` (un fichier `.txt` par essai).

## 4. Configuration (fichier `rt_trace.h`)

| Paramètre | Rôle | Défaut (opérationnel) |
|---|---|---|
| `RT_TRACE` | Active l'instrumentation et le rapport toutes les 10 s | 0 |
| `RT_TRACE_GPIO` | Chaque tâche met son GPIO à 1 pendant son exécution (non utilisé : pas d'essai à l'oscilloscope) | 0 |
| `RT_STIMULUS` | Tâche `T_STIM` (priorité 1) qui injecte un appui PB1 toutes les 100 ms (`RT_STIMULUS_MS`) | 0 |

### 4.1 Utilisation du moniteur série de l'Arduino IDE

Le moniteur série de l'Arduino IDE **ne sait pas enregistrer dans un fichier**, et on ne peut copier que ce qui est visible à l'écran. Ce n'est pas gênant ici : les statistiques sont **cumulées depuis le démarrage**, donc seul le **dernier rapport** de chaque essai compte, et il tient sur une douzaine de lignes.

1. Ouvrir le moniteur : *Outils → Moniteur série* (`Ctrl + Maj + M`), et régler la vitesse sur **115200 bauds** (liste déroulante en haut à droite).
2. Appuyer sur **EN** (reset) de l'ESP32 pour démarrer l'essai : les compteurs repartent de zéro. Le message « BONJOUR LUCAS / GREENH V2 RTOS » reste affiché 2 s sur le LCD.
3. **Pas besoin de chronomètre.** Toutes les 10 s, `T_DIAG` imprime un rapport qui commence par `===== RT_TRACE @ xxx ms`, où `xxx` est le temps écoulé depuis le reset. Il sert d'horloge pour la conduite des essais.
4. En fin d'essai, dès que le rapport **`@ 120000 ms`** (2 min) est affiché :
   - désactiver le **défilement automatique** (icône *Toggle Autoscroll*, en haut à droite), sinon la sélection saute toutes les 10 s ;
   - sélectionner à la souris de la ligne `===== RT_TRACE @ 120000 ms =====` jusqu'à la ligne `Pile libre min ...`, puis `Ctrl + C` ;
   - coller dans le Bloc-notes et enregistrer sous `05_MESURES/<nom de l'essai>.txt`.
5. Réactiver le défilement automatique avant l'essai suivant.

### 4.2 Lecture du rapport

```
===== RT_TRACE @ 120000 ms =====
Tache          N  Cmin_us  Cmoy_us  Cmax_us   Gigue_us
T_ACQ       1200      ...      ...      ...        ...   <- Gigue = écart max à la période de 100 ms
...
Latence appui->prise en compte max : ... us
Latence appui->ecran a jour max    : ... us
File pleine : 0 | Mutex timeout : 0                       <- doivent rester à 0
Pile libre min (o) : ACQ ... | REG ... | ...              <- marge de pile restante (octets)
```

**Attention à l'interprétation de C.** La durée mesurée va de `rt_trace_begin` à `rt_trace_end`. Pour `T_ACQ`, la tâche la plus prioritaire, c'est directement son **temps d'exécution C**. Pour les autres tâches, cette durée **inclut les préemptions** par les tâches plus prioritaires : c'est un majorant de C. L'essai E4 permet d'isoler le vrai C de `T_LCD`, la seule tâche où l'écart est significatif.

## 5. Essais

> **Déroulé type d'un essai :** modifier la configuration → téléverser → ouvrir le moniteur série (115200) → appuyer sur EN → conduire l'essai en suivant l'horodatage `@ ... ms` des rapports → copier le rapport `@ 120000 ms` (§4.1).

| Essai | Config (`rt_trace.h` / `greenhv2.ino`) | Durée | Rapport à copier | Fichier |
|---|---|---|---|---|
| E1 nominal | `RT_TRACE 1`, `RT_STIMULUS 0` | 2 min | `@ 120000 ms` | `E1_nominal.txt` |
| E2 pire cas | `RT_TRACE 1`, `RT_STIMULUS 1` | 2 min | `@ 120000 ms` | `E2_pire_cas.txt` |
| E3 boutons | `RT_TRACE 1`, `RT_STIMULUS 0` | 2 min | `@ 120000 ms` | `E3_latences.txt` |
| E4 isolation LCD | comme E2 + `PRIO_LCD 7` | 2 min | `@ 120000 ms` | `E4_isolation_LCD.txt` |
| E6 chien de garde | `RT_TRACE 0` + code de test | ≈ 30 s | message `task_wdt` | `E6_watchdog.txt` |

Total : environ 10 min d'essais, soit une vingtaine de minutes en comptant les téléversements.

### E1 – Fonctionnement nominal (2 min)
- **Config :** `RT_TRACE 1`, `RT_STIMULUS 0`.
- **Conduite :** rester sur la page 1 et ne rien toucher jusqu'au rapport `@ 120000 ms` (ou plus) : l'horodatage du rapport sert de chronomètre.
- **Objectif :** ≥ 1000 activations pour `T_ACQ` et `T_REG` (environ 1200 en 2 min). On obtient le comportement « au repos » : peu de redessins, gigue de base. `T_LCD` et `T_DIAG` n'ont qu'environ 240 activations ici : leurs 1000 activations au pire cas sont obtenues en E2 et E4.
- **Fichier :** `E1_nominal.txt`.

### E2 – Pire cas IHM / LCD / régulation (2 min)
- **Config :** `RT_TRACE 1`, `RT_STIMULUS 1`.
- **Conduite :** le banc de stimulation fait défiler les pages toutes les 100 ms : chaque appui provoque un redessin complet du LCD, et `T_REG` est réveillée à l'entrée et à la sortie de la page 4. Pendant ce temps :
  - vers `@ 10000 ms`, débrancher la sonde de température 20 s (le défaut se confirme en 15 s), puis la rebrancher : diagnostic, ventilateur forcé ;
  - vers `@ 40000 ms`, débrancher la sonde d'humidité 20 s, puis la rebrancher ;
  - vers `@ 70000 ms`, serrer le TMP235 dans la main pour faire commuter le ventilateur ;
  - copier le rapport `@ 120000 ms`.
- **Objectif :** ≥ 1000 activations de `T_IHM` et de `T_LCD` (environ 1200 redessins complets en 2 min), en faisant passer le code par tous ses chemins. `T_DIAG` (environ 240 activations par essai) a un code court à chemin unique et ne pèse que 0,02 % de la charge : un échantillon plus réduit suffit, ce qui est justifié dans le rapport.
- **Fichier :** `E2_pire_cas.txt`.

### E3 – Latence réelle des boutons, interruption comprise (2 min)
- **Config :** `RT_TRACE 1`, `RT_STIMULUS 0`.
- **Conduite :** pendant les 2 min, environ 30 appuis sur chaque bouton, sur toutes les pages, dont **20 appuis rapides sur PB1** : les pages changent pendant que le LCD redessine encore. C'est le pire cas pour SR-GH-023. Copier le rapport `@ 120000 ms`.
- **Objectif :** latences appui → prise en compte et appui → écran à jour, mesurées depuis l'interruption. Le banc E2 ne mesure, lui, que depuis l'injection dans la file.
- **Fichier :** `E3_latences.txt`.

### E4 – Isolation du C de T_LCD, et démonstration de l'intérêt des priorités (2 min)
- **Config :** comme E2, et dans `greenhv2.ino` : `#define PRIO_LCD 7` (au-dessus de `T_ACQ`). **Essai de laboratoire uniquement : remettre 3 ensuite.**
- **Conduite :** ne rien toucher (le banc de stimulation fait défiler les pages), copier le rapport `@ 120000 ms`.
- **Objectif 1 :** `T_LCD` n'est plus préemptée par personne, donc sa durée mesurée vaut son vrai **C_LCD**. L'écart avec E2 donne l'interférence subie.
- **Objectif 2, pour l'oral :** la **gigue de `T_ACQ` doit exploser** jusqu'à environ la durée d'un redessin, soit quelques dizaines de ms, et ne plus respecter SR-GH-021. C'est la preuve par la mesure que l'ordre des priorités (Deadline Monotonic) est nécessaire.
- **Fichier :** `E4_isolation_LCD.txt`.

### E5 – Contrôle à l'oscilloscope
*Non retenu (décision du 10/11/2026) : l'instrumentation logicielle suffit pour l'approche WCET du projet.*

### E6 – Chien de garde (SR-GH-030)
- **Config :** `RT_TRACE 0`. Dans `T_DIAG`, ajouter **temporairement**, juste avant `vTaskDelayUntil` :
  ```cpp
  static int essai_wdt = 0;
  if (++essai_wdt == 20) { vTaskDelay(pdMS_TO_TICKS(8000)); }   // TEST WDT : blocage 8 s après ~10 s
  ```
- **Attendu :** environ 15 s après le reset (2 s de message de démarrage, environ 10 s de fonctionnement, puis 5 s de blocage détecté), le moniteur série affiche `task_wdt: Task watchdog got triggered` en citant `T_DIAG`, puis l'ESP32 redémarre. Ce message est affiché par ESP-IDF même avec `RT_TRACE 0`.
- **Conduite :** copier les lignes du message `task_wdt` et du redémarrage (moniteur série, §4.1).
- **Retirer le code de test ensuite.**
- **Fichier :** `E6_watchdog.txt`.

## 6. Exploitation (je la ferai à partir de tes fichiers)

1. **Tableau des C** : min, moyen et max observé, puis WCET retenu = max observé × 1,2, pour chaque tâche (E1, E2, E4).
2. **Nouveau calcul de la charge** U = Σ WCET/T, comparé à la borne de Liu & Layland.
3. **Nouveau calcul des temps de réponse pire cas** avec les valeurs mesurées, et comparaison aux échéances.
4. **Comparaison mesure / calcul** pour les latences (E3) et la gigue (E1, E2, E4).
5. **Piles** : taille finale = taille utilisée + 25 % de marge, et ajustement des `STACK_xxx`.
6. **Bilan par exigence** (§1) et rédaction de `Mesures_WCET_v2.md`, qui sera reprise dans le rapport.

## 7. Précautions

- **Instrumentation non intrusive :** avec `RT_TRACE 0`, toutes les fonctions de trace sont vides (aucun surcoût en exploitation). Avec `RT_TRACE 1`, le coût est d'environ 2 lectures d'horloge par activation (≈ 1 µs), négligeable devant les durées mesurées.
- **Le rapport série est imprimé par `T_DIAG` après sa mesure** : il ne fausse pas son C. Il ajoute en revanche de l'activité UART, qui fait partie des interférences réalistes.
- **Toujours remettre en configuration opérationnelle** après les essais : `RT_TRACE 0`, `RT_STIMULUS 0`, `PRIO_LCD 3`, code de test du chien de garde retiré. Commiter les traces dans `05_MESURES/`.
