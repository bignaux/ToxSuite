#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tox/tox.h>
#include "misc.h"

#define MINLINE    50 /* IP: 7 + port: 5 + key: 38 + spaces: 2 = 70. ! (& e.g. tox.im = 6) */
#define MAXLINE   256 /* Approx max number of chars in a sever line (name + port + key) */
#define MAXSERVERS 50
#define SERVERLEN (MAXLINE - TOX_PUBLIC_KEY_SIZE - 7)

/*
 * Get a configuration value from a key=value configuration file
 * Returns pointer to allocated memory if success
 * Return NULL if keyname is not found or on error
 */
char *get_conf_str(FILE *fp, const char *keyname)
{
	if (fp == NULL)
		return NULL;
	fseek(fp, 0, SEEK_SET);
	
	char line[4096];
	while (fgets(line, sizeof(line), fp))
	{
		char *key = strtok(line, "=");
		char *value = strtok(NULL, "\n");
		/* invalid line */
		if (key == NULL || value == NULL)
			continue;
		/* not what we are looking for or commented out */
		if (strcmp(keyname, key) != 0 || strchr(key, '#') != NULL)
			continue;
		
		return strdup(value);
	}
	
	return NULL;
}

/*
 * Get a positive number value from a key=value configuration file
 * Returns integer >= 0 if success
 * Return -1 if keyname is not found or on error
 */
int get_conf_int(FILE *fp, const char *keyname)
{
	char *strnum = get_conf_str(fp, keyname);
	if (strnum == NULL)
		return -1;
	
	int number = atoi(strnum);
	if (number == 0 && strcmp(strnum, "0") != 0)
		number = -1;
	
	free(strnum);
	return number;
}
