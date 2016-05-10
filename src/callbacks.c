#include <time.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <tox/tox.h>

#include "ylog/ylog.h"
#include "misc.h"
#include "fileop.h"
#include "filesend.h"

static int group_chat_number = -1;
static Tox *group_messenger;

/* START COMMANDS */
#define MAX_ARGS_SIZE 20
#define MAX_ARGS_NUM 3
#define	SYNTAX_ERR_MSG "Invalid syntax!"
#define NOTFND_ERR_MSG "Pack number not found"
#define NOTFND_F_MSG "Could not open requested file"
#define QUEUEFULL_F_MSG "The queue is full, try later"
#define SRVERR_F_MSG "Error sending file"
static void cmd_invite(Tox *m, int friendnum, int argc, char (*argv)[MAX_ARGS_SIZE])
{
    if(group_chat_number != -1)
    {
        int rc = tox_invite_friend(m, friendnum, group_chat_number);
        if(rc == -1)
            yerr("Failed to invite friend n°%i to groupchat n°%i", friendnum, group_chat_number);
    }
}

void cmd_help(Tox *m, int friendnum, int argc, char (*argv)[MAX_ARGS_SIZE])
{
    TOX_ERR_FRIEND_SEND_MESSAGE error;
    char helpstr[] = "\n Supported commands:\n\
            invite          Join the groupchat where new files get announced\n\
            send -1         Request a list of all the files available\n\
            send x          Request the file (replace x with pack number)\n\
            info x          Request details about the file (replace x with pack number)";
            tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) helpstr, strlen(helpstr), &error);
}

void cmd_info(Tox *m, int friendnum, int argc, char (*argv)[MAX_ARGS_SIZE])
{
    TOX_ERR_FRIEND_SEND_MESSAGE error;
    const char formatstr[] = "Pack Info for Pack #%i:\n\
            Filename       %s\n\
            Filesize       %lu [%s]\n\
            Last Modified  %s GMT\n\
            Blake2b        %s";

            /* formatstring + packn + filename + (size + numan_size) + gmtime + Blake2b*/
            char buf[sizeof(formatstr) + 20 + PATH_MAX + 28 + 26 + 64];
    char hu_size[8];
    FileNode *fn;
    FileNode **fnode = file_get_shared();
    int fnode_num = file_get_shared_len();
    char *blake, *timestr;
    int rc;

    if(argc != 1)
    {
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL,(uint8_t *) SYNTAX_ERR_MSG, strlen(SYNTAX_ERR_MSG), &error);
        return;
    }

    int packn = atoi(argv[1]);
    if((packn == 0 && strcmp(argv[1], "0") != 0) || packn < 0 || packn >= fnode_num || !fnode[packn]->exists)
    {
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) NOTFND_ERR_MSG, strlen(NOTFND_ERR_MSG), &error);
        return;
    }

    fn = fnode[packn];
    blake = human_readable_id(fn->BLAKE2b, TOX_FILE_ID_LENGTH);
    human_readable_filesize(hu_size, fn->length);
    timestr = asctime(gmtime(&fn->mtime));
    timestr[strlen(timestr) - 1] = '\0'; /* remove newline that asctime adds */

    rc = snprintf(buf, sizeof(buf), formatstr, packn, gnu_basename(fn->file), fn->length,
                  hu_size, timestr, blake);
    free(blake);

    if(rc > 0) {
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL,
                                (uint8_t *) buf, rc + 1, &error);
    }
    else
    {
        yerr("Failed to send info msg to friend n°%i", friendnum);
    }
}

