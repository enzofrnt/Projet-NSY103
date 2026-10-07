#ifndef PATH_H
#define PATH_H

#include "../common/types.h"

/*
 * Résolution de chemins absolus (/a/b) ou relatifs (depuis le cwd virtuel
 * du Noyau — indépendant du cwd du processus hôte).
 * resolve_parent sépare "dossier parent" + "nom feuille" pour creat/mkdir/rm.
 */

err_t resolve_path(uint32_t cwd, const char *path, uint32_t *out_ino);
err_t resolve_parent(uint32_t cwd, const char *path, uint32_t *parent_ino,
                     char *leaf, size_t leaf_sz);

#endif
