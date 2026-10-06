#include "inode.h"
#include "disk.h"
#include "bitmap.h"

#include <string.h>

/* Plusieurs inodes par bloc */
static uint32_t inodes_per_block(void)
{
    return BLOCK_SIZE / sizeof(inode_t);
}

err_t read_inode(uint32_t ino, inode_t *out)
{
    superblock_t *sb = disk_superblock();
    uint8_t buf[BLOCK_SIZE];
    uint32_t ipb, blk, idx;
    err_t e;

    if (ino >= sb->total_inodes)
        return ERR_INVALID_PATH;

    ipb = inodes_per_block();
    blk = sb->inode_table_block + (ino / ipb);
    idx = ino % ipb;

    e = disk_read_block(blk, buf);
    if (e != ERR_OK)
        return e;

    memcpy(out, buf + idx * sizeof(inode_t), sizeof(inode_t));
    return ERR_OK;
}

err_t write_inode(const inode_t *in)
{
    superblock_t *sb = disk_superblock();
    uint8_t buf[BLOCK_SIZE];
    uint32_t ipb, blk, idx;
    err_t e;

    if (in->id >= sb->total_inodes)
        return ERR_INVALID_PATH;

    ipb = inodes_per_block();
    blk = sb->inode_table_block + (in->id / ipb);
    idx = in->id % ipb;

    e = disk_read_block(blk, buf);
    if (e != ERR_OK)
        return e;

    memcpy(buf + idx * sizeof(inode_t), in, sizeof(inode_t));
    return disk_write_block(blk, buf);
}

err_t inode_init_empty(inode_t *ino, uint32_t id, inode_type_t type, uint16_t mode)
{
    uint32_t i;
    memset(ino, 0, sizeof(*ino));
    ino->id = id;
    ino->type = (uint16_t)type;
    ino->mode = mode;
    ino->size = 0;
    ino->nlinks = 1;
    for (i = 0; i < DIRECT_BLOCKS; i++)
        ino->direct[i] = 0;
    ino->indirect = 0;
    return ERR_OK;
}

/*
 * Traduit un numéro de bloc logique (dans le fichier) en adresse disque.
 * Si alloc != 0, alloue le bloc (et éventuellement le bloc d'indirection).
 */
err_t inode_get_block(inode_t *ino, uint32_t file_block, uint32_t *disk_block, int alloc)
{
    uint8_t buf[BLOCK_SIZE];
    uint32_t *ptrs;
    uint32_t max_indirect = BLOCK_SIZE / sizeof(uint32_t);
    err_t e;

    if (file_block < DIRECT_BLOCKS) {
        if (ino->direct[file_block] == 0) {
            if (!alloc)
                return ERR_NOT_FOUND;
            e = allocate_block(&ino->direct[file_block]);
            if (e != ERR_OK)
                return e;
            memset(buf, 0, BLOCK_SIZE);
            e = disk_write_block(ino->direct[file_block], buf);
            if (e != ERR_OK)
                return e;
            e = write_inode(ino);
            if (e != ERR_OK)
                return e;
        }
        *disk_block = ino->direct[file_block];
        return ERR_OK;
    }

    file_block -= DIRECT_BLOCKS;
    if (file_block >= max_indirect)
        return ERR_NO_SPACE;

    if (ino->indirect == 0) {
        if (!alloc)
            return ERR_NOT_FOUND;
        e = allocate_block(&ino->indirect);
        if (e != ERR_OK)
            return e;
        memset(buf, 0, BLOCK_SIZE);
        e = disk_write_block(ino->indirect, buf);
        if (e != ERR_OK)
            return e;
        e = write_inode(ino);
        if (e != ERR_OK)
            return e;
    }

    e = disk_read_block(ino->indirect, buf);
    if (e != ERR_OK)
        return e;

    ptrs = (uint32_t *)buf;
    if (ptrs[file_block] == 0) {
        if (!alloc)
            return ERR_NOT_FOUND;
        e = allocate_block(&ptrs[file_block]);
        if (e != ERR_OK)
            return e;
        {
            uint8_t z[BLOCK_SIZE];
            memset(z, 0, BLOCK_SIZE);
            e = disk_write_block(ptrs[file_block], z);
            if (e != ERR_OK)
                return e;
        }
        e = disk_write_block(ino->indirect, buf);
        if (e != ERR_OK)
            return e;
    }
    *disk_block = ptrs[file_block];
    return ERR_OK;
}

err_t inode_free_all_blocks(inode_t *ino)
{
    uint8_t buf[BLOCK_SIZE];
    uint32_t *ptrs;
    uint32_t i, max_indirect;
    err_t e;

    for (i = 0; i < DIRECT_BLOCKS; i++) {
        if (ino->direct[i]) {
            e = free_block(ino->direct[i]);
            if (e != ERR_OK)
                return e;
            ino->direct[i] = 0;
        }
    }

    if (ino->indirect) {
        e = disk_read_block(ino->indirect, buf);
        if (e != ERR_OK)
            return e;
        ptrs = (uint32_t *)buf;
        max_indirect = BLOCK_SIZE / sizeof(uint32_t);
        for (i = 0; i < max_indirect; i++) {
            if (ptrs[i]) {
                e = free_block(ptrs[i]);
                if (e != ERR_OK)
                    return e;
            }
        }
        e = free_block(ino->indirect);
        if (e != ERR_OK)
            return e;
        ino->indirect = 0;
    }

    ino->size = 0;
    return write_inode(ino);
}
