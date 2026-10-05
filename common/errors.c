#include "types.h"

const char *err_to_string(err_t e)
{
    switch (e) {
    case ERR_OK:           return "OK";
    case ERR_NOT_FOUND:    return "fichier ou repertoire introuvable";
    case ERR_EXISTS:       return "element deja existant";
    case ERR_NO_SPACE:     return "espace insuffisant";
    case ERR_BAD_TYPE:     return "mauvais type d'objet";
    case ERR_NOT_EMPTY:    return "repertoire non vide";
    case ERR_INVALID_PATH: return "chemin invalide";
    case ERR_IO:           return "erreur d'entree/sortie";
    case ERR_PERM:         return "permission refusee";
    case ERR_BUSY:         return "ressource occupee";
    case ERR_ARGS:         return "arguments invalides";
    default:               return "erreur inconnue";
    }
}
