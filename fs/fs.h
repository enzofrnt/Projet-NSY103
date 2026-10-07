#ifndef FS_H
#define FS_H

#include "../common/types.h"

/*
 * API haut niveau du SGF (appelée uniquement par le Noyau).
 * Le cwd (répertoire courant) est un état virtuel en mémoire du processus
 * Noyau — il ne change pas le cwd du système hôte.
 *
 * Layout disque.img : superbloc | bitmap inodes | bitmap blocs |
 *                     table inodes | blocs de données.
 */

err_t fs_format(const char *path);
err_t fs_mount(const char *path);
void  fs_unmount(void);

/* Primitives demandées par le sujet (retournent -err_t en cas d'échec). */
int32_t _mycreat(const char *nom, uint16_t mode);   /* crée un fichier, retourne l'inode */
int32_t _myopen(const char *nom, int mode);         /* ouvre/crée selon les flags MODE_* */
err_t   _myclose(uint32_t inode);                   /* ferme un descripteur logique */
int32_t _myread(uint32_t inode, void *buffer, uint32_t nombre);
int32_t _mywrite(uint32_t inode, const void *buffer, uint32_t nombre);
err_t   _mkdir(const char *nom);                    /* crée un répertoire */
err_t   _rmdir(const char *nom);                    /* supprime un répertoire vide */
err_t   _unlink(const char *nom);                   /* efface une entrée fichier */

/* Helpers exposés au noyau */
void     fs_set_cwd(uint32_t ino);
uint32_t fs_get_cwd(void);
err_t    fs_stat_df(char *out, size_t out_sz);
err_t    fs_resolve(const char *path, uint32_t *out);
err_t    fs_list(const char *path, char *out, size_t out_sz, int long_fmt);
err_t    fs_cat(const char *path, char *out, size_t out_sz);
err_t    fs_echo_write(const char *path, const char *text);
err_t    fs_cp(const char *src, const char *dst);
err_t    fs_mv(const char *src, const char *dst);
err_t    fs_pwd(char *out, size_t out_sz);

#endif
