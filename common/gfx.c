#include "ctx.h"
#include "efiprot.h"
#include "font.h"
#include "image.h"
#include "key.h"
#include "mouse.h"
#include "stddef.h"
#include "ui/box.h"
#include "ui/button.h"
#include "ui/component.h"
#include "ui/cursor.h"
#include "ui/image.h"
#include "ui/input_box.h"
#include "ui/screens/home.h"
#include "ui/screens/info.h"
#include "ui/screens/install.h"
#include "ui/screens/minesweeper.h"
#include "ui/screens/options.h"
#include <gfx.h>
#include <memory.h>
#include <stdint.h>
#include <stdio.h>

static light_gfx_t light_gfx = { 0 };

/* Immutable root */
static light_component_t* root_component;

/* Mark the current component that is mounted as the root of the screen */
static light_component_t* current_screen_root;

static light_component_t* current_selected_btn;
static light_component_t* current_selected_inputbox;

#define SCREEN_HOME_IDX 0
#define SCREEN_OPTIONS_IDX 1
#define SCREEN_INSTALL_IDX 2
#define SCREEN_INFO_IDX 3
#define SCREEN_MINESWEEPER_IDX 4

/* The different components that can be mounted as screenroot */
static light_component_t* screens[] = {
    [SCREEN_HOME_IDX] = nullptr,
    [SCREEN_OPTIONS_IDX] = nullptr,
    [SCREEN_INSTALL_IDX] = nullptr,
    [SCREEN_INFO_IDX] = nullptr,
    [SCREEN_MINESWEEPER_IDX] = nullptr,
};

static char* screen_labels[] = {
    [SCREEN_HOME_IDX] = "res/home.bmp",
    [SCREEN_OPTIONS_IDX] = "res/opt.bmp",
    [SCREEN_INSTALL_IDX] = "res/instl.bmp",
    [SCREEN_INFO_IDX] = "res/info.bmp",
    [SCREEN_MINESWEEPER_IDX] = "res/mswpr.bmp"
};

static const size_t screens_count = sizeof screens / sizeof screens[0];

light_color_t WHITE = CLR(0xFFFFFFFF);
light_color_t BLACK = CLR(0x000000FF);
light_color_t GRAY = CLR(0x1e1e1eFF);
light_color_t DARK_GRAY = CLR(0x0c0c0cFF);
light_color_t LIGHT_GRAY = CLR(0x303030FF);

light_color_t PINK = CLR(0xf404dcFF);
light_color_t DARK_PINK = CLR(0x1c081aFF);

light_color_t PURPLE = CLR(0x7f27eaFF);
light_color_t DARK_PURPLE = CLR(0x25192bFF);

light_color_t BLUE = CLR(0x0e1defFF);
light_color_t DARK_BLUE = CLR(0x1c192bFF);

light_color_t CYAN = CLR(0x0ed5efFF);
light_color_t DARK_CYAN = CLR(0x081b1cFF); /* Yay, I love dark light blue =))))))) */

light_color_t GREEN = CLR(0x27ea34FF);
light_color_t DARK_GREEN = CLR(0x0b1c08FF);

light_color_t RED = CLR(0xf40000FF);
light_color_t DARK_RED = CLR(0x1c0808FF);

void gfx_load_font(struct light_font* font)
{
    light_gfx.current_font = font;
}

static uint32_t
expand_byte_to_dword(uint8_t byte)
{
    uint32_t ret = 0;

    for (uint8_t i = 0; i < sizeof(uint32_t); i++) {
        ret |= (((uint32_t)(byte)) << (i * 8));
    }
    return ret;
}

static uint32_t
transform_light_clr(light_gfx_t* gfx, light_color_t clr)
{
    uint32_t fb_color = 0;

    fb_color |= expand_byte_to_dword(clr.red) & gfx->red_mask;
    fb_color |= expand_byte_to_dword(clr.green) & gfx->green_mask;
    fb_color |= expand_byte_to_dword(clr.blue) & gfx->blue_mask;
    fb_color |= expand_byte_to_dword(clr.alpha) & gfx->alpha_mask;

    return fb_color;
}

