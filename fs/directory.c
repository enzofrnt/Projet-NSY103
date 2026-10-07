#include "directory.h"
#include "disk.h"
#include "inode.h"

#include <stdio.h>
#include <string.h>

#define DIRENTS_PER_BLOCK (BLOCK_SIZE / sizeof(dirent_t))

static err_t dir_read_entry(inode_t *dir, uint32_t index, dirent_t *ent, int *valid)
{
    uint32_t fblk = index / DIRENTS_PER_BLOCK;
    uint32_t off  = index % DIRENTS_PER_BLOCK;
    uint32_t dblk;
    uint8_t buf[BLOCK_SIZE];
    err_t e;

    *valid = 0;
    if ((index + 1) * sizeof(dirent_t) > dir->size && dir->size > 0) {
        /* peut encore lire dans le dernier bloc partiel */
        if (index * sizeof(dirent_t) >= dir->size)
            return ERR_NOT_FOUND;
    }
    if (index * sizeof(dirent_t) >= dir->size)
        return ERR_NOT_FOUND;

    e = inode_get_block(dir, fblk, &dblk, 0);
    if (e != ERR_OK)
        return e;
    e = disk_read_block(dblk, buf);
    if (e != ERR_OK)
        return e;
    memcpy(ent, buf + off * sizeof(dirent_t), sizeof(dirent_t));
    *valid = (ent->name[0] != '\0');
    return ERR_OK;
}

static err_t dir_write_entry(inode_t *dir, uint32_t index, const dirent_t *ent)
{
    uint32_t fblk = index / DIRENTS_PER_BLOCK;
    uint32_t off  = index % DIRENTS_PER_BLOCK;
    uint32_t dblk;
    uint8_t buf[BLOCK_SIZE];
    uint32_t needed;
    err_t e;

    e = inode_get_block(dir, fblk, &dblk, 1);
    if (e != ERR_OK)
        return e;
    e = disk_read_block(dblk, buf);
    if (e != ERR_OK)
        return e;
    memcpy(buf + off * sizeof(dirent_t), ent, sizeof(dirent_t));
    e = disk_write_block(dblk, buf);
    if (e != ERR_OK)
        return e;

    needed = (index + 1) * (uint32_t)sizeof(dirent_t);
    if (needed > dir->size) {
        dir->size = needed;
        e = write_inode(dir);
        if (e != ERR_OK)
            return e;
    }
    return ERR_OK;
}

err_t directory_init(uint32_t dir_ino, uint32_t parent_ino)
{
    inode_t dir;
    dirent_t ent;
    err_t e;

    e = read_inode(dir_ino, &dir);
    if (e != ERR_OK)
        return e;

    memset(&ent, 0, sizeof(ent));
    strncpy(ent.name, ".", MAX_FILENAME - 1);
    ent.inode = dir_ino;
    e = dir_write_entry(&dir, 0, &ent);
    if (e != ERR_OK)
        return e;

    e = read_inode(dir_ino, &dir);
    if (e != ERR_OK)
        return e;

    memset(&ent, 0, sizeof(ent));
    strncpy(ent.name, "..", MAX_FILENAME - 1);
    ent.inode = parent_ino;
    return dir_write_entry(&dir, 1, &ent);
}

err_t directory_find(uint32_t dir_ino, const char *name, uint32_t *out_ino)
{
    inode_t dir;
    dirent_t ent;
    uint32_t i, n;
    int valid;
    err_t e;

    e = read_inode(dir_ino, &dir);
    if (e != ERR_OK)
        return e;
    if (dir.type != INODE_DIR)
        return ERR_BAD_TYPE;

    n = dir.size / sizeof(dirent_t);
    for (i = 0; i < n; i++) {
        e = dir_read_entry(&dir, i, &ent, &valid);
        if (e != ERR_OK)
            return e;
        if (valid && strcmp(ent.name, name) == 0) {
            *out_ino = ent.inode;
            return ERR_OK;
        }
    }
    return ERR_NOT_FOUND;
}

