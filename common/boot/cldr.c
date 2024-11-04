#include "file.h"
#include "gfx.h"
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

static inline void __tokenize_config_file(config_file_t* file)
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

        /* Reset these fuckers */
        token_start = c_char;
        cur_type = TOML_TOKEN_TYPE_UNKNOWN;

        /* Determine which kind of token we're working on */
        switch (*c_char) {
        case '[':
            cur_type = TOML_TOKEN_TYPE_GROUP_START;

            /* A '[' after an assign means we're dealing with a list */
            if (toml_token_istype(prev_token, TOML_TOKEN_TYPE_ASSIGN))
                cur_type = TOML_TOKEN_TYPE_LIST_START;
            break;
        case ']':
            cur_type = TOML_TOKEN_TYPE_LIST_END;

            if (toml_token_istype(prev_token, TOML_TOKEN_TYPE_IDENTIFIER))
                cur_type = TOML_TOKEN_TYPE_GROUP_END;
            break;
        case '{':
            cur_type = TOML_TOKEN_TYPE_OBJ_START;
            break;
        case '}':
            cur_type = TOML_TOKEN_TYPE_OBJ_END;
            break;
        case '=':
            cur_type = TOML_TOKEN_TYPE_ASSIGN;
            break;
        case ',':
            cur_type = TOML_TOKEN_TYPE_COMMA;
            break;
        case '\n':
            cur_type = TOML_TOKEN_TYPE_NEWLINE;
            break;
        /* Whitespace chars */
        case ' ':
        case '\t':
            break;
        default:
            if (c_isletter(*c_char)) {
                /* Find the last char in the sequence */
                while ((c_isletter(file_buffer[i]) || file_buffer[i] == '_') && i < file->file->filesize)
                    i++;

                c_char = &file_buffer[i - 1];
                cur_type = TOML_TOKEN_TYPE_IDENTIFIER;
            } else if (*c_char == '\"' || *c_char == '\'') {
                i++;

                while (file_buffer[i] != *token_start && file_buffer[i] != '\n' && i < file->file->filesize)
                    i++;

                /* Set the current char */
                c_char = &file_buffer[i - 1];
                token_start += 1;
                /* Skip the trailing string identifier */
                i++;

                /* Set the right type */
                cur_type = TOML_TOKEN_TYPE_STRING;
            }

            i--;
            break;
        }

        if (cur_type == TOML_TOKEN_TYPE_UNKNOWN)
            continue;

        toml_token_t* token = create_toml_token(file, token_start, c_char - token_start + 1, cur_type);

        if (token) {
            link_toml_token(file, token);

            prev_token = token;
        }
    }

free_and_exit:
    heap_free(file_buffer);
}

static inline config_node_t* create_config_node(const char* key, enum CFG_NODE_TYPE type, void* data)
{
    config_node_t* node;

    node = heap_allocate(sizeof(*node));

    if (!node)
        return nullptr;

    node->key = key;
    node->type = type;
    node->value = data;

    node->next = nullptr;

    return node;
}

/*!
 * @brief: Link a config node into a group node
 *
 * Since we don't have any fancy enqueue pointer magic here, just loop over the children until
 * we find the last one =(
 */
static inline void link_config_node(config_node_t** group_ptr, config_node_t* child)
{
    config_node_t** walker;

    if (!child || !group_ptr)
        return;

    walker = group_ptr;

    /* Walk to the last place */
    while (*walker)
        walker = &(*walker)->next;

    child->next = nullptr;
    *walker = child;
}

/*!
 * @brief: Parse the TOML in a config file to a tree of nodes
 *
 *
 */