static void cmd_send(Tox *m, int friendnum, int argc, char (*argv)[MAX_ARGS_SIZE])
{
    TOX_ERR_FRIEND_SEND_MESSAGE error;
    FileNode **fnode = file_get_shared();
    int fnode_num = file_get_shared_len();
    int rc;

    if(argc != 1)
    {
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) SYNTAX_ERR_MSG, strlen(SYNTAX_ERR_MSG), &error);
        return;
    }

    int packn = atoi(argv[1]);
    if((packn == 0 && strcmp(argv[1], "0") != 0) || packn < -1 || packn >= fnode_num || (packn != -1 && !fnode[packn]->exists))
    {
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) NOTFND_ERR_MSG, strlen(NOTFND_ERR_MSG), &error);
        return;
    }

    rc = file_sender_new(friendnum, fnode, packn, m);
    char *fname = packn == -1 ? packlist_filename : fnode[packn]->file;
    switch(rc)
    {
    case FILESEND_OK:
        yinfo("Sending file request to friend n°%i: %s", friendnum, fname);
        break;
    case FILESEND_ERR_FILEIO:
        yerr("I/O Error accessing file for friend n°%i: %s", friendnum, fname);
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) NOTFND_F_MSG, strlen(NOTFND_F_MSG), &error);
        break;
    case FILESEND_ERR_FULL:
        yerr("File queue full, could not send to friend n°%i: %s", friendnum, fname);
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) QUEUEFULL_F_MSG, strlen(QUEUEFULL_F_MSG), &error);
        break;
    case FILESEND_ERR_SENDING:
        yerr("Error creating new file request for friend n°%i: %s", friendnum, fname);
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) SRVERR_F_MSG, strlen(SRVERR_F_MSG), &error);
        break;
    default:
        yerr("Error sending file request to friend n°%i: %s", friendnum, fname);
        tox_friend_send_message(m, friendnum, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) SRVERR_F_MSG, strlen(SRVERR_F_MSG), &error);
    }
}

struct cmd_func
{
    const char *name;
    void (*func)(Tox *m, int friendnum, int argc, char (*argv)[MAX_ARGS_SIZE]);
};

#define COMMAND_NUM 4
static struct cmd_func bot_commands[] = {
{"invite", cmd_invite},
{"help", cmd_help},
{"info", cmd_info},
{"send", cmd_send}
};

/* Parses input command and puts args into arg array. 
   Returns number of arguments on success, -1 on failure. */
static int parse_command(char *cmd, char (*args)[MAX_ARGS_SIZE])
{
    int num_args = 0;
    int cmd_end = false;    // flags when we get to the end of cmd
    char *end;               // points to the end of the current arg

    while (!cmd_end && num_args < MAX_ARGS_NUM)
    {
        end = strchr(cmd, ' ');
        cmd_end = end == NULL;
        if (!cmd_end)
            *end++ = '\0';    /* mark end of current argument */
        /* Copy from start of current arg to where we just inserted the null byte */
        strcpy(args[num_args++], cmd);
        cmd = end;
    }

    return num_args;
}

void execute(Tox *m, char *cmd, int friendnum)
{
    char args[MAX_ARGS_NUM][MAX_ARGS_SIZE];
    memset(args, 0, sizeof(args));
    int num_args = parse_command(cmd, args);
    int i;

    for (i = 0; i < COMMAND_NUM; ++i)
    {
        if (strcmp(args[0], bot_commands[i].name) == 0)
        {
            (bot_commands[i].func)(m, friendnum, num_args - 1, args);
            return;
        }
    }
}
/* END COMMANDS */


int group_chat_init(Tox *m)
{
    group_messenger = m;
    group_chat_number = tox_add_groupchat(m);
    if(group_chat_number == -1)
        yerr("Failed to initialize groupchat");
    return group_chat_number;
}

void on_new_file(FileNode *fn, int packn)
{
    uint8_t groupmsg[sizeof("New file: # [9999  b]: %s") + PATH_MAX + 32];
    char hu_size[8];
    int rc;
    if(group_chat_number == -1)
        return;

    human_readable_filesize(hu_size, fn->length);

    rc = snprintf((char *)groupmsg, sizeof(groupmsg), "New file: #%i [%s] %s", packn, hu_size, gnu_basename(fn->file));
    if(rc < 0)
        goto errormsg;

    rc = tox_group_message_send(group_messenger, group_chat_number, groupmsg, rc + 1); /* length needs to include the terminator */
    if(rc == -1 && tox_group_number_peers(group_messenger, group_chat_number) > 1)
        goto errormsg;

    return;

errormsg:
    yerr("Failed to notify new file %s on groupchat", fn->file);
}