light_color_t
gfx_transform_pixel(light_gfx_t* gfx, uint32_t clr)
{
    light_color_t ret = { 0 };

    ret.red = clr & gfx->red_mask;
    ret.green = clr & gfx->green_mask;
    ret.blue = clr & gfx->blue_mask;
    ret.alpha = clr & gfx->alpha_mask;

    return ret;
}

inline uint32_t
gfx_get_pixel(light_gfx_t* gfx, uint32_t x, uint32_t y)
{
    if (x >= gfx->width || y >= gfx->height || !gfx->back_fb)
        return NULL;

    return *(uint32_t volatile*)(gfx->back_fb + x * gfx->bpp / 8 + y * gfx->stride * sizeof(uint32_t));
}

void gfx_draw_rect(light_gfx_t* gfx, uint32_t _x, uint32_t _y, uint32_t width, uint32_t height, light_color_t clr)
{
    uint32_t trf;

    if (!clr.alpha)
        return;

    trf = transform_light_clr(gfx, clr);

    gfx_draw_rect_raw(gfx, _x, _y, width, height, trf);
}

static inline volatile uint32_t*
_gfx_get_draw_addr(light_gfx_t* gfx, uint32_t x, uint32_t y, bool back)
{
    return (volatile uint32_t*)((uintptr_t)(back ? gfx->back_fb : gfx->phys_addr) + x * gfx->bpp / 8 + y * gfx->stride * sizeof(uint32_t));
}

void gfx_draw_rect_raw(light_gfx_t* gfx, uint32_t _x, uint32_t _y, uint32_t width, uint32_t height, uint32_t clr)
{
    uint32_t x, y;
    uint32_t draw_offset;
    uint32_t gfx_width, gfx_height;
    volatile uint32_t* draw_addr;
    bool write_to_backbuf, drawing_cursor;

    gfx_width = gfx->width;
    gfx_height = gfx->height;

    write_to_backbuf = gfx->back_fb && gfx->ctx->has_fw;
    drawing_cursor = gfx_is_drawing_cursor(gfx);

    x = _x;
    y = _y;
    draw_offset = (gfx->bpp / 8);
    draw_addr = _gfx_get_draw_addr(gfx, x, y, write_to_backbuf);

    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {

            if (x >= gfx_width)
                goto cycle;

            *draw_addr = clr;

            /* When we are not drawing the cursor, we should look for any pixel updates */
            if (!drawing_cursor)
                update_cursor_pixel(gfx, x, y, clr);

        cycle:
            draw_addr++;
            x++;
        }
        if (y++ >= gfx_height)
            break;
        x = _x;
        draw_addr = _gfx_get_draw_addr(gfx, x, y, write_to_backbuf);
    }
}

void gfx_draw_rect_img(light_gfx_t* gfx, uint32_t _x, uint32_t _y, uint32_t width, uint32_t height, uint32_t* clrs)
{
    uint32_t x, y;
    uint32_t draw_offset;
    uint32_t gfx_width, gfx_height;
    volatile uint32_t* draw_addr;
    bool write_to_backbuf, drawing_cursor;

    gfx_width = gfx->width;
    gfx_height = gfx->height;

    write_to_backbuf = gfx->back_fb && gfx->ctx->has_fw;
    drawing_cursor = gfx_is_drawing_cursor(gfx);

    x = _x;
    y = _y;
    draw_offset = (gfx->bpp / 8);
    draw_addr = _gfx_get_draw_addr(gfx, x, y, write_to_backbuf);

    for (uint32_t i = 0; i < height; i++) {
        for (uint32_t j = 0; j < width; j++) {

            if (x >= gfx_width)
                goto cycle;

            *draw_addr = *clrs;

            /* When we are not drawing the cursor, we should look for any pixel updates */
            if (!drawing_cursor)
                update_cursor_pixel(gfx, x, y, *clrs);

        cycle:
            draw_addr++;
            clrs++;
            x++;
        }
        if (y++ >= gfx_height)
            break;
        x = _x;
        draw_addr = _gfx_get_draw_addr(gfx, x, y, write_to_backbuf);
    }
}

int lclr_blend(light_color_t fg, light_color_t bg, light_color_t* out)
{
    /* TODO: actual blending */

    if (!fg.alpha) {
        *out = bg;
        return 0;
    }

    *out = fg;
    return 0;
}

