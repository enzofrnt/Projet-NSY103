#ifndef BITMAP_H
#define BITMAP_H

#include "../common/types.h"

/*
 * Allocation : bitmaps persistées sur le disque (0 = libre, 1 = occupé).
 * allocate_* / free_* mettent à jour la bitmap + compteurs du superbloc.
 * Les numéros de blocs donnés au reste du SGF sont des adresses absolues
 * sur disque.img (data_start + index bitmap).
 */

err_t bitmap_init(void);
err_t allocate_inode(uint32_t *out);
err_t free_inode(uint32_t ino);
err_t allocate_block(uint32_t *out);
err_t free_block(uint32_t blk);
int   inode_is_used(uint32_t ino);
int   block_is_used(uint32_t blk);

#endif
