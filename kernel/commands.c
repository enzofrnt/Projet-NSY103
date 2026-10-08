#include "commands.h"
#include "../fs/fs.h"
#include "../fs/inode.h"
#include "../common/errors.h"

#include <stdio.h>
#include <string.h>

static void resp_ok(response_t *resp, const char *msg)
{
    resp->status = ERR_OK;
    resp->cwd_changed = 0;
    if (msg)
        snprintf(resp->output, sizeof(resp->output), "%s", msg);
    else
        resp->output[0] = '\0';
}

static void resp_err(response_t *resp, err_t e)
{
    resp->status = e;
    resp->cwd_changed = 0;
    snprintf(resp->output, sizeof(resp->output), "Erreur : %s\n", err_to_string(e));
}

static err_t cmd_ls(const request_t *req, response_t *resp)
{
    int long_fmt = 0;
    const char *path = ".";
    int i;
    err_t e;

    for (i = 0; i < req->argc; i++) {
        if (strcmp(req->args[i], "-l") == 0)
            long_fmt = 1;
        else
            path = req->args[i];
    }
    e = fs_list(path, resp->output, sizeof(resp->output), long_fmt);
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    resp->status = ERR_OK;
    resp->cwd_changed = 0;
    return ERR_OK;
}

static err_t cmd_mkdir(const request_t *req, response_t *resp)
{
    err_t e;
    if (req->argc < 1) {
        resp_err(resp, ERR_ARGS);
        return ERR_ARGS;
    }
    e = _mkdir(req->args[0]);
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    resp_ok(resp, "");
    return ERR_OK;
}

static err_t cmd_rmdir(const request_t *req, response_t *resp)
{
    err_t e;
    if (req->argc < 1) {
        resp_err(resp, ERR_ARGS);
        return ERR_ARGS;
    }
    e = _rmdir(req->args[0]);
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    resp_ok(resp, "");
    return ERR_OK;
}

static err_t cmd_cd(const request_t *req, response_t *resp)
{
    uint32_t ino;
    inode_t node;
    err_t e;
    const char *path = (req->argc >= 1) ? req->args[0] : "/";

    e = fs_resolve(path, &ino);
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    e = read_inode(ino, &node);
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    if (node.type != INODE_DIR) {
        resp_err(resp, ERR_BAD_TYPE);
        return ERR_BAD_TYPE;
    }
    fs_set_cwd(ino);
    resp->status = ERR_OK;
    resp->cwd_changed = 1;
    resp->new_cwd = ino;
    resp->output[0] = '\0';
    return ERR_OK;
}

static err_t cmd_cp(const request_t *req, response_t *resp)
{
    err_t e;
    if (req->argc < 2) {
        resp_err(resp, ERR_ARGS);
        return ERR_ARGS;
    }
    e = fs_cp(req->args[0], req->args[1]);
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    resp_ok(resp, "");
    return ERR_OK;
}

static err_t cmd_rm(const request_t *req, response_t *resp)
{
    err_t e;
    if (req->argc < 1) {
        resp_err(resp, ERR_ARGS);
        return ERR_ARGS;
    }
    e = _unlink(req->args[0]);
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    resp_ok(resp, "");
    return ERR_OK;
}

static err_t cmd_mv(const request_t *req, response_t *resp)
{
    err_t e;
    if (req->argc < 2) {
        resp_err(resp, ERR_ARGS);
        return ERR_ARGS;
    }
    e = fs_mv(req->args[0], req->args[1]);
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    resp_ok(resp, "");
    return ERR_OK;
}

static err_t cmd_cat(const request_t *req, response_t *resp)
{
    err_t e;
    if (req->argc < 1) {
        resp_err(resp, ERR_ARGS);
        return ERR_ARGS;
    }
    e = fs_cat(req->args[0], resp->output, sizeof(resp->output));
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    /* Ajouter un \n si absent pour l'affichage */
    {
        size_t len = strlen(resp->output);
        if (len > 0 && resp->output[len - 1] != '\n' && len + 1 < sizeof(resp->output)) {
            resp->output[len] = '\n';
            resp->output[len + 1] = '\0';
        }
    }
    resp->status = ERR_OK;
    resp->cwd_changed = 0;
    return ERR_OK;
}

static err_t cmd_echo(const request_t *req, response_t *resp)
{
    char text[MAX_OUTPUT];
    int i;
    size_t used = 0;

    text[0] = '\0';
    for (i = 0; i < req->argc; i++) {
        int nw = snprintf(text + used, sizeof(text) - used, "%s%s",
                          (i > 0) ? " " : "", req->args[i]);
        if (nw < 0 || (size_t)nw >= sizeof(text) - used)
            break;
        used += (size_t)nw;
    }

    if (req->redirect) {
        err_t e = fs_echo_write(req->redir_path, text);
        if (e != ERR_OK) {
            resp_err(resp, e);
            return e;
        }
        resp_ok(resp, "");
        return ERR_OK;
    }

    snprintf(resp->output, sizeof(resp->output), "%s\n", text);
    resp->status = ERR_OK;
    resp->cwd_changed = 0;
    return ERR_OK;
}

