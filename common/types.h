#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stddef.h>

/*
 * Types partagés Shell / Noyau / SGF.
 * Les messages IPC (request_t, response_t) ne contiennent aucun pointeur :
 * ils sont sérialisables tels quels à travers un pipe.
 */

/* Paramètres du disque virtuel */
#define BLOCK_SIZE          4096u
#define MAX_INODES          128u
#define MAX_DATA_BLOCKS     256u
#define DIRECT_BLOCKS       12u
#define MAX_FILENAME        28u
#define MAX_PATH            256u
#define MAX_ARGS            8u
#define MAX_ARG_LEN         128u
#define MAX_OUTPUT          8192u
#define FS_MAGIC            0x4D444F53u  /* "MDOS" */
#define ROOT_INODE          0u

#define DISK_PATH           "data/disque.img"

/* Types d'objets */
typedef enum {
    INODE_FREE = 0,
    INODE_FILE = 1,
    INODE_DIR  = 2
} inode_type_t;

/* Modes d'ouverture */
#define MODE_READ   0x01
#define MODE_WRITE  0x02
#define MODE_CREATE 0x04
#define MODE_TRUNC  0x08

/* Droits simplifiés : utilisateur / autres */
#define PERM_UREAD  0x0100
#define PERM_UWRITE 0x0080
#define PERM_UEXEC  0x0040
#define PERM_OREAD  0x0004
#define PERM_OWRITE 0x0002
#define PERM_OEXEC  0x0001
#define PERM_DEFAULT_FILE (PERM_UREAD | PERM_UWRITE | PERM_OREAD)
#define PERM_DEFAULT_DIR  (PERM_UREAD | PERM_UWRITE | PERM_UEXEC | PERM_OREAD | PERM_OEXEC)

/* Codes d'erreur communs */
typedef enum {
    ERR_OK = 0,
    ERR_NOT_FOUND,
    ERR_EXISTS,
    ERR_NO_SPACE,
    ERR_BAD_TYPE,
    ERR_NOT_EMPTY,
    ERR_INVALID_PATH,
    ERR_IO,
    ERR_PERM,
    ERR_BUSY,
    ERR_ARGS,
    ERR_UNKNOWN
} err_t;

/* Types de commandes (interprétés une fois par le Shell) */
typedef enum {
    CMD_NONE = 0,
    CMD_LS,
    CMD_MKDIR,
    CMD_RMDIR,
    CMD_CD,
    CMD_CP,
    CMD_RM,
    CMD_MV,
    CMD_CAT,
    CMD_ECHO,
    CMD_DF,
    CMD_PWD,
    CMD_HELP,
    CMD_EXIT
} cmd_type_t;

/* Superbloc (1 bloc) */
typedef struct {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t free_blocks;
    uint32_t total_inodes;
    uint32_t free_inodes;
    uint32_t inode_bitmap_block;
    uint32_t block_bitmap_block;
    uint32_t inode_table_block;
    uint32_t data_start_block;
    uint32_t inode_size;
    uint32_t root_inode;
    uint8_t  reserved[BLOCK_SIZE - 48];
} superblock_t;

/* Inode — allocation indexée : 12 directs + 1 bloc d'indirection simple */
typedef struct {
    uint32_t id;
    uint16_t type;
    uint16_t mode;
    uint32_t size;
    uint32_t nlinks;
    uint32_t direct[DIRECT_BLOCKS];
    uint32_t indirect;              /* 0 = pas encore alloué */
    uint32_t reserved[4];
} inode_t;

/* Entrée de répertoire */
typedef struct {
    char     name[MAX_FILENAME];
    uint32_t inode;
} dirent_t;

/* Requête Shell → Noyau (pas de pointeurs) */
typedef struct {
    cmd_type_t type;
    int        argc;
    char       args[MAX_ARGS][MAX_ARG_LEN];
    int        redirect;                 /* 1 si echo ... > fichier */
    char       redir_path[MAX_ARG_LEN];
} request_t;

/* Réponse Noyau → Shell */
typedef struct {
    err_t  status;
    char   output[MAX_OUTPUT];
    int    cwd_changed;
    uint32_t new_cwd;                    /* inode du nouveau cwd si cwd_changed */
} response_t;

#endif /* TYPES_H */