void gfx_draw_char(light_gfx_t* gfx, char c, uint32_t x, uint32_t y, light_color_t clr)
{
    int error;
    light_glyph_t glyph;

    if (!gfx || !gfx->current_font)
        return;

    error = lf_get_glyph_for_char(gfx->current_font, c, &glyph);

    if (error)
        return;

    for (uint8_t _y = 0; _y < gfx->current_font->height; _y++) {
        char glyph_part = glyph.data[_y];
        for (uint8_t _x = 0; _x < gfx->current_font->width; _x++) {
            if (glyph_part & (1 << _x)) {
                gfx_draw_rect(gfx, x + _x, y + _y, 1, 1, clr);
            }
        }
    }
}

void gfx_draw_str(light_gfx_t* gfx, char* str, uint32_t x, uint32_t y, light_color_t clr)
{
    uint32_t x_idx = 0;
    char* c = str;

    /*
     * Loop over every char, draw them after one another over the x-axis, no looping
     * when the screen-edge is hit
     */
    while (*c) {
        gfx_draw_char(gfx, *c, x + x_idx, y, clr);
        x_idx += gfx->current_font->width;
        c++;
    }
}

void gfx_clear_screen(light_gfx_t* gfx)
{
    gfx_draw_rect_raw(gfx, 0, 0, gfx->width, gfx->height, 0x00);

    /* Just in case */
    gfx_switch_buffers(gfx);
}

void gfx_clear_screen_splash(light_gfx_t* gfx)
{
    light_image_t* image;

    gfx_draw_rect_raw(gfx, 0, 0, gfx->width, gfx->height, 0x00);

    image = load_bmp_image("res/splash.bmp");

    if (!image)
        goto switch_and_exit;

    draw_image(gfx, (gfx->width >> 1) - 64, (gfx->height >> 1) - 64, image);

    destroy_bmp_image(image);
switch_and_exit:
    /* Just in case */
    gfx_switch_buffers(gfx);
}

void gfx_draw_rect_outline(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t width, uint32_t height, light_color_t clr)
{
    /* Horizontal lines */
    gfx_draw_rect(gfx, x, y, width, 1, clr);
    gfx_draw_rect(gfx, x, y + height - 1, width, 1, clr);

    /* Vertical lines */
    gfx_draw_rect(gfx, x, y, 1, height, clr);
    gfx_draw_rect(gfx, x + width - 1, y, 1, height, clr);
}

void gfx_draw_circle(light_gfx_t* gfx, uint32_t x, uint32_t y, uint32_t radius, light_color_t clr)
{
    for (uint32_t i = 0; i <= radius; i++) {
        for (uint32_t j = 0; j <= radius; j++) {
            if (i * i + j * j > radius * radius)
                continue;

            gfx_draw_rect(gfx, x + j, y + i, 1, 1, clr);
        }
    }
}

int gfx_get_pixels(light_gfx_t* gfx, void* buff, uint32_t width, uint32_t height)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = gfx->priv;

    if (!gfx->back_fb || !gfx->ctx->has_fw)
        return -1;

    gop->Blt(gop, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)buff, EfiBltVideoToBltBuffer, NULL, NULL, NULL, NULL, width, height, NULL);
    return 0;
}

/*!
 * @brief: Switch back and front buffer
 *
 * TODO: this is UEFI specific code, move this to the platform-dependant section for UEFI and
 * create an API throught the context thingy
 */
int gfx_switch_buffers(light_gfx_t* gfx)
{
    EFI_GRAPHICS_OUTPUT_PROTOCOL* gop = gfx->priv;

    if (!gfx->back_fb || !gfx->ctx->has_fw)
        return -1;

    gop->Blt(gop, (EFI_GRAPHICS_OUTPUT_BLT_PIXEL*)gfx->back_fb, EfiBltBufferToVideo, NULL, NULL, NULL, NULL, gfx->width, gfx->height, NULL);
    /*
    for (uint32_t i = 0; i < gfx->height; i++) {
      for (uint32_t j = 0; j < gfx->width; j++) {
        *(uint32_t volatile*)(gfx->phys_addr + j * gfx->bpp / 8 + i * gfx->stride * sizeof(uint32_t)) = gfx_get_pixel(gfx, j, i);
      }
    }
    */
    return 0;
}

