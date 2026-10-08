# Système de gestion de fichiers (SGF)

## Support

Un fichier image binaire `data/disque.img` simule un disque dur (comme l’image d’une VM).  
Il est découpé en **blocs de 4096 octets**.

## Layout

```text
Bloc 0          Superbloc (magic, tailles, compteurs, offsets des zones)
Bloc 1          Bitmap des inodes   (0 = libre, 1 = utilisé)
Bloc 2          Bitmap des blocs de données
Blocs 3…        Table des inodes
Ensuite…        Zone de données
```

Le superbloc décrit où commence chaque zone ; les bitmaps permettent d’**allouer** et de **libérer** sans parcourir tout le disque.

## Méthodes d’allocation : alternatives et choix

| Méthode | Principe | Avantages | Inconvénients |
|---------|----------|-----------|---------------|
| **Contiguë** | Blocs collés d’affilée | Accès simple / rapide | Fragmentation, difficile d’étendre un fichier |
| **Chaînée** | Chaque bloc pointe vers le suivant | Pas de contiguïté requise | Accès au milieu = parcours ; fragile |
| **Indexée** | L’inode pointe vers les blocs | Accès direct au n-ième bloc | Structures un peu plus riches |

### Notre choix : **indexée**, 1 niveau d’indirection

Conformément au sujet (inspiration Linux, un seul niveau d’indirection) :

```text
inode
 ├── direct[0 .. 11]  →  jusqu’à 12 blocs de données (12 × 4 Ko)
 └── indirect         →  un bloc contenant une table de n° de blocs
```

- Les blocs d’un fichier **ne sont pas forcément contigus**.
- À la suppression (`rm`, `rmdir`), `inode_free_all_blocks` + bitmaps **recyclent** l’espace.
- Fonction centrale : `inode_get_block(file_block, alloc)` dans `fs/inode.c`.

## Inodes et répertoires

- Un **inode** décrit un fichier ou un répertoire (type, taille, droits, pointeurs).
- Un **répertoire** est un fichier spécial contenant des `dirent_t` : `nom → numéro d’inode`.
- La racine `/` (inode 0) est créée au formatage, avec les entrées `.` et `..`.

## Primitives exposées

Demandées par le sujet, dans `fs/fs.c` :

| Primitive | Rôle |
|-----------|------|
| `_mycreat` / `_myopen` / `_myclose` | Création / ouverture / fermeture |
| `_myread` / `_mywrite` | Lecture / écriture d’octets |
| `_mkdir` / `_rmdir` / `_unlink` | Répertoires et suppression d’entrée |

Le **cwd** (répertoire courant) est un état **virtuel** du processus Noyau (`fs_set_cwd` / `fs_get_cwd`), indépendant du cwd de la machine hôte.

## Chemins

`resolve_path` / `resolve_parent` (`fs/path.c`) convertissent un chemin absolu ou relatif en numéro d’inode, en suivant les `dirent` et en gérant `.` / `..`.
