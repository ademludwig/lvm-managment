introduction:
Ce document source est un programme en langage C qui établit un système automatisé de surveillance et d'extension pour les Volumes Logiques (LVM).
Le code crée plusieurs processus enfants via fork() : le writer_process génère automatiquement des fichiers de taille aléatoire sur quatre points de montage pour simuler l'utilisation de l'espace disque. 
Le supervisor_process vérifie l'utilisation de ces volumes toutes les cinq secondes et, si l'espace dépasse 90%, il envoie un signal via un pipe interprocessus et SIGUSR1 au processus d'extension. Enfin,
l'extender_process reçoit la demande, exécute des commandes shell (lvextend et resize2fs) pour augmenter la taille du volume d'un gigaoctet, et enregistre toutes les actions dans un fichier journal.

discraption:
La logique C pour déterminer quand et comment exécuter l'extension LVM est répartie entre deux processus principaux : le processus `supervisor_process` qui surveille et déclenche l'extension, et le processus `extender_process` qui exécute l'extension.

### Logique de Détermination (Quand l'extension doit être exécutée)

C'est le **`supervisor_process`** qui est responsable de la surveillance automatique.

1.  **Surveillance de l'utilisation:** Le superviseur parcourt tous les volumes logiques (LV) définis (il y en a quatre, nommés "logic1" à "logic4").
2.  **Vérification du pourcentage d'utilisation:** Pour chaque point de montage associé à un LV, le superviseur exécute la commande `df` pour obtenir le pourcentage d'utilisation du disque :
    ```c
    df --output=pcent %s 2>/dev/null | tail -1
    ```
    (Où `%s` est le point de montage du LV, par exemple `/point1`).
3.  **Condition de déclenchement:** L'extension est déclenchée si le pourcentage d'utilisation (`usage`) est **supérieur ou égal à 90%** (`if (usage >= 90)`).
4.  **Communication du déclenchement:** Si le seuil est atteint, le superviseur enregistre un message d'avertissement. Il transmet ensuite le nom du volume logique concerné (`lvs[i].lv_name`) via un *pipe* (`pipe_fd`) au processus d'extension. Il envoie enfin le signal `SIGUSR1` au processus `extender_pid` pour le réveiller et lui signaler qu'une extension est nécessaire.
5.  **Fréquence:** Cette vérification est effectuée toutes les 5 secondes (`sleep(5)`) pour tous les volumes.

### Logique d'Exécution (Comment l'extension est exécutée)

C'est le **`extender_process`** qui exécute l'opération LVM.

1.  **Réception du signal:** Le processus extenseur est initialement en attente (`pause()`) et est réveillé par le signal `SIGUSR1`. Ce signal active le drapeau `extend_requested`.
2.  **Lecture du volume cible:** Lorsque l'extension est demandée (`if (extend_requested)`), le processus lit le nom du LV à étendre depuis le *pipe* (`pipe_fd`).
3.  **Commande d'extension:** Le processus exécute une série de commandes système via `popen`. La commande spécifie deux actions :
    *   **Étendre le volume logique:** `lvextend -L +1G /dev/proj/%s 2>&1`. Cette commande augmente le volume logique (situé sous `/dev/proj/`) de **1 Gigaoctet (+1G)**.
    *   **Redimensionner le système de fichiers:** `&& resize2fs /dev/proj/%s 2>&1`. Ceci redimensionne le système de fichiers pour qu'il utilise l'espace nouvellement alloué au LV.
4.  **Journalisation:** Le processus enregistre l'opération, y compris la sortie des commandes exécutées, dans le fichier journal (`LOG_FILE`).

En résumé, le processus est comme un système d'alarme : le **superviseur** vérifie l'état de chaque porte (point de montage) toutes les 5 secondes et, si une porte atteint le niveau de danger (90% d'utilisation), il envoie un message (le nom du LV) et sonne l'alarme (SIGUSR1) au processus **extenseur**, qui prend alors l'outil nécessaire (`lvextend` et `resize2fs`) pour ajouter immédiatement 1G d'espace.
