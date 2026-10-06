#ifndef DISK_H
#define DISK_H

#include "../common/types.h"

/*
 * Couche bas niveau : disque.img vu comme un tableau de blocs fixes.
 * Accès via pread/pwrite (position indépendante du offset fichier courant).
 */

err_t disk_open(const char *path, int create);
void  disk_close(void);
int   disk_is_open(void);

err_t disk_read_block(uint32_t block_num, void *buf);
err_t disk_write_block(uint32_t block_num, const void *buf);

superblock_t *disk_superblock(void);
err_t disk_sync_superblock(void);
void  disk_mark_sb_dirty(void);

#endif
