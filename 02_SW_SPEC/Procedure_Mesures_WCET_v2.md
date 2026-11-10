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
- Un terminal série **qui enregistre dans un fichier**, réglé à 115200 bauds : PuTTY (*Session → Logging → All session output*) ou CoolTerm. Le moniteur série de l'Arduino IDE convient aussi, mais il faut copier-coller le texte.
- *Optionnel :* oscilloscope ou analyseur logique (un clone 8 voies à environ 10 € avec PulseView suffit) sur les GPIO de trace : **4** (T_ACQ), **5** (T_REG), **13** (T_IHM), **14** (T_LCD), **27** (T_DIAG), plus une voie sur la broche d'un bouton.
- Dossier de rangement des traces brutes : `GREENHOUSE_V2/05_MESURES/` (un fichier `.txt` par essai).

## 4. Configuration (fichier `rt_trace.h`)

| Paramètre | Rôle | Défaut (opérationnel) |
|---|---|---|
| `RT_TRACE` | Active l'instrumentation et le rapport toutes les 10 s | 0 |
| `RT_TRACE_GPIO` | Chaque tâche met son GPIO à 1 pendant son exécution | 0 |
| `RT_STIMULUS` | Tâche `T_STIM` (priorité 1) qui injecte un appui PB1 toutes les 250 ms | 0 |

**Lecture du rapport.** Il est imprimé par `T_DIAG` toutes les 10 s. Les statistiques sont **cumulées depuis le démarrage** : pour chaque essai, on relève le **dernier** rapport. On appuie sur **EN** (reset) entre deux essais pour remettre les compteurs à zéro.

```
===== RT_TRACE @ 600000 ms =====
Tache          N  Cmin_us  Cmoy_us  Cmax_us   Gigue_us
T_ACQ       6000      ...      ...      ...        ...   <- Gigue = écart max à la période de 100 ms
...
Latence appui->prise en compte max : ... us
Latence appui->ecran a jour max    : ... us
File pleine : 0 | Mutex timeout : 0                       <- doivent rester à 0
Pile libre min (o) : ACQ ... | REG ... | ...              <- marge de pile restante (octets)
```

**Attention à l'interprétation de C.** La durée mesurée va de `rt_trace_begin` à `rt_trace_end`. Pour `T_ACQ`, la tâche la plus prioritaire, c'est directement son **temps d'exécution C**. Pour les autres tâches, cette durée **inclut les préemptions** par les tâches plus prioritaires : c'est un majorant de C. L'essai E4 permet d'isoler le vrai C de `T_LCD`, la seule tâche où l'écart est significatif.

## 5. Essais

> Avant chaque essai : modifier la configuration, téléverser, ouvrir le terminal avec enregistrement, appuyer sur EN.

### E1 – Fonctionnement nominal (≈ 10 min)
- **Config :** `RT_TRACE 1`, `RT_STIMULUS 0`.
- **Conduite :** rester sur la page 1, ne rien toucher pendant 10 min.
- **Objectif :** ≥ 1000 activations pour `T_ACQ` (100 s suffisent), `T_REG`, `T_DIAG` et `T_LCD` (500 ms × 1000 ≈ 8,3 min). On obtient le comportement « au repos » : peu de redessins, gigue de base.
- **Fichier :** `E1_nominal.txt`.

### E2 – Pire cas IHM / LCD / régulation (≈ 10 min)
- **Config :** `RT_TRACE 1`, `RT_STIMULUS 1`.
- **Conduite :** le banc de stimulation fait défiler les pages toutes les 250 ms : chaque appui provoque un redessin complet du LCD, et `T_REG` est réveillée à l'entrée et à la sortie de la page 4. Pendant ce temps :
  - à t ≈ 2 min, débrancher la sonde de température 30 s, puis la rebrancher : diagnostic, ventilateur forcé ;
  - à t ≈ 4 min, débrancher la sonde d'humidité 30 s, puis la rebrancher ;
  - à t ≈ 6 min, souffler de l'air chaud sur le TMP235 ou le serrer dans la main pour faire commuter le ventilateur.
