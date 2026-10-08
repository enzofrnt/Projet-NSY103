#ifndef PARSER_H
#define PARSER_H

#include "../common/types.h"

/* Parse une ligne de commande en requête structurée.
 * Retourne 0 si OK, -1 si ligne vide / erreur de syntaxe. */
int parse_line(const char *line, request_t *req, char *err, size_t err_sz);

#endif
