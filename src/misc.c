/*    Copyright (C) 2016  Ronan Bignaux
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "misc.h"

#include <stdarg.h>
#include <ctype.h> // toupper
//#include "file.h"

#ifdef CAPTCHA
void generate_capcha(unsigned char gif[gifsize], unsigned char l[6])
{
    unsigned char im[70*200];

    captcha(im,l);
    makegif(im,gif);
}
#endif

/* return value needs to be freed */
char *human_readable_id(uint8_t *address, uint16_t length)
{
    char id[length * 2 + 1];
    sodium_bin2hex(id, sizeof(id),address, length);
    return strdup(id);
}

/* cut absolute pathname to filename */
char *gnu_basename(char *path)
{
    char *base = strrchr(path, '/');
    return base ? base+1 : path;
}

/* dest must be at least 8 bytes */
/* the output size isn't accurate */
void human_readable_filesize(char *dest, off_t size)
{
    char unit_letter[] = " KMGTPEZY";
    int unit_prefix = 0;

    while(size > 9999)
    {
        size /= 1024;
        unit_prefix++;
    }

    snprintf(dest, 8, "%lu %cb", size, unit_letter[unit_prefix]);
}

ssize_t toxstream_write(void *c, const char *message, size_t size)
{
    struct toxstream_cookie *cookie = c;
    TOX_ERR_FRIEND_SEND_MESSAGE error;
    size_t length, offset;

    for ( offset = 0; offset < size; offset += length) {
        length = size - offset > TOX_MAX_MESSAGE_LENGTH ? TOX_MAX_MESSAGE_LENGTH : size - offset;
                tox_friend_send_message(cookie->tox, cookie->friend_number,
                                        cookie->type, (uint8_t*)message + offset, length, &error);
        if (error != TOX_ERR_FRIEND_SEND_MESSAGE_OK) {
            perror("tox_friend_send_message");
            return 0;
        }
    }
    return offset;
}

void print_option(struct Tox_Options *options)
{
    fprintf(stderr, "%s %s Port range : [%d, %d]. \n", options->ipv6_enabled ? "ipv6 enabled," : "",
           options->udp_enabled ? " UDP enabled," : "", options->start_port, options->end_port);
}

void print_secretkey(FILE *stream, const Tox *tox)
{
    uint8_t secret_keybin[TOX_SECRET_KEY_SIZE];
    char secret_keyhex[TOX_ADDRESS_SIZE * 2 + 1];
    tox_self_get_secret_key(tox, secret_keybin);

    sodium_bin2hex(secret_keyhex, sizeof(secret_keyhex), secret_keybin, TOX_SECRET_KEY_SIZE);
    for (size_t i = 0; i < sizeof(secret_keyhex) - 1; i++) {
        secret_keyhex[i] = toupper(secret_keyhex[i]);
    }
    fprintf(stream, "Secret key : %s\n",secret_keyhex);
}

void client_info_new(struct client_info *own_info)
{
    own_info->name = NULL;
    own_info->status_message = NULL;
}

void client_info_destroy(struct client_info *own_info)
{
    free(own_info->name);
    free(own_info->status_message);
}

void client_info_update(const Tox *apptox, struct client_info *own_info)
{
    size_t name_size;
    size_t status_message_size;

    name_size = tox_self_get_name_size(apptox);
    own_info->name = realloc(own_info->name, name_size * sizeof(uint8_t) + 1);
    tox_self_get_name(apptox, own_info->name);
    own_info->name[name_size] = '\0';

    tox_self_get_address(apptox, own_info->tox_id_bin);
    sodium_bin2hex(own_info->tox_id_hex, sizeof(own_info->tox_id_hex), own_info->tox_id_bin, sizeof(own_info->tox_id_bin));
    for (size_t i = 0; i < sizeof(own_info->tox_id_hex) - 1; i++) {
        own_info->tox_id_hex[i] = toupper(own_info->tox_id_hex[i]);
    }

    own_info->connect = tox_self_get_connection_status(apptox);

    status_message_size = tox_self_get_status_message_size(apptox);
    own_info->status_message = realloc(own_info->status_message, status_message_size * sizeof(uint8_t) + 1);
    tox_self_get_status_message(apptox, own_info->status_message);
    own_info->status_message[status_message_size] = '\0';
}

