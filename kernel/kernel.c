#include "../common/types.h"
#include "../common/errors.h"
#include "../fs/fs.h"
#include "../ipc/ipc.h"
#include "commands.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * Noyau — processus fils du Shell (un seul fork pour toute la session).
 *
 * Boucle : lire request_t → cmd_dispatch (table de pointeurs de fonctions)
 *        → écrire response_t. Monte le SGF une fois au démarrage.
 * Aucun fork par commande : ls, mkdir, etc. sont des handlers C.
 */
void kernel_loop(int req_fd, int resp_fd)
{
    request_t req;
    response_t resp;
    int rc;

    if (fs_mount(DISK_PATH) != ERR_OK) {
        fprintf(stderr, "[noyau] impossible de monter %s\n", DISK_PATH);
        exit(1);
    }

    for (;;) {
        rc = ipc_recv_request(req_fd, &req);
        if (rc != 0)
            break;

        cmd_dispatch(&req, &resp);

        if (ipc_send_response(resp_fd, &resp) != 0)
            break;

        if (req.type == CMD_EXIT)
            break;
    }

    fs_unmount();
}
