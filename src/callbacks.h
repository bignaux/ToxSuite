#ifndef CALLBACKS_H
#define CALLBACKS_H

int group_chat_init(Tox *m);
void on_request(uint8_t *key, uint8_t *data, uint16_t length, void *m);
void on_message(Tox *tox, uint32_t friend_number, TOX_MESSAGE_TYPE type, const uint8_t *message, size_t length, void *user_data);
void on_new_file(FileNode *fn, int);
#endif