void client_info_print(FILE *stream, const struct client_info *own_info)
{
    fprintf(stream,"Name : %s\n", own_info->name);
    fprintf(stream,"tox:%s\n", own_info->tox_id_hex);
    fprintf(stream,"----------------------------------- Status message -----------------------------------\n");
    fprintf(stream,"%s\n", own_info->status_message);
    fprintf(stream,"--------------------------------------------------------------------------------------\n");
}

// this function returns minimum of integer numbers passed.  First
// argument is count of numbers.
int min(int arg_count, ...)
{
    int i;
    int min, a;

    // va_list is a type to hold information about variable arguments
    va_list ap;

    // va_start must be called before accessing variable argument list
    va_start(ap, arg_count);

    // Now arguments can be accessed one by one using va_arg macro
    // Initialize min as first argument in list
    min = va_arg(ap, int);

    // traverse rest of the arguments to find out minimum
    for(i = 2; i <= arg_count; i++) {
        if((a = va_arg(ap, int)) < min)
            min = a;
    }

    //va_end should be executed before the function returns whenever
    // va_start has been previously used in that function
    va_end(ap);

    return min;
}

/* ssssshhh I stole this from ToxBot, don't tell anyone.. */
void get_elapsed_time_str(char *buf, int bufsize, uint64_t secs)
{
        long unsigned int minutes = (secs % 3600) / 60;
        long unsigned int hours = (secs / 3600) % 24;
        long unsigned int days = (secs / 3600) / 24;

        snprintf(buf, bufsize, "%lud %luh %lum", days, hours, minutes);
}


//bool load_json_profile(Tox **tox, struct Tox_Options *options)
//{
//    FILE *file = fopen("config.json", "rb");
//    jsmn_parser parser;
//    jsmntok_t tokens[256];

//    char keyname[50];
////    size_t szkey = 0;


//    char *line = NULL;
//    char *js = NULL;
//    size_t len = 0;
//    size_t blen = 0;
//    size_t offset = 0;
//    int read, r = 0;
//    int j = 0, i = 0;

//    char ** masterkeys = NULL;

//    //    enum jsmnerr {
//    //        /* Not enough tokens were provided */
//    //        JSMN_ERROR_NOMEM = -1,
//    //        /* Invalid character inside JSON string */
//    //        JSMN_ERROR_INVAL = -2,
//    //        /* The string is not a full JSON packet, more bytes expected */
//    //        JSMN_ERROR_PART = -3
//    //    };


//    jsmn_init(&parser);


//    if (file) {

//        /* TODO : improve parsing against jsmn_parse()
//         *
//         */
//        yinfo("read a json bloc");
//        // read a json bloc
//        while ( (read = getline(&line, &len, file)) != -1 ) {
//            offset = blen;
//            blen += read;
//            js = realloc(js, blen);
//            strncpy(js + offset, line, read);
//        }
//        free(line);

//        r = jsmn_parse(&parser, js, blen, tokens, 256);
//        while ( i < r ) {
//            if ( tokens[i].type == JSMN_PRIMITIVE ) {
//                strncpy(keyname, js, tokens[i].end - tokens[i].start + 1);
//                keyname[tokens[i].end - tokens[i].start ] = '\0';
//                yinfo("key:%s", keyname);
//            }else if ( tokens[i].type == JSMN_ARRAY ) {
//                masterkeys = realloc(masterkeys, sizeof(char*) * tokens[i].size); // resize array of strings
//            }else if ( tokens[i].type == JSMN_STRING) {
//                masterkeys[j] = realloc( masterkeys[j],  tokens[i].end - tokens[i].start );
//                strncpy( masterkeys[j], js + tokens[i].start, tokens[i].end - tokens[i].start );
//                j++;
//            }
//            i++;
//        }
//        fclose(file);
//    }

//    for (int k = 0; k < j; k++) {
//        yinfo("%d keys : %s", k, masterkeys[k]);
//    }
//    return false;
//}