/*
 * NOTE: temporary
 */
static uint32_t printf_x = 4;
static uint32_t printf_y = 4;

int gfx_putchar(char c)
{
    gfx_draw_char(&light_gfx, c, printf_x, printf_y, WHITE);

    printf_x += light_gfx.current_font->width;
    return 0;
}

int gfx_printf(char* str, ...)
{
    /*
     * TODO: get a bootloader tty context
     * from this we get:
     *  - the current starting x and y coords where we can print the next string of info
     *  - the forground color and background color
     */
    gfx_draw_str(&light_gfx, str, printf_x, printf_y, WHITE);

    gfx_switch_buffers(&light_gfx);

    printf_y += light_gfx.current_font->height;
    printf_x = 4;
    return 0;
}

/*
 * GFX screen management
 */

static light_component_t*
gfx_get_screen(uint32_t idx)
{
    if (idx > screens_count)
        return nullptr;

    return screens[idx];
}

static light_component_t*
gfx_get_current_screen()
{
    if (!current_screen_root)
        return nullptr;

    return current_screen_root->next;
}

static uint32_t
gfx_get_current_screen_type()
{
    light_component_t* current_screen = gfx_get_current_screen();

    if (!current_screen)
        return -1;

    for (uint32_t i = 0; i < screens_count; i++) {
        if (screens[i] == current_screen)
            return i;
    }

    return -1;
}

static int
gfx_do_screenswitch(light_gfx_t* gfx, light_component_t* new_screen)
{
    current_screen_root->next = new_screen;

    gfx_reset_btn_select();
    gfx_select_inputbox(gfx, nullptr);

    gfx->flags |= GFX_FLAG_SHOULD_CHANGE_SCREEN;

    return 0;
}

static int
gfx_do_screen_update()
{
    light_component_t* root = root_component;

    /* Make sure every component gets rendered on the screenswitch */
    for (; root; root = root->next) {
        root->should_update = true;
    }

    return 0;
}

static int
tab_btn_onclick(button_component_t* c)
{
    light_component_t* target;
    tab_btn_private_t* priv = (tab_btn_private_t*)c->private;
    int target_index = priv->index;

    /* Invalid index =\ */
    if (target_index < 0)
        return -1;

    /* Grab the target */
    target = gfx_get_screen(target_index);

    /* NOTE: we don't care if target is null, since that will be okay */
    gfx_do_screenswitch(c->parent->gfx, target);
    return 0;
}

static int
pwr_btn_onclick(button_component_t* c)
{
    /* Yeet, if there is a nullpointer thingy here somewhere, the system will go down anyway lmao */
    c->parent->gfx->ctx->f_shutdown();
    return 0;
}

static light_component_t*
__next_btn(light_component_t* start)
{
    light_component_t* itt = start;

    while (itt) {

        if (itt->type == COMPONENT_TYPE_BUTTON)
            break;

        itt = itt->next;
    }

    return itt;
}

static void
get_next_btn(light_component_t** out)
{
    light_component_t* itt;

    if (*out) {
        itt = __next_btn((*out)->next);

        /* If we could not find it, loop back around */
        if (!itt)
            itt = __next_btn(root_component);
    } else
        itt = __next_btn(root_component);

    *out = itt;
}

/*!
 * @brief Set the component @component as the current selected inputbox
 *
 * Nothing to add here...
 */
int gfx_select_inputbox(light_gfx_t* gfx, struct light_component* component)
{
    inputbox_component_t* inputbox;

    /* If the current inputbox is set, it is ensured to be of the type inputbox =) */
    if (current_selected_inputbox && current_selected_inputbox != component) {
        inputbox = current_selected_inputbox->private;

        /* Update the deselect graphics */
        current_selected_inputbox->should_update = true;

        /* Clear the focussed flag */
        inputbox->focussed = false;
    }

    if (!component) {
        current_selected_inputbox = nullptr;
        return 0;
    }

    if (component->type != COMPONENT_TYPE_INPUTBOX)
        return -1;

    inputbox = component->private;

    if (!inputbox)
        return -2;

    inputbox->focussed = true;
    current_selected_inputbox = component;

    return 0;
}

