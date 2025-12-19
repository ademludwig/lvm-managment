#!/bin/bash

# 1. Création des répertoires de montage définis dans les sources [1]
echo "Création des points de montage..."
sudo mkdir -p /point1 /point2 /point3 /point4

# 2. Simulation d'un disque physique (si vous n'avez pas de disque vide)
# Nous créons un fichier de 10 Go pour simuler un périphérique de stockage
truncate -s 10G /tmp/lvm_disk.img
sudo losetup /dev/loop0 /tmp/lvm_disk.img

# 3. Initialisation du Volume Physique (PV) et du Groupe de Volumes (VG)
# Le nom du VG doit être 'proj' comme spécifié dans les commandes C [2]
echo "Initialisation du LVM (VG: proj)..."
sudo pvcreate /dev/loop0
sudo vgcreate proj /dev/loop0

# 4. Création des 4 Volumes Logiques (LV) définis dans les sources [1]
# Taille initiale de 500Mo pour permettre de tester l'extension de +1G [2]
for i in {1..4}
do
    echo "Création de logic$i..."
    sudo lvcreate -L 500M -n logic$i proj
done

# 5. Formatage et Montage
# Le code utilise 'resize2fs', ce qui implique un système de fichiers ext4 [2]
for i in {1..4}
do
    echo "Formatage et montage de /dev/proj/logic$i sur /point$i..."
    sudo mkfs.ext4 /dev/proj/logic$i
    sudo mount /dev/proj/logic$i /point$i
done

# 6. Préparation du fichier journal [1]
sudo touch /var/log/lvm_monitor.log
sudo chmod 666 /var/log/lvm_monitor.log

echo "Architecture prête pour l'exécution du programme C."
