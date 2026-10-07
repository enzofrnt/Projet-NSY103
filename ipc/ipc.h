#ifndef IPC_H
#define IPC_H

#include "../common/types.h"

/*
 * IPC Shell ↔ Noyau : tubes anonymes (pipes) après fork().
 *
 * On sérialise des structures complètes (request_t / response_t), sans
 * pointeurs. write()/read() peuvent être partiels sur de gros messages
 * (response_t ~8 Ko) : ipc_* utilise donc des boucles write_full/read_full.
 */

int  ipc_pipe_create(int fds[2]);
int  ipc_send_request(int fd, const request_t *req);
int  ipc_recv_request(int fd, request_t *req);
int  ipc_send_response(int fd, const response_t *resp);
int  ipc_recv_response(int fd, response_t *resp);

#endif