static void gfx_handle_keypress(char pressed_char)
{

    switch (pressed_char) {
    case '\t':
        get_next_btn(&current_selected_btn);

        if (current_selected_btn) {
            set_previous_mousepos((light_mousepos_t) {
                .x = current_selected_btn->x + (current_selected_btn->width >> 1) - 1,
                .y = current_selected_btn->y + (current_selected_btn->height >> 1) - 1,
                .btn_flags = 0 });
        }
        break;
    case '\r': {
        button_component_t* current_selected_btn_cmp = NULL;

        if (current_selected_btn && current_selected_btn->type == COMPONENT_TYPE_BUTTON && current_selected_btn->private) {
            current_selected_btn_cmp = current_selected_btn->private;

            button_click(current_selected_btn_cmp);
        }
        break;
    }
    }
}

static inline bool
gfx_should_switch_screen(light_gfx_t* gfx)
{
    return ((gfx->flags & GFX_FLAG_SHOULD_CHANGE_SCREEN) == GFX_FLAG_SHOULD_CHANGE_SCREEN);
}

static inline bool
gfx_should_exit_frontend(light_gfx_t* gfx)
{
    return ((gfx->flags & GFX_FLAG_SHOULD_EXIT_FRONTEND) == GFX_FLAG_SHOULD_EXIT_FRONTEND);
}

void gfx_reset_btn_select()
{
    current_selected_btn = nullptr;
}

/*!
 * @brief: 'stdout' redirection function
 *
 * When we're in GFX context, we're able to have our own logging. We do this in the form of a simple dialog overlay
 * box, which acts independant from the actual GFX context. When a printf is called, it's content will be directly
 * put onto the screen, without it needing to wait for a GFX redraw (Although a redraw will cause the dialog box to
 * be erased).
 */
static int
__gfx_printf(char* fmt, ...)
{
    return 0;
}

/*!
 * @brief Main bootloader frontend loop
 *
 * Nothing to add here...
 */
