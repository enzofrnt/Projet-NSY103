# Communication Shell ↔ Noyau (IPC)

## Choix

| Lien | Mécanisme | Pourquoi |
|------|-----------|----------|
| Shell ↔ Noyau | **Pipes anonymes** + `fork()` | Relation père/fils naturelle ; simple et portable |
| Affichage | **stdout** du Shell | L’« écran » = le terminal hôte |

Pas de FIFO ni de socket Unix dans l’implémentation actuelle.

## Protocole

Deux structures dans `common/types.h`, **sans pointeurs** (sérialisables telles quelles) :

```text
request_t   type, argc, args[][], redirect, redir_path
response_t  status, output[], cwd_changed, new_cwd
```

Échanges via `ipc/ipc.c` :

- `ipc_send_request` / `ipc_recv_request`
- `ipc_send_response` / `ipc_recv_response`

## Point technique : lectures / écritures complètes

`sizeof(response_t)` est grand (~8 Ko à cause de `output`).  
`write(2)` / `read(2)` peuvent être **partiels** sur un pipe.  
D’où les helpers `write_full` / `read_full` qui bouclent jusqu’à transfert complet (ou erreur / EOF).

## Schéma

```mermaid
flowchart LR
    S[Shell père]
    N[Noyau fils]
    S -->|to_kernel request_t| N
    N -->|from_kernel response_t| S
```

Chaque côté **ferme** les extrémités inutiles juste après le `fork` (bon usage classique des pipes).
