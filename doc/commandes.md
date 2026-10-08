# Commandes et table de dispatch

## Principe

Éviter un long `switch` / enchaînement de `if` dans le Noyau.

Une **table** (`g_table` dans `kernel/commands.c`) associe :

| Champ | Rôle |
|-------|------|
| `type` | Enum `cmd_type_t` (identifiant binaire) |
| `name` | Nom textuel (`"ls"`, `"mkdir"`, …) |
| `min_args` / `max_args` | Documentation d’arité |
| `help` | Texte pour `help` |
| `handler` | Pointeur de fonction `cmd_*` |

## Deux usages, une seule table

```text
Shell  :  "ls"  ──cmd_type_from_name──►  CMD_LS     (dans la request_t)
Noyau  :  CMD_LS ──cmd_dispatch────────►  cmd_ls()
```

Le parser (`shell/parser.c`) **ne duplique pas** la liste des noms : il interroge la table.  
Ajouter une commande = **une ligne** dans `g_table` + le handler associé.

## Différence avec un « vrai » shell

| Mon D.O.S. | Bash typique |
|------------|--------------|
| Handler C dans le Noyau | Souvent `fork` + `exec` d’un binaire (`/bin/ls`) |
| Une table de pointeurs | PATH, exécutables externes, builtins |

Ici, l’objectif pédagogique est un **mini-noyau** qui orchestre le SGF, pas un lanceur de programmes externes.

## Commandes supportées

`ls` (`-l`), `mkdir`, `rmdir`, `cd`, `pwd`, `cat`, `echo` (+ redirection `>`), `cp`, `mv`, `rm`, `df`, `help`, `exit` (alias `quit`).

Le parser gère aussi les **guillemets** et l’opérateur `>` (ex. `echo "bonjour" > f.txt`).
