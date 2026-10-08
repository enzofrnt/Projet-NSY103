/*
 * Shell — processus père, unique point d'entrée utilisateur.
 *
 * Mécanisme clé : un seul fork() au démarrage crée le Noyau (fils).
 * Les commandes ne sont PAS forkées ensuite : elles sont exécutées
 * dans le Noyau via la table de dispatch, après échange request_t /
 * response_t sur deux pipes anonymes.
 *
 * L'« écran » du sujet = stdout de ce processus (le terminal hôte).
 */

#define _POSIX_C_SOURCE 200809L

#include "../common/types.h"
#include "../common/errors.h"
#include "../ipc/ipc.h"
#include "../kernel/kernel.h"
#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

static pid_t g_kernel_pid = -1;

static void on_signal(int sig)
{
    (void)sig;
    if (g_kernel_pid > 0)
        kill(g_kernel_pid, SIGTERM);
    _exit(0);
}

int main(void)
{
    int to_kernel[2];   /* Shell écrit → Noyau lit */
    int from_kernel[2]; /* Noyau écrit → Shell lit */
    char line[512];
    char err[128];
    request_t req;
    response_t resp;
    char prompt[MAX_PATH + 32];

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (ipc_pipe_create(to_kernel) != 0 || ipc_pipe_create(from_kernel) != 0) {
        perror("pipe");
        return 1;
    }

    g_kernel_pid = fork();
    if (g_kernel_pid < 0) {
        perror("fork");
        return 1;
    }

    if (g_kernel_pid == 0) {
        /* Fils = Noyau : ne garde que les extrémités utiles des pipes */
        close(to_kernel[1]);
        close(from_kernel[0]);
        kernel_loop(to_kernel[0], from_kernel[1]);
        _exit(0);
    }

    /* Père = Shell */
    close(to_kernel[0]);
    close(from_kernel[1]);

    printf("Mon D.O.S. — tapez 'help' pour l'aide, 'exit' pour quitter.\n");
    snprintf(prompt, sizeof(prompt), "mdos:/ > ");

    while (1) {
        printf("%s", prompt);
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            req.type = CMD_EXIT;
            req.argc = 0;
            req.redirect = 0;
            ipc_send_request(to_kernel[1], &req);
            ipc_recv_response(from_kernel[0], &resp);
            break;
        }

        {
            size_t len = strlen(line);
            if (len && line[len - 1] == '\n')
                line[len - 1] = '\0';
        }

        if (line[0] == '\0')
            continue;

        /* Parsing local : texte → request_t (type issu de la table du Noyau) */
        if (parse_line(line, &req, err, sizeof(err)) != 0) {
            if (err[0])
                printf("Erreur : %s\n", err);
            continue;
        }

        if (ipc_send_request(to_kernel[1], &req) != 0) {
            fprintf(stderr, "Erreur envoi vers le noyau\n");
            break;
        }
        if (ipc_recv_response(from_kernel[0], &resp) != 0) {
            fprintf(stderr, "Erreur reception du noyau\n");
            break;
        }

        if (resp.output[0])
            printf("%s", resp.output);
        else if (resp.status != ERR_OK)
            printf("Erreur : %s\n", err_to_string(resp.status));

        if (req.type == CMD_EXIT)
            break;

        /* Le cwd vit dans le Noyau : on interroge pwd pour rafraîchir le prompt */
        if (req.type == CMD_CD && resp.status == ERR_OK) {
            request_t preq;
            response_t presp;
            memset(&preq, 0, sizeof(preq));
            preq.type = CMD_PWD;
            if (ipc_send_request(to_kernel[1], &preq) == 0 &&
                ipc_recv_response(from_kernel[0], &presp) == 0 &&
                presp.status == ERR_OK) {
                size_t len = strlen(presp.output);
                if (len && presp.output[len - 1] == '\n')
                    presp.output[len - 1] = '\0';
                snprintf(prompt, sizeof(prompt), "mdos:%s > ", presp.output);
            }
        }
    }

    close(to_kernel[1]);
    close(from_kernel[0]);
    waitpid(g_kernel_pid, NULL, 0);
    return 0;
}