err_t directory_add(uint32_t dir_ino, const char *name, uint32_t child_ino)
{
    inode_t dir;
    dirent_t ent;
    uint32_t i, n, slot;
    int valid, found_slot;
    err_t e;

    if (!name || name[0] == '\0' || strlen(name) >= MAX_FILENAME)
        return ERR_INVALID_PATH;

    e = read_inode(dir_ino, &dir);
    if (e != ERR_OK)
        return e;
    if (dir.type != INODE_DIR)
        return ERR_BAD_TYPE;

    /* Existe déjà ? */
    e = directory_find(dir_ino, name, &slot);
    if (e == ERR_OK)
        return ERR_EXISTS;
    if (e != ERR_NOT_FOUND)
        return e;

    n = dir.size / sizeof(dirent_t);
    found_slot = 0;
    slot = n;
    for (i = 0; i < n; i++) {
        e = dir_read_entry(&dir, i, &ent, &valid);
        if (e != ERR_OK)
            return e;
        if (!valid) {
            slot = i;
            found_slot = 1;
            break;
        }
    }

    memset(&ent, 0, sizeof(ent));
    strncpy(ent.name, name, MAX_FILENAME - 1);
    ent.inode = child_ino;

    e = read_inode(dir_ino, &dir);
    if (e != ERR_OK)
        return e;
    e = dir_write_entry(&dir, slot, &ent);
    if (e != ERR_OK)
        return e;

    (void)found_slot;
    return ERR_OK;
}

err_t directory_remove(uint32_t dir_ino, const char *name)
{
    inode_t dir;
    dirent_t ent;
    uint32_t i, n;
    int valid;
    err_t e;

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return ERR_INVALID_PATH;

    e = read_inode(dir_ino, &dir);
    if (e != ERR_OK)
        return e;
    if (dir.type != INODE_DIR)
        return ERR_BAD_TYPE;

    n = dir.size / sizeof(dirent_t);
    for (i = 0; i < n; i++) {
        e = dir_read_entry(&dir, i, &ent, &valid);
        if (e != ERR_OK)
            return e;
        if (valid && strcmp(ent.name, name) == 0) {
            memset(&ent, 0, sizeof(ent));
            e = read_inode(dir_ino, &dir);
            if (e != ERR_OK)
                return e;
            return dir_write_entry(&dir, i, &ent);
        }
    }
    return ERR_NOT_FOUND;
}

err_t directory_is_empty(uint32_t dir_ino, int *empty)
{
    inode_t dir;
    dirent_t ent;
    uint32_t i, n;
    int valid;
    err_t e;

    e = read_inode(dir_ino, &dir);
    if (e != ERR_OK)
        return e;

    *empty = 1;
    n = dir.size / sizeof(dirent_t);
    for (i = 0; i < n; i++) {
        e = dir_read_entry(&dir, i, &ent, &valid);
        if (e != ERR_OK)
            return e;
        if (valid && strcmp(ent.name, ".") != 0 && strcmp(ent.name, "..") != 0) {
            *empty = 0;
            return ERR_OK;
        }
    }
    return ERR_OK;
}

err_t directory_list(uint32_t dir_ino, char *out, size_t out_sz, int long_fmt)
{
    inode_t dir, child;
    dirent_t ent;
    uint32_t i, n;
    int valid;
    size_t used = 0;
    err_t e;
    char line[160];
    int nwrite;

    e = read_inode(dir_ino, &dir);
    if (e != ERR_OK)
        return e;
    if (dir.type != INODE_DIR)
        return ERR_BAD_TYPE;

    out[0] = '\0';
    n = dir.size / sizeof(dirent_t);
    for (i = 0; i < n; i++) {
        e = dir_read_entry(&dir, i, &ent, &valid);
        if (e != ERR_OK)
            return e;
        if (!valid)
            continue;

        if (long_fmt) {
            e = read_inode(ent.inode, &child);
            if (e != ERR_OK)
                return e;
            nwrite = snprintf(line, sizeof(line), "%c%c%c%c%c%c%c %5u %8u %s\n",
                child.type == INODE_DIR ? 'd' : '-',
                (child.mode & PERM_UREAD)  ? 'r' : '-',
                (child.mode & PERM_UWRITE) ? 'w' : '-',
                (child.mode & PERM_UEXEC)  ? 'x' : '-',
                (child.mode & PERM_OREAD)  ? 'r' : '-',
                (child.mode & PERM_OWRITE) ? 'w' : '-',
                (child.mode & PERM_OEXEC)  ? 'x' : '-',
                ent.inode, child.size, ent.name);
        } else {
            nwrite = snprintf(line, sizeof(line), "%s\n", ent.name);
        }
        if (nwrite < 0)
            return ERR_IO;
        if (used + (size_t)nwrite + 1 >= out_sz)
            break;
        memcpy(out + used, line, (size_t)nwrite);
        used += (size_t)nwrite;
        out[used] = '\0';
    }
    return ERR_OK;
}
