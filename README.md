# Mon D.O.S. — Projet NSY103

Mini système d'exploitation en C : émulateur de système de gestion de fichiers (SGF) + mini-interpréteur de commandes de type Bash.

**Auteurs :** Enzo FOURNET & Soltan HAMADOUCHE — CNAM Toulouse (IPST)

## Documentation

La conception et le détail technique sont dans **[`doc/`](doc/README.md)** :

- [Architecture](doc/architecture.md)
- [SGF (allocation, layout)](doc/sgf.md)
- [IPC Shell ↔ Noyau](doc/ipc.md)
- [Commandes & dispatch](doc/commandes.md)
- [Docker](doc/docker.md)

## Architecture (résumé)

```
Terminal (stdout) ← Shell ↔(pipes)↔ Noyau → SGF → data/disque.img
```

| Module | Rôle |
|--------|------|
| `shell/` | Prompt, parsing, affichage des résultats sur **stdout** |
| `kernel/` | Processus fils (`fork`), table de dispatch, commandes |
| `fs/` | Disque virtuel, inodes, bitmaps, répertoires, primitives SGF |
| `ipc/` | Pipes anonymes Shell ↔ Noyau |
| `common/` | Types, structures de messages, codes d'erreur |

L’« écran » du sujet = le **terminal** qui affiche le `stdout` du Shell.

## Compilation & exécution

```bash
make          # → bin/shell
make run      # lance le Shell
```

### Docker (image minimale ~scratch)

```bash
make docker-build
make docker-run
# équivalent : docker run --rm -it -v "$(PWD)/data:/app/data" mdos-nsy103
```

## Commandes disponibles

| Commande | Description |
|----------|-------------|
| `ls` / `ls -l` | Liste le contenu d'un répertoire |
| `mkdir` / `rmdir` | Crée / supprime un répertoire vide |
| `cd` / `pwd` | Navigation |
| `cat` / `echo "texte" > fichier` | Lecture / écriture |
| `cp` / `mv` / `rm` | Copie, déplacement, suppression |
| `df` | Occupation du SGF |
| `help` / `exit` | Aide / quitter |

## Disque virtuel

Fichier `data/disque.img` (blocs de 4096 octets) : superbloc, bitmaps, table d’inodes, blocs de données (**allocation indexée**, 12 directs + 1 niveau d’indirection).  
Détails : [doc/sgf.md](doc/sgf.md).

## Scénario de validation

```text
mkdir docs
cd docs
echo "Bonjour" > notes.txt
cat notes.txt
cp notes.txt copie.txt
mv copie.txt archive.txt
ls -l
df
cd /
rm docs/notes.txt
rm docs/archive.txt
rmdir docs
df
exit
```

Ou : `./scripts/test_scenario.sh`
