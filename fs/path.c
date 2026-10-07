#define _POSIX_C_SOURCE 200809L

#include "path.h"
#include "directory.h"
#include "inode.h"

#include <string.h>

err_t resolve_path(uint32_t cwd, const char *path, uint32_t *out_ino)
{
    char buf[MAX_PATH];
    char *tok, *save;
    uint32_t cur;
    inode_t ino;
    err_t e;

    if (!path || path[0] == '\0')
        return ERR_INVALID_PATH;

    if (path[0] == '/')
        cur = ROOT_INODE;
    else
        cur = cwd;

    if (strcmp(path, "/") == 0) {
        *out_ino = ROOT_INODE;
        return ERR_OK;
    }

    strncpy(buf, path, MAX_PATH - 1);
    buf[MAX_PATH - 1] = '\0';

    tok = strtok_r(buf, "/", &save);
    while (tok) {
        e = read_inode(cur, &ino);
        if (e != ERR_OK)
            return e;
        if (ino.type != INODE_DIR)
            return ERR_BAD_TYPE;

        if (strcmp(tok, ".") == 0) {
            /* reste sur cur */
        } else if (strcmp(tok, "..") == 0) {
            e = directory_find(cur, "..", &cur);
            if (e != ERR_OK)
                return e;
        } else {
            e = directory_find(cur, tok, &cur);
            if (e != ERR_OK)
                return e;
        }
        tok = strtok_r(NULL, "/", &save);
    }

    *out_ino = cur;
    return ERR_OK;
}

err_t resolve_parent(uint32_t cwd, const char *path, uint32_t *parent_ino,
                     char *leaf, size_t leaf_sz)
{
    char buf[MAX_PATH];
    char *slash;
    err_t e;

    if (!path || path[0] == '\0')
        return ERR_INVALID_PATH;

    strncpy(buf, path, MAX_PATH - 1);
    buf[MAX_PATH - 1] = '\0';

    /* Trailing slash */
    while (strlen(buf) > 1 && buf[strlen(buf) - 1] == '/')
        buf[strlen(buf) - 1] = '\0';

    if (strcmp(buf, "/") == 0)
        return ERR_INVALID_PATH;

    slash = strrchr(buf, '/');
    if (!slash) {
        *parent_ino = cwd;
        if (strlen(buf) >= leaf_sz)
            return ERR_INVALID_PATH;
        strcpy(leaf, buf);
        return ERR_OK;
    }

    if (slash == buf) {
        /* /name */
        *parent_ino = ROOT_INODE;
        if (strlen(slash + 1) >= leaf_sz || slash[1] == '\0')
            return ERR_INVALID_PATH;
        strcpy(leaf, slash + 1);
        return ERR_OK;
    }

    *slash = '\0';
    e = resolve_path(cwd, buf, parent_ino);
    if (e != ERR_OK)
        return e;
    if (strlen(slash + 1) >= leaf_sz || slash[1] == '\0')
        return ERR_INVALID_PATH;
    strcpy(leaf, slash + 1);
    return ERR_OK;
}
