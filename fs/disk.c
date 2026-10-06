#include "disk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

static int            g_fd = -1;
static superblock_t   g_sb;
static int            g_sb_dirty = 0;

err_t disk_open(const char *path, int create)
{
    if (g_fd >= 0)
        return ERR_BUSY;

    if (create) {
        g_fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
        if (g_fd < 0)
            return ERR_IO;
        return ERR_OK;
    }

    g_fd = open(path, O_RDWR);
    if (g_fd < 0)
        return ERR_IO;

    if (pread(g_fd, &g_sb, sizeof(g_sb), 0) != (ssize_t)sizeof(g_sb)) {
        close(g_fd);
        g_fd = -1;
        return ERR_IO;
    }
    if (g_sb.magic != FS_MAGIC) {
        close(g_fd);
        g_fd = -1;
        return ERR_IO;
    }
    g_sb_dirty = 0;
    return ERR_OK;
}

void disk_close(void)
{
    if (g_fd < 0)
        return;
    if (g_sb_dirty)
        disk_sync_superblock();
    close(g_fd);
    g_fd = -1;
    g_sb_dirty = 0;
}

int disk_is_open(void)
{
    return g_fd >= 0;
}

err_t disk_read_block(uint32_t block_num, void *buf)
{
    if (g_fd < 0)
        return ERR_IO;
    off_t off = (off_t)block_num * BLOCK_SIZE;
    if (pread(g_fd, buf, BLOCK_SIZE, off) != (ssize_t)BLOCK_SIZE)
        return ERR_IO;
    return ERR_OK;
}

err_t disk_write_block(uint32_t block_num, const void *buf)
{
    if (g_fd < 0)
        return ERR_IO;
    off_t off = (off_t)block_num * BLOCK_SIZE;
    if (pwrite(g_fd, buf, BLOCK_SIZE, off) != (ssize_t)BLOCK_SIZE)
        return ERR_IO;
    return ERR_OK;
}

superblock_t *disk_superblock(void)
{
    return &g_sb;
}

err_t disk_sync_superblock(void)
{
    err_t e = disk_write_block(0, &g_sb);
    if (e == ERR_OK)
        g_sb_dirty = 0;
    return e;
}

/* Marque le superbloc comme modifié (appelé depuis bitmap/fs) */
void disk_mark_sb_dirty(void)
{
    g_sb_dirty = 1;
}
