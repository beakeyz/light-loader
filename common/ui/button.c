#include "font.h"
#include "gfx.h"
#include "image.h"
#include "mouse.h"
#include "ui/component.h"
#include <heap.h>
#include <memory.h>
#include <stddef.h>
#include <ui/button.h>

static bool
btn_should_update(light_component_t* component)
{
    bool hovered;
    button_component_t* btn = component->private;

    if (component->should_update)
        return true;

    hovered = component_is_hovered(component);

    /* Hovering currently */
    if (hovered)
        btn->was_hovered = true;
    /* Not hovering, but did hover previous update */
    else if (btn->was_hovered)
        return true;
    else
        return false;

    /* Make sure this button is clickable lmao */
    if (!btn->f_click_func)
        return false;

    if ((is_lmb_clicked(*component->mouse_buffer) || is_rmb_clicked(*component->mouse_buffer)) && !btn->is_clicked) {
        btn->is_clicked = true;
        button_click(btn);
    } else if (btn->is_clicked)
        btn->is_clicked = false;

    return true;
}

button_component_t*
create_button(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, int (*onclick)(button_component_t* this), int (*ondraw)(light_component_t* this))
{
    light_component_t* parent = create_component(link, COMPONENT_TYPE_BUTTON, label, x, y, width, height, ondraw);
    button_component_t* btn = heap_allocate(sizeof(button_component_t));

    memset(btn, 0, sizeof(button_component_t));

    btn->parent = parent;
    btn->f_click_func = onclick;
    btn->type = BTN_TYPE_NORMAL;

    parent->f_should_update = btn_should_update;
    parent->private = btn;

    return btn;
}

int button_click(button_component_t* btn)
{
    if (!btn || !btn->f_click_func)
        return -1;

    return btn->f_click_func(btn);
}

static int
draw_tab_btn(light_component_t* c)
{
    button_component_t* btn = c->private;
    struct tab_btn_private* priv = (struct tab_btn_private*)btn->private;

    gfx_draw_rect(c->gfx, c->x, c->y, c->width, c->height, LIGHT_GRAY);

    if (component_is_hovered(c)) {
        gfx_draw_rect_outline(c->gfx, c->x, c->y, c->width, c->height, BLACK);
        gfx_draw_rect(c->gfx, c->x, c->y + c->height - 2, c->width, 2, BLACK);
        gfx_draw_rect(c->gfx, c->x + c->width - 2, c->y, 2, c->height, BLACK);
    }

    if (priv->icon_image) {
        draw_image(c->gfx, c->x, c->y, priv->icon_image);
    } else if (c->label) {
        uint32_t label_x = (c->width >> 1) - (lf_get_str_width(c->gfx->current_font, c->label) >> 1);
        uint32_t label_y = (c->height >> 1) - (c->gfx->current_font->height >> 1);
        component_draw_string_at(c, c->label, label_x, label_y, WHITE);
    }
    return 0;
}

button_component_t*
create_tab_button(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, int (*onclick)(button_component_t* this), uint32_t target_index)
{
    struct tab_btn_private* priv;
    button_component_t* ret;

    if (!link)
        return nullptr;

    ret = create_button(link, label, x, y, width, height, onclick, draw_tab_btn);

    if (!ret)
        return nullptr;

    priv = heap_allocate(sizeof(*priv));

    priv->index = target_index;
    priv->icon_image = load_bmp_image(label);

    /* For tabs, we can simply store the target index directly in ->private */
    ret->private = (uint64_t)priv;
    ret->type = BTN_TYPE_TAB;

    return ret;
}

/*!
 * @brief: Flick that switch
 *
 * This is called right before we update the pixels, so we can simply
 * toggle the boolean value and the draw function will update accordingly
 */
int switch_onclick(button_component_t* comp)
{
    /* Return value of this function is often ignored, but still */
    if (comp->type != BTN_TYPE_SWITCH)
        return -1;

    bool* value = (bool*)comp->private;

    if (!value)
        return -1;

    /* Epic toggle */
    *value = !(*value);

    return 0;
}

/*
 * ------------------------------------------------
 * <label>                                  [-/O] | <height>
 * -----------------------------------------------
 *                      <width>
 * This is kinda how the switch should look (schematically)
 */
int switch_draw(light_component_t* comp)
{
    bool status;
    button_component_t* btn;
    const uint32_t switch_width = 16;
    const uint32_t switch_height = 16;
    const uint32_t switch_padding = 8;
    const uint32_t switch_x = comp->x + comp->width - switch_width - switch_padding;
    const uint32_t switch_y = comp->y + comp->height - switch_height - (comp->height - switch_height) / 2;

    /* Fuck you lmao */
    if (switch_height > comp->height)
        return -1;

    /* Yikes lol */
    if (comp->type != COMPONENT_TYPE_BUTTON)
        return -1;

    btn = comp->private;
    status = *(bool*)btn->private;

    /* Gray box =) */
    gfx_draw_rect(comp->gfx, comp->x, comp->y, comp->width, comp->height, GRAY);
    /* Black outline =) */
    gfx_draw_rect_outline(comp->gfx, comp->x, comp->y, comp->width, comp->height, BLACK);

    /* If there is a label, we'll draw that first on the left side */
    if (comp->label) {
        uint32_t label_draw_x = 4;
        uint32_t label_draw_y = (comp->height >> 1) - (comp->gfx->current_font->height >> 1);

        component_draw_string_at(comp, comp->label, label_draw_x, label_draw_y, WHITE);
    }

    /* Switch box */
    gfx_draw_rect(comp->gfx, switch_x, switch_y, switch_width, switch_height, DARK_GRAY);

    if (comp->gfx->_checkbox_image)
        draw_image(comp->gfx, switch_x, switch_y, comp->gfx->_checkbox_image);

    if (status && comp->gfx->_check_image)
        draw_image(comp->gfx, switch_x, switch_y, comp->gfx->_check_image);

    return 0;
}

button_component_t*
create_switch(light_component_t** link, char* label, uint32_t x, uint32_t y, uint32_t width, uint32_t height, bool* value)
{
    button_component_t* ret;

    if (!link)
        return nullptr;

    ret = create_button(link, label, x, y, width, height, switch_onclick, switch_draw);

    if (!ret)
        return nullptr;

    /* For tabs, we can simply store the target index directly in ->private */
    ret->private = (uintptr_t)value;
    ret->type = BTN_TYPE_SWITCH;

    return ret;
}
