#include "fs.h"
#include "disk.h"
#include "bitmap.h"
#include "inode.h"
#include "directory.h"
#include "path.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>
#include <unistd.h>

static uint32_t g_cwd = ROOT_INODE;

/* Table des fichiers ouverts (inode → compteur) */
#define MAX_OPEN 64
static uint32_t g_open[MAX_OPEN];
static int      g_open_count[MAX_OPEN];

void fs_set_cwd(uint32_t ino) { g_cwd = ino; }
uint32_t fs_get_cwd(void) { return g_cwd; }

static int open_add(uint32_t ino)
{
    int i, free_slot = -1;
    for (i = 0; i < MAX_OPEN; i++) {
        if (g_open_count[i] > 0 && g_open[i] == ino) {
            g_open_count[i]++;
            return 0;
        }
        if (g_open_count[i] == 0 && free_slot < 0)
            free_slot = i;
    }
    if (free_slot < 0)
        return -1;
    g_open[free_slot] = ino;
    g_open_count[free_slot] = 1;
    return 0;
}

static int open_remove(uint32_t ino)
{
    int i;
    for (i = 0; i < MAX_OPEN; i++) {
        if (g_open_count[i] > 0 && g_open[i] == ino) {
            g_open_count[i]--;
            return 0;
        }
    }
    return -1;
}