static inline void __parse_config_file(config_file_t* file)
{
    config_node_t* new_node;
    config_node_t* cur_gnode = nullptr;
    toml_token_t* prev_token = nullptr;

    if (!file)
        return;

    /* Break up the file into small chunks */
    __tokenize_config_file(file);

    /* Create a rootnode */
    file->rootnode = create_config_node("root", CFG_NODE_TYPE_GROUP, nullptr);

    /* Loop over all the tokens to construct the node tree */
    for (toml_token_t* c_token = file->tokens; c_token != nullptr; c_token = c_token->next) {
        switch (c_token->type) {
        case TOML_TOKEN_TYPE_GROUP_START:
            /* This sucks bad */
            if (!toml_token_istype(c_token->next, TOML_TOKEN_TYPE_IDENTIFIER))
                return;

            /* Also very bad */
            if (!toml_token_istype(c_token->next->next, TOML_TOKEN_TYPE_GROUP_END))
                return;

            /* Yayyy a new group node */
            cur_gnode = new_node = create_config_node(c_token->next->tok, CFG_NODE_TYPE_GROUP, nullptr);

            /* Link this group node to the rootnodes group */
            link_config_node(&file->rootnode->group_value, new_node);

            /* Skip ahead two tokens */
            c_token = c_token->next->next;
            break;
        case TOML_TOKEN_TYPE_IDENTIFIER:
            if (!toml_token_istype(c_token->next, TOML_TOKEN_TYPE_ASSIGN))
                return;

            /* Fuck */
            if (!c_token->next->next)
                return;

            switch (c_token->next->next->type) {
            case TOML_TOKEN_TYPE_STRING:
                // ident = "str"
                new_node = create_config_node(c_token->tok, CFG_NODE_TYPE_STRING, (void*)c_token->next->next->tok);

                /* Link the node into it's group */
                link_config_node(&cur_gnode->group_value, new_node);
                break;
            case TOML_TOKEN_TYPE_NUM:
                // ident = num
                break;
            /* Invalid next token type */
            default:
                return;
            }

            /* Skip ahead two tokens */
            c_token = c_token->next->next;
            break;
        /* Any other token type is invalid lmao */
        default:
            break;
        }

        prev_token = c_token;
    }
}

config_file_t* open_config_file(const char* path)
{
    light_file_t* file;
    config_file_t* ret;

    if (!path)
        return nullptr;

    /* Make sure the file ends in .toml */
    if (strncmp(&path[strlen(path) - 4], ".cfg", 4))
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
    ret->tokens = nullptr;
    ret->tok_enq_ptr = &ret->tokens;
    ret->tok_cache = heap_allocate(TOML_TOKEN_CACHE_SIZE);

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

void close_config_file(config_file_t* file)
{
}

/*!
 * @brief: Grab a node from a parsed config file
 */
int config_file_get_node(config_file_t* file, const char* path, config_node_t** pnode)
{
    config_node_t* cur_node = file->rootnode;
    config_node_t* cur_group_list;
    const char* cur_path_subentry = path;
    uint32_t path_len;

    if (!file || !pnode)
        return -1;

    /* Root node is always a group node */
    if (!cur_node || cur_node->type != CFG_NODE_TYPE_GROUP)
        return -1;

    cur_group_list = cur_node->group_value;

    path_len = strlen(path);

    for (uint32_t i = 0; i < (path_len + 1); i++) {
        char cur_char = path[i];

        if (!cur_node || !cur_group_list)
            break;

        if (cur_char != '\0' && cur_char != '.')
            continue;

        /* Grab some info on the current path subentry */
        uint32_t node_name_len = &path[i] - cur_path_subentry;
        const char* node_name = strdup_ex(cur_path_subentry, node_name_len);

        /* Skip any fucking anoying dots */
        while (path[i + 1] == '.')
            i++;

        /* We're looking for a new node, clear the old one */
        cur_node = nullptr;

        /* Check all these nodes */
        for (config_node_t* n = cur_group_list; n; n = n->next) {
            if (strncmp(node_name, n->key, node_name_len))
                continue;

            /* Name match, this is our new current node */
            cur_node = n;

            /* If this node is also a group node, set that as well */
            if (cur_node->type == CFG_NODE_TYPE_GROUP)
                cur_group_list = cur_node->group_value;
            else
                cur_group_list = nullptr;

            break;
        }

        cur_path_subentry = &path[i + 1];
        heap_free((void*)node_name);
    }

    /* This should be caught by the check inside the loop, but just check again real quick */
    if (!cur_node)
        return -1;

    /* Export the node */
    *pnode = cur_node;

    return 0;
}

/*!
 * @brief: Gets a config node at index @idx
 *
 * @path: Path to a group node. If it does not point to a group node, we're very sad
 */
int config_file_get_node_at(config_file_t* file, const char* path, uint32_t idx, config_node_t** pnode)
{
    config_node_t* ret;
    config_node_t* group_node = nullptr;

    if (!file || !path || !pnode)
        return -1;

    /* Grab the group node */
    if (config_file_get_node(file, path, &group_node) || !group_node)
        return -1;

    /* Invalid path =( */
    if (group_node->type != CFG_NODE_TYPE_GROUP)
        return -1;

    /* Get the group list */
    ret = group_node->group_value;

    /* Loop over the nodes in this group */
    for (uint32_t i = 0; ret && i < idx; i++)
        ret = ret->next;

    if (!ret)
        return -1;

    *pnode = ret;
    return 0;
}
