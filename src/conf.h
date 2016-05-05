#ifndef CONF_H
#define CONF_H

char *get_conf_str(FILE *fp, const char *keyname);
int get_conf_int(FILE *fp, const char *keyname);

#endif
