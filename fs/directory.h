#ifndef DIRECTORY_H
#define DIRECTORY_H

#include "../common/types.h"

/*
 * Un répertoire = inode de type DIR dont les blocs contiennent des dirent_t
 * (nom[MAX_FILENAME] + numéro d'inode). Entrées "." / ".." créées à l'init.
 * Une entrée libre a name[0] == '\0' (tombstone pour réutilisation).
 */

err_t directory_find(uint32_t dir_ino, const char *name, uint32_t *out_ino);
err_t directory_add(uint32_t dir_ino, const char *name, uint32_t child_ino);
err_t directory_remove(uint32_t dir_ino, const char *name);
err_t directory_list(uint32_t dir_ino, char *out, size_t out_sz, int long_fmt);
err_t directory_is_empty(uint32_t dir_ino, int *empty);
err_t directory_init(uint32_t dir_ino, uint32_t parent_ino);

#endif
