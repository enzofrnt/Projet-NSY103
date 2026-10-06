#include "bitmap.h"
#include "disk.h"

#include <string.h>

static uint8_t g_inode_bm[BLOCK_SIZE];
static uint8_t g_block_bm[BLOCK_SIZE];

static int bit_get(const uint8_t *bm, uint32_t i)
{
    return (bm[i / 8] >> (i % 8)) & 1;
}

static void bit_set(uint8_t *bm, uint32_t i)
{
    bm[i / 8] |= (uint8_t)(1u << (i % 8));
}

static void bit_clear(uint8_t *bm, uint32_t i)
{
    bm[i / 8] &= (uint8_t)~(1u << (i % 8));
}

err_t bitmap_init(void)
{
    superblock_t *sb = disk_superblock();
    err_t e;

    e = disk_read_block(sb->inode_bitmap_block, g_inode_bm);
    if (e != ERR_OK)
        return e;
    e = disk_read_block(sb->block_bitmap_block, g_block_bm);
    if (e != ERR_OK)
        return e;
    return ERR_OK;
}

static err_t sync_inode_bm(void)
{
    return disk_write_block(disk_superblock()->inode_bitmap_block, g_inode_bm);
}

static err_t sync_block_bm(void)
{
    return disk_write_block(disk_superblock()->block_bitmap_block, g_block_bm);
}

err_t allocate_inode(uint32_t *out)
{
    superblock_t *sb = disk_superblock();
    uint32_t i;

    if (sb->free_inodes == 0)
        return ERR_NO_SPACE;

    for (i = 0; i < sb->total_inodes; i++) {
        if (!bit_get(g_inode_bm, i)) {
            bit_set(g_inode_bm, i);
            sb->free_inodes--;
            disk_mark_sb_dirty();
            if (sync_inode_bm() != ERR_OK)
                return ERR_IO;
            *out = i;
            return ERR_OK;
        }
    }
    return ERR_NO_SPACE;
}

err_t free_inode(uint32_t ino)
{
    superblock_t *sb = disk_superblock();
    if (ino >= sb->total_inodes)
        return ERR_INVALID_PATH;
    if (!bit_get(g_inode_bm, ino))
        return ERR_OK;
    bit_clear(g_inode_bm, ino);
    sb->free_inodes++;
    disk_mark_sb_dirty();
    return sync_inode_bm();
}

err_t allocate_block(uint32_t *out)
{
    superblock_t *sb = disk_superblock();
    uint32_t i;

    if (sb->free_blocks == 0)
        return ERR_NO_SPACE;

    for (i = 0; i < sb->total_blocks; i++) {
        if (!bit_get(g_block_bm, i)) {
            bit_set(g_block_bm, i);
            sb->free_blocks--;
            disk_mark_sb_dirty();
            if (sync_block_bm() != ERR_OK)
                return ERR_IO;
            *out = sb->data_start_block + i;
            return ERR_OK;
        }
    }
    return ERR_NO_SPACE;
}

err_t free_block(uint32_t blk)
{
    superblock_t *sb = disk_superblock();
    uint32_t idx;

    if (blk < sb->data_start_block)
        return ERR_INVALID_PATH;
    idx = blk - sb->data_start_block;
    if (idx >= sb->total_blocks)
        return ERR_INVALID_PATH;
    if (!bit_get(g_block_bm, idx))
        return ERR_OK;
    bit_clear(g_block_bm, idx);
    sb->free_blocks++;
    disk_mark_sb_dirty();
    return sync_block_bm();
}

int inode_is_used(uint32_t ino)
{
    if (ino >= disk_superblock()->total_inodes)
        return 0;
    return bit_get(g_inode_bm, ino);
}

int block_is_used(uint32_t blk)
{
    superblock_t *sb = disk_superblock();
    if (blk < sb->data_start_block)
        return 0;
    uint32_t idx = blk - sb->data_start_block;
    if (idx >= sb->total_blocks)
        return 0;
    return bit_get(g_block_bm, idx);
}
