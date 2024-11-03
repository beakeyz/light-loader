#include "boot/cldr.h"
#include "gfx.h"
#include "stddef.h"
#include "ui/button.h"
#include "ui/component.h"
#include <ctx.h>
#include <font.h>
#include <ui/screens/home.h>

/*
 * Click handler to boot the default bootconfig
 */
int boot_default_config(button_component_t* component)
{
    light_gfx_t* gfx;
    get_light_gfx(&gfx);

    /* Reset the file paths to null in order to ensure the default paths */
    gfx->ctx->light_bcfg.kernel_file = NULL;
    gfx->ctx->light_bcfg.ramdisk_file = NULL;

    gfx->flags |= GFX_FLAG_SHOULD_EXIT_FRONTEND;
    return 0;
}

#define WELCOME_LABEL "Welcome to the Light Loader!"

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

    create_component(root, COMPONENT_TYPE_LABEL, WELCOME_LABEL, screen_center_x - (welcome_lbl_width >> 1), 50, welcome_lbl_width, gfx->current_font->height, nullptr);

    config_file_t* test = open_config_file("res/bcfg/default.cfg");

    config_node_t* test_node;

    if (config_file_get_node(test, "misc.name", &test_node) == 0)
        gfx_printf(test_node->str_value);

    for (;;) { }

    create_button(root, "Boot default", screen_center_x - (boot_btn_width >> 1), gfx->height - boot_btn_height - 8, boot_btn_width, boot_btn_height, boot_default_config, nullptr);
    return 0;
}