gfx_frontend_result_t
gfx_enter_frontend()
{
    light_ctx_t* ctx = light_gfx.ctx;
    light_key_t key_buffer = { 0 };
    light_mousepos_t mouse_buffer = { 0 };
    gfx_frontend_result_t result;
    uint32_t prev_mousepx;

    current_selected_btn = nullptr;
    current_selected_inputbox = nullptr;

    // ctx->f_printf = __gfx_printf;

    gfx_clear_screen(&light_gfx);

    if (!ctx->f_has_keyboard()) {
        printf("Could not locate a keyboard! Plug in a keyboard and restart the PC.");
        return REBOOT;
    }

    /*
     * FIXME: the loader should still be able to function, even without a mouse
     */
    if (!ctx->f_has_mouse()) {
        printf("Could not locate a mouse!");
        return REBOOT;
    }

    init_keyboard();

    init_mouse();

    /* Make sure we have the images cached for our switches */
    light_gfx._check_image = load_bmp_image("res/check.bmp");
    light_gfx._checkbox_image = load_bmp_image("res/chkbox.bmp");

    /* Initialize the cursor */
    init_cursor(&light_gfx);

    /*
     * Build the UI stack
     * We first build how the root layout is going to look. Think about the
     * navigation bar, navigation buttons, any headers or footers, ect.
     *
     * after that we construct our screens, seperate from the root and then we
     * connect the screen up to the bottom of the root link if we want to show that
     * screen
     *
     * TODO: widgets -> interacable islands that are again their seperate things. we
     * can use these for popups and things, but these need to be initialized seperately
     * aswell. Widgets are always relative to a certain component, and they are linked to
     * the ->active_widget field in the component and from that they each link through the
     * ->next field that every component has
     */

    /* Main background color */
    create_box(&root_component, nullptr, 0, 0, light_gfx.width, light_gfx.height, 0, true, DARK_GRAY);

    create_image(&root_component, nullptr, 0, 0, light_gfx.width, light_gfx.height, IMAGE_TYPE_BMP, "res/bckgrnd.bmp");

    /* Navigation bar */
    create_box(&root_component, nullptr, 0, 0, light_gfx.width, 42, 0, true, GRAY);

    /* Navigation buttons */
    for (uint32_t i = 0; i < screens_count; i++) {
        const uint32_t btn_width = 36;
        const uint32_t initial_offset = 4;

        /* Add btn_width for every screen AND add the initial offset times the amount of buttons before us */
        uint32_t x_offset = i * btn_width + initial_offset * (i + 1);

        create_tab_button(&root_component, screen_labels[i], x_offset, 3, btn_width, 36, tab_btn_onclick, i);
    }

    create_tab_button(&root_component, "res/pwrbtn.bmp", light_gfx.width - 39, 3, 36, 36, pwr_btn_onclick, NULL);

    /* Create a box for the home screen */
    construct_homescreen(&screens[SCREEN_HOME_IDX], &light_gfx);

    /* Create a box for the options screen */
    construct_optionsscreen(&screens[SCREEN_OPTIONS_IDX], &light_gfx);

    /* Create a box for the install screen */
    construct_installscreen(&screens[SCREEN_INSTALL_IDX], &light_gfx);

    /* Create a box for the info screen */
    construct_infoscreen(&screens[SCREEN_INFO_IDX], &light_gfx);

    /* Create a screen for minesweeper */
    construct_minesweeper_screen(&screens[SCREEN_MINESWEEPER_IDX], &light_gfx);

    /*
     * Create the screen link, which never gets rendered, but simply acts as a connector for the screens to
     * plug into if they want to get rendered
     * NOTE: this should be the last component to be directly linked into the actual root
     */
    current_screen_root = create_component(&root_component, COMPONENT_TYPE_LINK, nullptr, NULL, NULL, NULL, NULL, NULL);

    /* Make sure we start at the homescreen */
    gfx_do_screenswitch(&light_gfx, screens[SCREEN_HOME_IDX]);

    /*
     * Loop until something wants us to exit
     */
    while (!gfx_should_exit_frontend(&light_gfx)) {

        ctx->f_get_keypress(&key_buffer);
        ctx->f_get_mousepos(&mouse_buffer);

        /* Tab (temp) */
        if (key_buffer.typed_char && !current_selected_inputbox) {
            gfx_handle_keypress(key_buffer.typed_char);

            /* Catch gfx exit stuff */
            if (gfx_should_exit_frontend(&light_gfx))
                break;

            if (gfx_should_switch_screen(&light_gfx)) {
                gfx_do_screen_update();
                goto frontend_loop_cycle;
            }
        }

        FOREACH_UI_COMPONENT(i, root_component)
        {

            /* Skip lmao */
            if (!i->f_should_update || !i->f_ondraw)
                continue;

            i->key_buffer = &key_buffer;
            i->mouse_buffer = &mouse_buffer;

            update_component(i, key_buffer, mouse_buffer);

            /* Update prompted a screen chance. Do it */
            if (gfx_should_switch_screen(&light_gfx)) {
                gfx_do_screen_update();
                break;
            }

            draw_component(i, key_buffer, mouse_buffer);
        }

    frontend_loop_cycle:

        /* keep this clear after a draw pass */
        light_gfx.flags &= ~GFX_FLAG_SHOULD_CHANGE_SCREEN;

        draw_cursor(&light_gfx, mouse_buffer.x, mouse_buffer.y);

        gfx_switch_buffers(&light_gfx);
    }

    /* Make sure to not update the cursor stuff when we are not in the frontend */
    light_gfx.flags &= ~GFX_FLAG_SHOULD_DRAW_CURSOR;

    /* Reset the input devices */
    reset_keyboard();
    reset_mouse();

    /* Default option, just boot our kernel */
    return BOOT_MULTIBOOT;
}

void get_light_gfx(light_gfx_t** gfx)
{
    if (!gfx)
        return;

    *gfx = &light_gfx;
}

void init_light_gfx()
{
    /* Make sure the structure is clean */
    memset(&light_gfx, 0, sizeof(light_gfx));

    light_gfx.current_font = &default_light_font;
    light_gfx.ctx = get_light_ctx();

    root_component = nullptr;
}
