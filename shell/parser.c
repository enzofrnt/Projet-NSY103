#include "parser.h"
#include "../kernel/commands.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

/* Tokenise en respectant les guillemets */
static int next_token(const char **pp, char *out, size_t out_sz)
{
    const char *p = *pp;
    size_t n = 0;
    char quote = 0;

    while (*p && isspace((unsigned char)*p))
        p++;
    if (*p == '\0') {
        *pp = p;
        return 0;
    }

    if (*p == '"' || *p == '\'') {
        quote = *p++;
        while (*p && *p != quote && n + 1 < out_sz)
            out[n++] = *p++;
        if (*p == quote)
            p++;
    } else {
        while (*p && !isspace((unsigned char)*p) && *p != '>' && n + 1 < out_sz)
            out[n++] = *p++;
    }
    out[n] = '\0';
    *pp = p;
    return 1;
}

int parse_line(const char *line, request_t *req, char *err, size_t err_sz)
{
    char tok[MAX_ARG_LEN];
    const char *p = line;
    int first = 1;

    memset(req, 0, sizeof(*req));
    if (err && err_sz)
        err[0] = '\0';

    while (next_token(&p, tok, sizeof(tok))) {
        /* Opérateur de redirection > */
        while (*p && isspace((unsigned char)*p))
            p++;
        if (tok[0] == '>' && tok[1] == '\0') {
            if (!next_token(&p, tok, sizeof(tok))) {
                snprintf(err, err_sz, "redirection sans fichier");
                return -1;
            }
            req->redirect = 1;
            strncpy(req->redir_path, tok, MAX_ARG_LEN - 1);
            continue;
        }
        /* Forme « >fichier » collée */
        if (tok[0] == '>' && tok[1] != '\0') {
            req->redirect = 1;
            strncpy(req->redir_path, tok + 1, MAX_ARG_LEN - 1);
            continue;
        }

        if (first) {
            /* Une seule source de vérité : la table du Noyau */
            req->type = cmd_type_from_name(tok);
            if (req->type == CMD_NONE) {
                snprintf(err, err_sz, "commande inconnue : %s", tok);
                return -1;
            }
            first = 0;
        } else {
            if (req->argc >= (int)MAX_ARGS) {
                snprintf(err, err_sz, "trop d'arguments");
                return -1;
            }
            strncpy(req->args[req->argc], tok, MAX_ARG_LEN - 1);
            req->argc++;
        }

        /* Détection de « > » après un argument */
        while (*p && isspace((unsigned char)*p))
            p++;
        if (*p == '>') {
            p++;
            if (*p == '>')
                p++; /* >> traité comme > */
            if (!next_token(&p, tok, sizeof(tok))) {
                snprintf(err, err_sz, "redirection sans fichier");
                return -1;
            }
            req->redirect = 1;
            strncpy(req->redir_path, tok, MAX_ARG_LEN - 1);
        }
    }

    if (first) {
        /* ligne vide */
        return -1;
    }
    return 0;
}