- **Objectif :** ≥ 1000 activations de `T_IHM` et de `T_LCD` (≈ 4 min), en faisant passer le code par tous ses chemins.
- **Fichier :** `E2_pire_cas.txt`.

### E3 – Latence réelle des boutons, interruption comprise (≈ 5 min)
- **Config :** `RT_TRACE 1`, `RT_STIMULUS 0`.
- **Conduite :** environ 50 appuis sur chaque bouton, sur toutes les pages, dont **20 appuis rapides sur PB1** : les pages changent pendant que le LCD redessine encore. C'est le pire cas pour SR-GH-023.
- **Objectif :** latences appui → prise en compte et appui → écran à jour, mesurées depuis l'interruption. Le banc E2 ne mesure, lui, que depuis l'injection dans la file.
- **Fichier :** `E3_latences.txt`.

### E4 – Isolation du C de T_LCD, et démonstration de l'intérêt des priorités (≈ 5 min)
- **Config :** comme E2, et dans `greenhv2.ino` : `#define PRIO_LCD 7` (au-dessus de `T_ACQ`). **Essai de laboratoire uniquement : remettre 3 ensuite.**
- **Objectif 1 :** `T_LCD` n'est plus préemptée par personne, donc sa durée mesurée vaut son vrai **C_LCD**. L'écart avec E2 donne l'interférence subie.
- **Objectif 2, pour l'oral :** la **gigue de `T_ACQ` doit exploser** jusqu'à environ la durée d'un redessin, soit quelques dizaines de ms, et ne plus respecter SR-GH-021. C'est la preuve par la mesure que l'ordre des priorités (Deadline Monotonic) est nécessaire.
- **Fichier :** `E4_isolation_LCD.txt`.

### E5 – Contrôle à l'oscilloscope (optionnel, recommandé)
- **Config :** `RT_TRACE 1`, `RT_TRACE_GPIO 1`, `RT_STIMULUS 1`.
- **Mesures :**
  - largeur d'impulsion de chaque GPIO (min, max, en persistance) à comparer aux valeurs logicielles : l'écart donne le coût de l'instrumentation ;
  - période et gigue de la broche 4 (`T_ACQ`) ;
  - broche d'un bouton (front descendant) jusqu'à la broche 13 (`T_IHM`) : latence matérielle complète ;
  - chronogramme réel du cœur 1, à mettre en face du chronogramme théorique de la conception (§6.3). C'est une capture parfaite pour le rapport.
- **Fichier :** captures d'écran `E5_*.png`.

### E6 – Chien de garde (SR-GH-030)
- **Config :** `RT_TRACE 0`. Dans `T_DIAG`, ajouter **temporairement**, juste avant `vTaskDelayUntil` :
  ```cpp
  static int essai_wdt = 0;
  if (++essai_wdt == 20) { vTaskDelay(pdMS_TO_TICKS(8000)); }   // TEST WDT : blocage 8 s après ~10 s
  ```
- **Attendu :** environ 15 s après le démarrage, le moniteur série affiche `task_wdt: Task watchdog got triggered` en citant `T_DIAG`, puis l'ESP32 redémarre.
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

- **Instrumentation non intrusive :** avec `RT_TRACE 0`, toutes les fonctions de trace sont vides (aucun surcoût en exploitation). Avec `RT_TRACE 1`, le coût est d'environ 2 lectures d'horloge par activation (≈ 1 µs), à vérifier en E5.
- **Le rapport série est imprimé par `T_DIAG` après sa mesure** : il ne fausse pas son C. Il ajoute en revanche de l'activité UART, qui fait partie des interférences réalistes.
- **Toujours remettre en configuration opérationnelle** après les essais : `RT_TRACE 0`, `RT_STIMULUS 0`, `PRIO_LCD 3`, code de test du chien de garde retiré. Commiter les traces dans `05_MESURES/`.
