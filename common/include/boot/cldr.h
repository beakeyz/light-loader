#ifndef __LIGHTLOADER_CONFIG_LOADER_H__
#define __LIGHTLOADER_CONFIG_LOADER_H__

#include "file.h"
#include "stddef.h"
#include <stdint.h>

struct config_node;

enum CFG_NODE_TYPE {
    CFG_NODE_TYPE_STRING,
    CFG_NODE_TYPE_NUM,
    CFG_NODE_TYPE_LIST,
    CFG_NODE_TYPE_GROUP,
};

/* Per-cache token max */
#define MAX_TOML_TOKENS 0x5000

/*
 * Token representation from the TOML config file
 */
typedef struct toml_token {
    const char* tok;
    enum TOML_TOKEN_TYPE {
        TOML_TOKEN_TYPE_GROUP_START, // '['
        TOML_TOKEN_TYPE_GROUP_END, // ']'
        TOML_TOKEN_TYPE_LIST_START, // '['
        TOML_TOKEN_TYPE_LIST_END, // ']'
        TOML_TOKEN_TYPE_IDENTIFIER, // Anything that gives some value a name
        TOML_TOKEN_TYPE_ASSIGN, // '='
        TOML_TOKEN_TYPE_STRING, // Anyting in between "" or ''
        TOML_TOKEN_TYPE_NUM, // Just any number lmao
        TOML_TOKEN_TYPE_OBJ_START, // '{'
        TOML_TOKEN_TYPE_OBJ_END, // '}'
        TOML_TOKEN_TYPE_COMMA, // ','
        TOML_TOKEN_TYPE_NEWLINE, // '\n', EOS (end of statement)

        TOML_TOKEN_TYPE_UNKNOWN,
    } type;
    struct toml_token* next;
} toml_token_t;

static inline bool toml_token_istype(toml_token_t* token, enum TOML_TOKEN_TYPE type)
{
    return (token && token->type == type);
}

typedef struct toml_token_cache {
    size_t next_free_idx;
    /* When next_free_idx == MAX_TOML_TOKENS, append a new cache here lol */
    struct toml_token_cache* next;
    toml_token_t cache[];
} toml_token_cache_t;

#define TOML_TOKEN_CACHE_SIZE sizeof(toml_token_cache_t) + (sizeof(toml_token_t) * MAX_TOML_TOKENS)

static inline toml_token_t* allocate_token(toml_token_cache_t* cache)
{
    if (!cache || cache->next_free_idx == MAX_TOML_TOKENS)
        return nullptr;

    return &cache->cache[cache->next_free_idx++];
}

/*
 * Single TOML/YAML (idk) node
 */
typedef struct config_node {
    const char* key;
    enum CFG_NODE_TYPE type;
    union {
        const char* str_value;
        uintptr_t num_value;
        struct config_node* list_value;
        struct config_node* group_value;

        void* value;
    };
    struct config_node* next;
} config_node_t;

typedef struct config_file {
    light_file_t* file;

    toml_token_cache_t* tok_cache;
    /* List of the tokens that came from this bastard */
    toml_token_t* tokens;
    size_t nr_tokens;
    toml_token_t** tok_enq_ptr;
    /* List of nodes that came from this bastard */
    config_node_t* rootnode;
} config_file_t;

extern config_file_t* open_config_file(const char* path);
extern config_file_t* open_config_file_idx(const char* path, uint32_t idx);
extern void close_config_file(config_file_t* file);

int config_file_get_node(config_file_t* file, const char* path, config_node_t** pnode);
int config_file_get_node_at(config_file_t* file, const char* path, uint32_t idx, config_node_t** pnode);

#endif // !__LIGHTLOADER_CONFIG_LOADER_H__
