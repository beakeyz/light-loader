#include "file.h"
#include "stddef.h"
#include <boot/cldr.h>
#include <heap.h>
#include <memory.h>

static toml_token_t* create_toml_token(config_file_t* file, const char* tok, size_t tok_len, enum TOML_TOKEN_TYPE type)
{
    toml_token_t* ret;

    ret = allocate_token(file->tok_cache);

    if (!ret)
        return ret;

    ret->tok = strdup_ex(tok, tok_len);
    ret->type = type;
    ret->next = nullptr;

    return ret;
}

static void link_toml_token(config_file_t* file, toml_token_t* token)
{
    *file->tok_enq_ptr = token;
    file->tok_enq_ptr = &token->next;

    file->nr_tokens++;
}

/*!
 * @brief: Parse the TOML in a config file to a tree of nodes
 *
 *
 */
static inline void __parse_config_file(config_file_t* file)
{
    /* The char we're currently on */
    char* c_char;
    /* The start character of the current token we're parsing */
    char* token_start = nullptr;
    /* The previous token we've parsed */
    toml_token_t* prev_token = nullptr;
    /* Type of the current token */
    enum TOML_TOKEN_TYPE cur_type = TOML_TOKEN_TYPE_UNKNOWN;
    /* Buffer containing all the chars */
    char* file_buffer = heap_allocate(file->file->filesize);

    if (!file_buffer)
        return;

    if (file->file->f_readall(file->file, file_buffer))
        goto free_and_exit;

    for (uint64_t i = 0; i < file->file->filesize; i++) {
        c_char = &file_buffer[i];

        /* Determine which kind of token we're working on */
        switch (*c_char) {
        case '[':
            cur_type = TOML_TOKEN_TYPE_GROUP_START;

            /* A '[' after an assign means we're dealing with a list */
            if (prev_token && prev_token->type == TOML_TOKEN_TYPE_ASSIGN)
                cur_type = TOML_TOKEN_TYPE_LIST_START;
            break;
        case ']':
            break;
        case '{':
            break;
        case '}':
            break;
        case '=':
            break;
        case ',':
            break;
        case '\n':
            break;
        default:
            if (c_isletter(*c_char)) {
            } else if (c_isdigit(*c_char)) {
            }
            break;
        }
    }

free_and_exit:
    heap_free(file_buffer);
}

config_file_t* open_config_file(const char* path)
{
    light_file_t* file;
    config_file_t* ret;

    if (!path)
        return nullptr;

    /* Make sure the file ends in .toml */
    if (strncmp(&path[strlen(path) - 5], ".toml", 4))
        return nullptr;

    file = fopen((char*)path);

    if (!path)
        return nullptr;

    ret = heap_allocate(sizeof(*ret));

    if (!ret) {
        file->f_close(file);
        return nullptr;
    }

    /* Set the file */
    ret->file = file;
    ret->rootnode = nullptr;
    ret->tok_cache = heap_allocate(TOML_TOKEN_CACHE_SIZE);
    ret->tokens = nullptr;
    ret->tok_enq_ptr = &ret->tokens;

    /* Check if we are able to allocate this memory */
    if (!ret->tok_cache) {
        heap_free(ret);
        file->f_close(file);
        return nullptr;
    }

    memset(ret->tok_cache, 0, TOML_TOKEN_CACHE_SIZE);

    /* Parse the TOML */
    __parse_config_file(ret);

    return ret;
}

void close_config_file(config_file_t* file);

int config_file_get_node(const char* key, config_node_t** pnode);
