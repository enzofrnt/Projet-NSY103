#ifndef INODE_H
#define INODE_H

#include "../common/types.h"

/*
 * Allocation indexée (style Unix, 1 niveau d'indirection) :
 *   inode.direct[0..11]  → blocs de données
 *   inode.indirect       → un bloc contenant une table de n° de blocs
 *
 * Ce n'est ni du contigu, ni du chaînage bloc→bloc.
 * inode_get_block(file_block, alloc) traduit un offset logique en bloc disque.
 */

err_t read_inode(uint32_t ino, inode_t *out);
err_t write_inode(const inode_t *in);
err_t inode_init_empty(inode_t *ino, uint32_t id, inode_type_t type, uint16_t mode);
err_t inode_get_block(inode_t *ino, uint32_t file_block, uint32_t *disk_block, int alloc);
err_t inode_free_all_blocks(inode_t *ino);

#endif
