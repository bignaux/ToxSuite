#ifndef CALLBACKS_H
#define CALLBACKS_H

void execute(Tox *m, char *cmd, int friendnum);
int group_chat_init(Tox *m);
void on_request(uint8_t *key, uint8_t *data, uint16_t length, void *m);
void on_new_file(FileNode *fn, int);
#endif
