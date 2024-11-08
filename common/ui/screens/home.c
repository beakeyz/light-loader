#include "boot/boot.h"
#include "boot/cldr.h"
#include "gfx.h"
#include "stddef.h"
#include "ui/button.h"
#include "ui/component.h"
#include <ctx.h>
#include <font.h>
#include <ui/screens/home.h>

#define WELCOME_LABEL "Welcome to the Light Loader!"
#define CONFIG_BTN_HEIGHT 64
#define CONFIG_BTN_WIDTH 512
#define MAX_NR_CONFIGS 32

static config_file_t* current_cfg;
static config_file_t* available_cfg_files[MAX_NR_CONFIGS];

/*
 * Click handler to boot the default bootconfig
 */
int boot_selected_config(button_component_t* component)
{
    light_boot_config_t* bootconfig;
    light_gfx_t* gfx;

    if (!current_cfg)
        return 0;

    /* Grab the GFX context */
    get_light_gfx(&gfx);

    /* Grab the bootconfig */
    bootconfig = &gfx->ctx->light_bcfg;

    /* Configure the current bootconfig with this file */
    boot_config_from_file(bootconfig, current_cfg);

    /* Close all config files, since we've chosen which one we want to use */
    for (uint32_t i = 0; i < MAX_NR_CONFIGS; i++) {
        if (available_cfg_files[i])
            break;

        close_config_file(available_cfg_files[i]);
    }

    /* Zero this fucker */
    current_cfg = nullptr;

    gfx->flags |= GFX_FLAG_SHOULD_EXIT_FRONTEND;
    return 0;
}

static int __on_config_select_click(button_component_t* comp)
{
    config_file_t* file;

    /* Grab the config file from the button component */
    file = (config_file_t*)comp->private;

    /* Prevent selects when current_cfg == null, in order to avoid weirdness */
    if (!file || !current_cfg)
        return -1;

    /* Set the current config */
    current_cfg = file;

    return 0;
}

static inline char* __get_quick_node_string(config_file_t* file, const char* path)
{
    config_node_t* n = nullptr;

    if (config_file_get_node(file, path, &n) || !n || n->type != CFG_NODE_TYPE_STRING)
        return nullptr;

    /* Return thing */
    return (char*)n->str_value;
}

static int __on_select_btn_draw(light_component_t* comp)
{
    config_file_t* cfg;
    char* version_str;
    button_component_t* btn = comp->private;

    /* =( */
    if (!btn)
        return 0;

    /* Grab the config */
    cfg = (config_file_t*)btn->private;

    /* Default box */
    gfx_draw_rect(comp->gfx, comp->x, comp->y, comp->width, comp->height, GRAY);

    /* Draw hovered state */
    if (component_is_hovered(comp))
        gfx_draw_rect_outline(comp->gfx, comp->x, comp->y, comp->width, comp->height, BLACK);

    /* Red outline for the selected config */
    if (cfg == current_cfg)
        gfx_draw_rect_outline(comp->gfx, comp->x + 1, comp->y + 1, comp->width - 2, comp->height - 2, RED);

    version_str = __get_quick_node_string(cfg, "misc.version");

    if (version_str) {
        /* Draw version, if it has it */
        component_draw_string_at(comp, "Version:", 6, 20, WHITE);
        component_draw_string_at(comp, version_str, 76, 20, WHITE);
    }

    /* Draw desc, if it has it */
    component_draw_string_at(comp, __get_quick_node_string(cfg, "misc.desc"), 6, 44, WHITE);

    if (!comp->label)
        return 0;

    component_draw_string_at(comp, "Name: ", 6, 4, WHITE);

    return component_draw_string_at(comp, comp->label, 50, 4, WHITE);
}

static void __try_add_config_button(light_component_t** root, config_file_t* file, uint32_t* current_y)
{
    config_node_t* name_node = nullptr;
    config_node_t *k_img_node, *rd_img_node;
    button_component_t* btn;

    /* Yikes */
    if (config_file_get_node(file, "misc.name", &name_node) || !name_node)
        return;

    if (config_file_get_node(file, "boot.kernel_image", &k_img_node) || !k_img_node)
        return;

    if (config_file_get_node(file, "boot.ramdisk_image", &rd_img_node) || !rd_img_node)
        return;

    /* Create a labelless buttion */
    btn = create_button(root, (char*)name_node->str_value, 24, *current_y, CONFIG_BTN_WIDTH, CONFIG_BTN_HEIGHT, __on_config_select_click, __on_select_btn_draw);

    /* Fuck */
    if (!btn)
        return;

    /* Let the button know about the file */
    btn->private = (uintptr_t)file;

    // Also add some padding
    *current_y += (CONFIG_BTN_HEIGHT + 8);
}

/*
 * Construct the homescreen
 *
 * Positioning will be done manually, which is the justification for the amount of random numbers in this
 * funciton. This makes the development of screens rather unscalable, but it is nice and easy / simple
 *
 * I try to be explicit in the use of these coordinates, but that does not make it any more scalable, just
 * a little easier to see the logic in them
 */
int construct_homescreen(light_component_t** root, light_gfx_t* gfx)
{
    uint32_t screen_center_x = (gfx->width >> 1);
    uint32_t screen_center_y = (gfx->height >> 1);

    const uint32_t boot_btn_width = 120;
    const uint32_t boot_btn_height = 28;
    const uint32_t welcome_lbl_width = lf_get_str_width(gfx->current_font, WELCOME_LABEL);

    /* Reset these fuckers */
    current_cfg = nullptr;
    memset(available_cfg_files, 0, sizeof(available_cfg_files));

    /* Create a welcome lable */
    create_component(root, COMPONENT_TYPE_LABEL, WELCOME_LABEL, screen_center_x - (welcome_lbl_width >> 1), 50, welcome_lbl_width, gfx->current_font->height, nullptr);

    uint32_t idx = 1;
    uint32_t current_y = 100;
    config_file_t* cfg = open_config_file_idx("res/bcfg", 0);

    /* TODO: Add buttons for each boot configuration, so the user can select wich one they want to use */
    if (cfg) {
        /* Select the first config by default */
        current_cfg = cfg;

        do {
            /*
             * Try to add the config
             * If the file is invalid, this function should catch it
             */
            __try_add_config_button(root, cfg, &current_y);

            /* Store this fucker */
            available_cfg_files[idx - 1] = cfg;

            cfg = open_config_file_idx("res/bcfg", idx++);
        } while (cfg && idx < MAX_NR_CONFIGS);
    }

    create_button(root, "Boot Selected", screen_center_x - (boot_btn_width >> 1), gfx->height - boot_btn_height - 8, boot_btn_width, boot_btn_height, boot_selected_config, nullptr);
    return 0;
}