static err_t ensure_parent_dir(const char *disk_path)
{
    char tmp[MAX_PATH];
    char *dir;
    strncpy(tmp, disk_path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    dir = dirname(tmp);
    if (mkdir(dir, 0755) != 0) {
        /* ignore EEXIST */
    }
    return ERR_OK;
}

/*
 * Formate disque.img (layout fixe) et crée la racine "/" (inode 0) avec "." / "..".
 */
err_t fs_format(const char *path)
{
    uint8_t zero[BLOCK_SIZE];
    superblock_t *sb;
    inode_t root;
    uint32_t i, inode_blocks, total_disk_blocks;
    err_t e;

    ensure_parent_dir(path);

    /* Layout :
     * 0              : superbloc
     * 1              : bitmap inodes
     * 2              : bitmap blocs
     * 3 .. 3+N-1     : table inodes
     * data_start ..  : blocs de données
     */
    inode_blocks = (MAX_INODES * sizeof(inode_t) + BLOCK_SIZE - 1) / BLOCK_SIZE;
    total_disk_blocks = 3 + inode_blocks + MAX_DATA_BLOCKS;

    e = disk_open(path, 1);
    if (e != ERR_OK)
        return e;

    memset(zero, 0, BLOCK_SIZE);
    for (i = 0; i < total_disk_blocks; i++) {
        e = disk_write_block(i, zero);
        if (e != ERR_OK) {
            disk_close();
            return e;
        }
    }

    sb = disk_superblock();
    memset(sb, 0, sizeof(*sb));
    sb->magic = FS_MAGIC;
    sb->block_size = BLOCK_SIZE;
    sb->total_blocks = MAX_DATA_BLOCKS;
    sb->free_blocks = MAX_DATA_BLOCKS;
    sb->total_inodes = MAX_INODES;
    sb->free_inodes = MAX_INODES;
    sb->inode_bitmap_block = 1;
    sb->block_bitmap_block = 2;
    sb->inode_table_block = 3;
    sb->data_start_block = 3 + inode_blocks;
    sb->inode_size = sizeof(inode_t);
    sb->root_inode = ROOT_INODE;

    e = disk_sync_superblock();
    if (e != ERR_OK) {
        disk_close();
        return e;
    }

    e = bitmap_init();
    if (e != ERR_OK) {
        disk_close();
        return e;
    }

    /* Allouer inode racine */
    {
        uint32_t ino;
        e = allocate_inode(&ino);
        if (e != ERR_OK || ino != ROOT_INODE) {
            disk_close();
            return ERR_IO;
        }
    }

    inode_init_empty(&root, ROOT_INODE, INODE_DIR, PERM_DEFAULT_DIR);
    root.nlinks = 2; /* . et .. pointent sur elle */
    e = write_inode(&root);
    if (e != ERR_OK) {
        disk_close();
        return e;
    }

    e = directory_init(ROOT_INODE, ROOT_INODE);
    if (e != ERR_OK) {
        disk_close();
        return e;
    }

    disk_sync_superblock();
    g_cwd = ROOT_INODE;
    memset(g_open, 0, sizeof(g_open));
    memset(g_open_count, 0, sizeof(g_open_count));
    return ERR_OK;
}

err_t fs_mount(const char *path)
{
    err_t e;
    struct stat st;

    if (stat(path, &st) != 0)
        return fs_format(path);

    e = disk_open(path, 0);
    if (e != ERR_OK)
        return e;

    e = bitmap_init();
    if (e != ERR_OK) {
        disk_close();
        return e;
    }

    g_cwd = ROOT_INODE;
    memset(g_open, 0, sizeof(g_open));
    memset(g_open_count, 0, sizeof(g_open_count));
    return ERR_OK;
}

void fs_unmount(void)
{
    disk_sync_superblock();
    disk_close();
}

err_t fs_resolve(const char *path, uint32_t *out)
{
    return resolve_path(g_cwd, path, out);
}

int32_t _mycreat(const char *nom, uint16_t mode)
{
    uint32_t parent, child;
    char leaf[MAX_FILENAME];
    inode_t ino;
    err_t e;

    e = resolve_parent(g_cwd, nom, &parent, leaf, sizeof(leaf));
    if (e != ERR_OK)
        return -e;

    e = directory_find(parent, leaf, &child);
    if (e == ERR_OK)
        return -ERR_EXISTS;
    if (e != ERR_NOT_FOUND)
        return -e;

    e = allocate_inode(&child);
    if (e != ERR_OK)
        return -e;

    inode_init_empty(&ino, child, INODE_FILE,
                     mode ? mode : PERM_DEFAULT_FILE);
    e = write_inode(&ino);
    if (e != ERR_OK) {
        free_inode(child);
        return -e;
    }

    e = directory_add(parent, leaf, child);
    if (e != ERR_OK) {
        free_inode(child);
        return -e;
    }
    return (int32_t)child;
}

int32_t _myopen(const char *nom, int mode)
{
    uint32_t ino;
    err_t e;
    int32_t created = -1;

    e = resolve_path(g_cwd, nom, &ino);
    if (e == ERR_NOT_FOUND) {
        if (!(mode & MODE_CREATE))
            return -ERR_NOT_FOUND;
        created = _mycreat(nom, PERM_DEFAULT_FILE);
        if (created < 0)
            return created;
        ino = (uint32_t)created;
    } else if (e != ERR_OK) {
        return -e;
    } else if (mode & MODE_TRUNC) {
        inode_t node;
        e = read_inode(ino, &node);
        if (e != ERR_OK)
            return -e;
        if (node.type != INODE_FILE)
            return -ERR_BAD_TYPE;
        e = inode_free_all_blocks(&node);
        if (e != ERR_OK)
            return -e;
    }

    {
        inode_t node;
        e = read_inode(ino, &node);
        if (e != ERR_OK)
            return -e;
        if (node.type != INODE_FILE && !(mode & MODE_READ))
            return -ERR_BAD_TYPE;
    }

    if (open_add(ino) != 0)
        return -ERR_BUSY;
    return (int32_t)ino;
}

err_t _myclose(uint32_t inode)
{
    if (open_remove(inode) != 0)
        return ERR_NOT_FOUND;
    return ERR_OK;
}

int32_t _myread(uint32_t inode, void *buffer, uint32_t nombre)
{
    inode_t node;
    uint8_t blk[BLOCK_SIZE];
    uint32_t pos = 0, to_read, fblk, dblk, off, chunk;
    uint8_t *dst = (uint8_t *)buffer;
    err_t e;

    e = read_inode(inode, &node);
    if (e != ERR_OK)
        return -e;
    if (node.type != INODE_FILE)
        return -ERR_BAD_TYPE;

    if (nombre > node.size)
        nombre = node.size;

    while (pos < nombre) {
        fblk = pos / BLOCK_SIZE;
        off  = pos % BLOCK_SIZE;
        chunk = BLOCK_SIZE - off;
        to_read = nombre - pos;
        if (chunk > to_read)
            chunk = to_read;

        e = inode_get_block(&node, fblk, &dblk, 0);
        if (e != ERR_OK)
            return -e;
        e = disk_read_block(dblk, blk);
        if (e != ERR_OK)
            return -e;
        memcpy(dst + pos, blk + off, chunk);
        pos += chunk;
    }
    return (int32_t)pos;
}

int32_t _mywrite(uint32_t inode, const void *buffer, uint32_t nombre)
{
    inode_t node;
    uint8_t blk[BLOCK_SIZE];
    uint32_t pos = 0, fblk, dblk, off, chunk;
    const uint8_t *src = (const uint8_t *)buffer;
    err_t e;

    e = read_inode(inode, &node);
    if (e != ERR_OK)
        return -e;
    if (node.type != INODE_FILE)
        return -ERR_BAD_TYPE;

    while (pos < nombre) {
        fblk = pos / BLOCK_SIZE;
        off  = pos % BLOCK_SIZE;
        chunk = BLOCK_SIZE - off;
        if (chunk > nombre - pos)
            chunk = nombre - pos;

        e = inode_get_block(&node, fblk, &dblk, 1);
        if (e != ERR_OK)
            return -e;
        e = disk_read_block(dblk, blk);
        if (e != ERR_OK)
            return -e;
        memcpy(blk + off, src + pos, chunk);
        e = disk_write_block(dblk, blk);
        if (e != ERR_OK)
            return -e;
        pos += chunk;
    }

    e = read_inode(inode, &node);
    if (e != ERR_OK)
        return -e;
    if (nombre > node.size)
        node.size = nombre;
    e = write_inode(&node);
    if (e != ERR_OK)
        return -e;
    return (int32_t)nombre;
}

err_t _mkdir(const char *nom)
{
    uint32_t parent, child;
    char leaf[MAX_FILENAME];
    inode_t ino, pnode;
    err_t e;

    e = resolve_parent(g_cwd, nom, &parent, leaf, sizeof(leaf));
    if (e != ERR_OK)
        return e;

    e = directory_find(parent, leaf, &child);
    if (e == ERR_OK)
        return ERR_EXISTS;
    if (e != ERR_NOT_FOUND)
        return e;

    e = allocate_inode(&child);
    if (e != ERR_OK)
        return e;

    inode_init_empty(&ino, child, INODE_DIR, PERM_DEFAULT_DIR);
    ino.nlinks = 2;
    e = write_inode(&ino);
    if (e != ERR_OK) {
        free_inode(child);
        return e;
    }

    e = directory_init(child, parent);
    if (e != ERR_OK) {
        free_inode(child);
        return e;
    }

    e = directory_add(parent, leaf, child);
    if (e != ERR_OK) {
        free_inode(child);
        return e;
    }

    /* Increment parent nlinks for ".." */
    e = read_inode(parent, &pnode);
    if (e == ERR_OK) {
        pnode.nlinks++;
        write_inode(&pnode);
    }
    return ERR_OK;
}

err_t _rmdir(const char *nom)
{
    uint32_t parent, target;
    char leaf[MAX_FILENAME];
    inode_t node, pnode;
    int empty;
    err_t e;

    e = resolve_parent(g_cwd, nom, &parent, leaf, sizeof(leaf));
    if (e != ERR_OK)
        return e;
    if (strcmp(leaf, ".") == 0 || strcmp(leaf, "..") == 0)
        return ERR_INVALID_PATH;

    e = directory_find(parent, leaf, &target);
    if (e != ERR_OK)
        return e;

    e = read_inode(target, &node);
    if (e != ERR_OK)
        return e;
    if (node.type != INODE_DIR)
        return ERR_BAD_TYPE;

    e = directory_is_empty(target, &empty);
    if (e != ERR_OK)
        return e;
    if (!empty)
        return ERR_NOT_EMPTY;

    e = directory_remove(parent, leaf);
    if (e != ERR_OK)
        return e;

    e = inode_free_all_blocks(&node);
    if (e != ERR_OK)
        return e;
    e = free_inode(target);
    if (e != ERR_OK)
        return e;

    e = read_inode(parent, &pnode);
    if (e == ERR_OK && pnode.nlinks > 0) {
        pnode.nlinks--;
        write_inode(&pnode);
    }

    if (g_cwd == target)
        g_cwd = parent;
    return ERR_OK;
}

err_t _unlink(const char *nom)
{
    uint32_t parent, target;
    char leaf[MAX_FILENAME];
    inode_t node;
    err_t e;

    e = resolve_parent(g_cwd, nom, &parent, leaf, sizeof(leaf));
    if (e != ERR_OK)
        return e;

    e = directory_find(parent, leaf, &target);
    if (e != ERR_OK)
        return e;

    e = read_inode(target, &node);
    if (e != ERR_OK)
        return e;
    if (node.type != INODE_FILE)
        return ERR_BAD_TYPE;

    e = directory_remove(parent, leaf);
    if (e != ERR_OK)
        return e;

    e = inode_free_all_blocks(&node);
    if (e != ERR_OK)
        return e;
    return free_inode(target);
}

err_t fs_stat_df(char *out, size_t out_sz)
{
    superblock_t *sb = disk_superblock();
    uint32_t used_blocks = sb->total_blocks - sb->free_blocks;
    uint32_t used_inodes = sb->total_inodes - sb->free_inodes;
    snprintf(out, out_sz,
             "SGF Mon D.O.S.\n"
             "Blocs  : %u / %u utilises (%u libres) — %u octets/bloc\n"
             "Inodes : %u / %u utilises (%u libres)\n"
             "Espace : %u Ko utilises / %u Ko total\n",
             used_blocks, sb->total_blocks, sb->free_blocks, sb->block_size,
             used_inodes, sb->total_inodes, sb->free_inodes,
             (used_blocks * sb->block_size) / 1024,
             (sb->total_blocks * sb->block_size) / 1024);
    return ERR_OK;
}

err_t fs_list(const char *path, char *out, size_t out_sz, int long_fmt)
{
    uint32_t ino;
    err_t e;
    const char *p = (path && path[0]) ? path : ".";

    e = resolve_path(g_cwd, p, &ino);
    if (e != ERR_OK)
        return e;
    return directory_list(ino, out, out_sz, long_fmt);
}

err_t fs_cat(const char *path, char *out, size_t out_sz)
{
    int32_t fd, n;
    if (out_sz == 0)
        return ERR_ARGS;
    fd = _myopen(path, MODE_READ);
    if (fd < 0)
        return (err_t)(-fd);
    n = _myread((uint32_t)fd, out, (uint32_t)(out_sz - 1));
    _myclose((uint32_t)fd);
    if (n < 0)
        return (err_t)(-n);
    out[n] = '\0';
    return ERR_OK;
}

err_t fs_echo_write(const char *path, const char *text)
{
    int32_t fd, n;
    size_t len = text ? strlen(text) : 0;

    fd = _myopen(path, MODE_WRITE | MODE_CREATE | MODE_TRUNC);
    if (fd < 0)
        return (err_t)(-fd);
    if (len > 0) {
        n = _mywrite((uint32_t)fd, text, (uint32_t)len);
        if (n < 0) {
            _myclose((uint32_t)fd);
            return (err_t)(-n);
        }
    }
    _myclose((uint32_t)fd);
    return ERR_OK;
}

err_t fs_cp(const char *src, const char *dst)
{
    char buf[MAX_OUTPUT];
    err_t e;
    int32_t n;

    e = fs_cat(src, buf, sizeof(buf));
    if (e != ERR_OK)
        return e;
    n = _myopen(dst, MODE_WRITE | MODE_CREATE | MODE_TRUNC);
    if (n < 0)
        return (err_t)(-n);
    if (buf[0]) {
        int32_t w = _mywrite((uint32_t)n, buf, (uint32_t)strlen(buf));
        if (w < 0) {
            _myclose((uint32_t)n);
            return (err_t)(-w);
        }
    }
    _myclose((uint32_t)n);
    return ERR_OK;
}

err_t fs_mv(const char *src, const char *dst)
{
    err_t e = fs_cp(src, dst);
    if (e != ERR_OK)
        return e;
    return _unlink(src);
}

/* Reconstruction du chemin pwd via remontée « .. » */
err_t fs_pwd(char *out, size_t out_sz)
{
    char parts[64][MAX_FILENAME];
    int depth = 0;
    uint32_t cur = g_cwd, parent, child;
    inode_t dir;
    dirent_t ent;
    uint32_t i, n;
    err_t e;
    uint8_t bbuf[BLOCK_SIZE];
    uint32_t fblk, dblk, off;
    const uint32_t dp = BLOCK_SIZE / sizeof(dirent_t);

    if (g_cwd == ROOT_INODE) {
        snprintf(out, out_sz, "/");
        return ERR_OK;
    }

    while (cur != ROOT_INODE && depth < 64) {
        e = directory_find(cur, "..", &parent);
        if (e != ERR_OK)
            return e;

        e = read_inode(parent, &dir);
        if (e != ERR_OK)
            return e;

        n = dir.size / sizeof(dirent_t);
        child = cur;
        parts[depth][0] = '\0';

        for (i = 0; i < n; i++) {
            fblk = i / dp;
            off = i % dp;
            e = inode_get_block(&dir, fblk, &dblk, 0);
            if (e != ERR_OK)
                return e;
            e = disk_read_block(dblk, bbuf);
            if (e != ERR_OK)
                return e;
            memcpy(&ent, bbuf + off * sizeof(dirent_t), sizeof(ent));
            if (ent.name[0] && ent.inode == child &&
                strcmp(ent.name, ".") != 0 && strcmp(ent.name, "..") != 0) {
                strncpy(parts[depth], ent.name, MAX_FILENAME - 1);
                parts[depth][MAX_FILENAME - 1] = '\0';
                break;
            }
        }
        if (parts[depth][0] == '\0')
            return ERR_IO;
        depth++;
        cur = parent;
    }

    {
        size_t used = 0;
        int j;
        out[0] = '\0';
        for (j = depth - 1; j >= 0; j--) {
            int nw = snprintf(out + used, out_sz - used, "/%s", parts[j]);
            if (nw < 0 || (size_t)nw >= out_sz - used)
                break;
            used += (size_t)nw;
        }
        if (used == 0)
            snprintf(out, out_sz, "/");
    }
    return ERR_OK;
}