static err_t cmd_df(const request_t *req, response_t *resp)
{
    err_t e;
    (void)req;
    e = fs_stat_df(resp->output, sizeof(resp->output));
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    resp->status = ERR_OK;
    resp->cwd_changed = 0;
    return ERR_OK;
}

static err_t cmd_pwd(const request_t *req, response_t *resp)
{
    err_t e;
    (void)req;
    e = fs_pwd(resp->output, sizeof(resp->output));
    if (e != ERR_OK) {
        resp_err(resp, e);
        return e;
    }
    {
        size_t len = strlen(resp->output);
        if (len + 1 < sizeof(resp->output)) {
            resp->output[len] = '\n';
            resp->output[len + 1] = '\0';
        }
    }
    resp->status = ERR_OK;
    resp->cwd_changed = 0;
    return ERR_OK;
}

static err_t cmd_help(const request_t *req, response_t *resp)
{
    const cmd_entry_t *t = cmd_table();
    size_t n = cmd_table_size();
    size_t i, used = 0;
    (void)req;

    used = (size_t)snprintf(resp->output, sizeof(resp->output),
                            "Commandes disponibles (Mon D.O.S.) :\n");
    for (i = 0; i < n; i++) {
        if (t[i].type == CMD_EXIT || t[i].type == CMD_NONE)
            continue;
        int nw = snprintf(resp->output + used, sizeof(resp->output) - used,
                          "  %-10s %s\n", t[i].name, t[i].help);
        if (nw < 0 || (size_t)nw >= sizeof(resp->output) - used)
            break;
        used += (size_t)nw;
    }
    resp->status = ERR_OK;
    resp->cwd_changed = 0;
    return ERR_OK;
}

static err_t cmd_exit(const request_t *req, response_t *resp)
{
    (void)req;
    resp_ok(resp, "Au revoir.\n");
    return ERR_OK;
}

/* Source unique des commandes (noms + handlers). Voir doc/commandes.md */
static const cmd_entry_t g_table[] = {
    { CMD_LS,    "ls",    0, 2, "liste le contenu [-l] [chemin]",     cmd_ls },
    { CMD_MKDIR, "mkdir", 1, 1, "cree un repertoire",                 cmd_mkdir },
    { CMD_RMDIR, "rmdir", 1, 1, "supprime un repertoire vide",        cmd_rmdir },
    { CMD_CD,    "cd",    0, 1, "change de repertoire",               cmd_cd },
    { CMD_CP,    "cp",    2, 2, "copie un fichier",                   cmd_cp },
    { CMD_RM,    "rm",    1, 1, "supprime un fichier",                cmd_rm },
    { CMD_MV,    "mv",    2, 2, "deplace/renomme un fichier",         cmd_mv },
    { CMD_CAT,   "cat",   1, 1, "affiche un fichier",                 cmd_cat },
    { CMD_ECHO,  "echo",  0, MAX_ARGS, "affiche ou redirige (> fic)", cmd_echo },
    { CMD_DF,    "df",    0, 0, "occupation du SGF",                  cmd_df },
    { CMD_PWD,   "pwd",   0, 0, "repertoire courant",                 cmd_pwd },
    { CMD_HELP,  "help",  0, 0, "aide",                               cmd_help },
    { CMD_EXIT,  "exit",  0, 0, "quitte le shell",                    cmd_exit },
};

const cmd_entry_t *cmd_table(void) { return g_table; }
size_t cmd_table_size(void) { return sizeof(g_table) / sizeof(g_table[0]); }

cmd_type_t cmd_type_from_name(const char *name)
{
    size_t i, n;

    if (!name || !name[0])
        return CMD_NONE;

    /* Alias pratique */
    if (strcmp(name, "quit") == 0)
        return CMD_EXIT;

    n = cmd_table_size();
    for (i = 0; i < n; i++) {
        if (strcmp(g_table[i].name, name) == 0)
            return g_table[i].type;
    }
    return CMD_NONE;
}

err_t cmd_dispatch(const request_t *req, response_t *resp)
{
    size_t i, n = cmd_table_size();
    memset(resp, 0, sizeof(*resp));

    for (i = 0; i < n; i++) {
        if (g_table[i].type == req->type) {
            return g_table[i].handler(req, resp);
        }
    }
    resp_err(resp, ERR_UNKNOWN);
    return ERR_UNKNOWN;
}
