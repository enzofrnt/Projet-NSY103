#ifndef COMMANDS_H
#define COMMANDS_H

#include "../common/types.h"

/*
 * Table de dispatch du Noyau.
 *
 * Au lieu d'un long switch/if sur le type de commande, chaque entrée
 * associe : nom textuel, arité, aide, et un pointeur de fonction handler.
 * - cmd_type_from_name() : utilisé par le parser Shell (texte → type)
 * - cmd_dispatch()       : utilisé par le Noyau (type → handler)
 * Une seule table = source de vérité pour les commandes.
 */

typedef err_t (*cmd_handler_t)(const request_t *req, response_t *resp);

typedef struct {
    cmd_type_t    type;
    const char   *name;
    int           min_args;
    int           max_args;
    const char   *help;
    cmd_handler_t handler;
} cmd_entry_t;

const cmd_entry_t *cmd_table(void);
size_t             cmd_table_size(void);
cmd_type_t         cmd_type_from_name(const char *name);
err_t              cmd_dispatch(const request_t *req, response_t *resp);

#endif
