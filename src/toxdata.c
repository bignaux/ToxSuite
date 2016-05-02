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

#include "toxdata.h"
#include "ylog/ylog.h"

#include <time.h>

#include <tox/tox.h>
#include <tox/toxencryptsave.h>
#include <sodium.h>

static bool shouldSaveConfig = false;

bool toxdata_get_shouldSaveConfig(void)
{
    return shouldSaveConfig;
}

void toxdata_set_shouldSaveConfig(bool saved)
{
    shouldSaveConfig = saved;
}

int load_profile(Tox **tox, struct Tox_Options *options, const char *data_filename, const char *passphrase)
{
    TOX_ERR_DECRYPTION decrypt_error;
    FILE *file = fopen(data_filename, "rb");
    bool encrypted;
    struct stat sb;
    size_t szread;

    uint8_t *cypher_data = NULL;
    uint8_t *save_data = NULL;
    int exit_code = EXIT_SUCCESS;

    if (!file)
        return -1;

    stat(data_filename, &sb);
    char * date = strdup(ctime((time_t*)&sb.st_mtim.tv_sec));
    date[strlen(date) -1] = '\0';

    size_t file_size = sb.st_size;

    save_data = malloc(file_size * sizeof(uint8_t));
    szread = fread(save_data, sizeof(uint8_t), file_size, file);
    if (szread != file_size) {
        ferror(file);
        yerr("An error occurred reading %s.",data_filename);
        fclose(file);
        file = NULL;
        exit_code = EXIT_FAILURE;
        goto cleanup;
    }
    fclose(file);

    encrypted = tox_is_data_encrypted(save_data);
    if (encrypted) {
        if (!passphrase) {
            ywarn("Encrypted profile, you must supply the passphrase.");
            exit_code = EXIT_FAILURE;
            goto cleanup;
        }
        cypher_data = malloc(file_size * sizeof(uint8_t));
        tox_pass_decrypt(save_data, file_size, (uint8_t*)passphrase, strlen(passphrase),
                         cypher_data, &decrypt_error);
        free(save_data);
        save_data = cypher_data;

        if (decrypt_error != TOX_ERR_DECRYPTION_OK) {
            ywarn("tox_pass_decrypt() error %d",decrypt_error);
            exit_code = EXIT_FAILURE;
            goto cleanup;
        }
    }

    options->savedata_data = save_data;
    options->savedata_type = TOX_SAVEDATA_TYPE_TOX_SAVE;
    options->savedata_length = file_size;

    // TODO move tox_new code and avoid have a **tox parameter here ?
    TOX_ERR_NEW err;
    *tox = tox_new(options, &err);

    if (err != TOX_ERR_NEW_OK)
        exit_code = EXIT_FAILURE;

    yinfo("%s %s loaded, last modification : %s",encrypted ? "Encrypted" : "clear", data_filename, date);

cleanup:
    free(date);
    free(save_data);
    return exit_code;
}

int save_profile(Tox *tox, const char *filename, const char *passphrase)
{
    TOX_ERR_ENCRYPTION err = 0;
    FILE *file;
    size_t clear_length     = tox_get_savedata_size(tox);
    size_t encrypted_length = clear_length + TOX_PASS_ENCRYPTION_EXTRA_LENGTH;
    uint8_t *clear_data = malloc(clear_length * sizeof(uint8_t));
    uint8_t *encrypted_data = NULL;

    tox_get_savedata(tox, clear_data);

    if(passphrase) {
       encrypted_data = malloc(encrypted_length * sizeof(uint8_t));
       tox_pass_encrypt(clear_data, clear_length,
                         (uint8_t*)passphrase, strlen(passphrase),
                         encrypted_data, &err);

       free(clear_data);

       if (err != TOX_ERR_ENCRYPTION_OK) {
           ywarn("tox_pass_encrypt() error %d",err);
           free(encrypted_data);
           return err;
       }

       clear_data = encrypted_data;
       clear_length = encrypted_length;
    }

    file = fopen(filename, "wb");
    if (!file) {
        free(clear_data);
        ywarn("Could not write data to disk");
        return -1;
    }
    fwrite(clear_data, sizeof(uint8_t), clear_length, file);
    free(clear_data);
    fclose(file);
    return 0;
}

int export_keys(const char *filename, struct list_head *friends_info)
{
    struct friend_info *f;
    FILE *file = fopen(filename, "wb");

    if (!file)
        return -1;

    list_for_each_entry(f, friends_info, list) {
        fwrite(f->tox_id_hex, sizeof(char), strlen(f->tox_id_hex), file);
        fwrite("\n", sizeof(char), 1, file);
    }
    fclose(file);
    return 0;
}

int import_keys(Tox *tox,struct list_head *friends_info, const char *filename)
{
    TOX_ERR_FRIEND_ADD error;
    FILE *file = fopen(filename, "r");
    ssize_t get;
    size_t len;
    char *public_key = NULL;
    uint8_t tox_id_bin[TOX_PUBLIC_KEY_SIZE];
    uint32_t friend_number;
    struct friend_info *f;

    if (!file)
        return -1;

    while ( (get = getline(&public_key, &len, file)) != -1 ) {

        if ( strlen(public_key) != TOX_PUBLIC_KEY_SIZE * 2 + 1)
            yinfo("%s is not a valid publickey %zu", public_key, strlen(public_key));
        else {
            sodium_hex2bin(tox_id_bin, TOX_PUBLIC_KEY_SIZE, public_key, strlen(public_key), NULL, NULL, NULL);
            friend_number = tox_friend_add_norequest(tox, tox_id_bin, &error);
            if ( error == TOX_ERR_FRIEND_ADD_OK) {
                f = friend_info_add_entry(friends_info, friend_number);
                f->name = (uint8_t *)strdup("_IMPORTED_KEY_"); // IMPROVE : encrypt key and init as a const.
                friend_info_update(tox, f); // IMPROVE : make no sense on a cold add.
            }
            else {
                ytrace("%s wont be added, error = %d.\n",public_key,error);
            }
        }
    }
    free(public_key);
    fclose(file);
    return 0;
}

int remove_keys(Tox *tox, struct list_head *friends_info, const char *filename)
{
    TOX_ERR_FRIEND_DELETE error;
    FILE *file = fopen(filename, "r");
    struct friend_info *f;
    ssize_t get;
    size_t len;
    char *public_key = NULL;

    if (!file)
        return -1;

    while ( (get = getline(&public_key, &len, file)) != -1 ) {

        if ( strlen(public_key) != TOX_PUBLIC_KEY_SIZE * 2 + 1)
            yinfo("%s is not a valid publickey %zu\n", public_key, strlen(public_key));
        else {
            f = friend_info_by_public_key(friends_info, public_key);
            if (!f)
                continue;

            tox_friend_delete(tox, f->friend_number, &error);

            if ( error == TOX_ERR_FRIEND_DELETE_OK ) {
                friend_info_destroy(f);
            } else {
            }
        }
    }
    free(public_key);
    fclose(file);
    return 0;
}
