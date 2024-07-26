#include "file.h"
#include "gfx.h"
#include "heap.h"
#include <image.h>
#include <memory.h>
#include <stdio.h>

light_image_t*
load_bmp_image(char* path)
{
    struct bmp_header* fbuffer;
    size_t image_s_size;
    light_image_t* image;
    light_file_t* file = fopen(path);

    if (!file || !file->filesize)
        return nullptr;

    fbuffer = heap_allocate(file->filesize);

    file->f_readall(file, fbuffer);

    if (fbuffer->magic[0] != 'B' && fbuffer->magic[1] != 'M')
        goto dealloc_and_fail;

    if (fbuffer->width < 0 || fbuffer->height < 0)
        goto dealloc_and_fail;

    image_s_size = sizeof(light_image_t) + (fbuffer->bpp >> 3) * fbuffer->width * fbuffer->height;
    image = heap_allocate(image_s_size);

    if (!image)
        goto dealloc_and_fail;

    memset(image, 0, image_s_size);

    image->height = fbuffer->height;
    image->width = fbuffer->width;
    image->bytes_per_pixel = (fbuffer->bpp >> 3);

    /*
    if (image->bytes_per_pixel != 4) {
      printf("Could not load bmp, bpp != 32");
      goto dealloc_and_fail;
    }
    */

    uint32_t y_offset = 0;
    uint64_t load_idx = 0;

    for (uint32_t i = 0; i < image->height; i++) {
        for (uint32_t j = 0; j < image->width; j++) {

            /* BMPs are loaded from the bottom fsr xD */
            y_offset = (image->height - 1) - i;

            uint8_t* blue = (uint8_t*)((uint8_t*)fbuffer + fbuffer->image_start) + load_idx + 0;
            uint8_t* green = (uint8_t*)((uint8_t*)fbuffer + fbuffer->image_start) + load_idx + 1;
            uint8_t* red = (uint8_t*)((uint8_t*)fbuffer + fbuffer->image_start) + load_idx + 2;

            image->pixel_data[y_offset * image->bytes_per_pixel * image->width + j * image->bytes_per_pixel + 0] = *red;
            image->pixel_data[y_offset * image->bytes_per_pixel * image->width + j * image->bytes_per_pixel + 1] = *green;
            image->pixel_data[y_offset * image->bytes_per_pixel * image->width + j * image->bytes_per_pixel + 2] = *blue;

            if (image->bytes_per_pixel == 4) {
                uint8_t* alpha = (uint8_t*)((uint8_t*)fbuffer + fbuffer->image_start) + load_idx + 3;

                image->pixel_data[y_offset * image->bytes_per_pixel * image->width + j * image->bytes_per_pixel + 3] = *alpha;
            }

            load_idx += image->bytes_per_pixel;
        }
    }

    file->f_close(file);
    heap_free(fbuffer);
    return image;

dealloc_and_fail:
    printf("Could not load bmp");
    heap_free(fbuffer);
    file->f_close(file);
    return nullptr;
}

void destroy_bmp_image(light_image_t* image)
{
    heap_free(image);
}

#define IMG_CLR_BYTE(image, x, y, rgba_idx) (((rgba_idx) > 3) ? 0 : ((y) * (image)->bytes_per_pixel * (image)->width + (x) * (image)->bytes_per_pixel + (rgba_idx)))

void draw_image(light_gfx_t* gfx, uint32_t x, uint32_t y, light_image_t* l_image)
{
    for (uint32_t i = 0; i < l_image->height; i++) {
        for (uint32_t j = 0; j < l_image->width; j++) {

            uint8_t r = l_image->pixel_data[IMG_CLR_BYTE(l_image, j, i, 0)];
            uint8_t g = l_image->pixel_data[IMG_CLR_BYTE(l_image, j, i, 1)];
            uint8_t b = l_image->pixel_data[IMG_CLR_BYTE(l_image, j, i, 2)];

            uint8_t a;

            if (l_image->bytes_per_pixel != 4)
                a = 0xFF;
            else
                a = l_image->pixel_data[IMG_CLR_BYTE(l_image, j, i, 3)];

            gfx_draw_rect(gfx, x + j, y + i, 1, 1, (light_color_t) {
                                                       .alpha = a,
                                                       .red = r,
                                                       .green = g,
                                                       .blue = b,
                                                   });
        }
    }
}

/*!
 * @brief Create a new image with scaled dimentions
 */
light_image_t*
scale_image(light_gfx_t* gfx, light_image_t* image, uint32_t new_width, uint32_t new_height)
{
    size_t total_image_size;
    size_t old_width, old_height;
    light_image_t* ret;

    if (!image)
        return NULL;

    total_image_size = sizeof(light_image_t) + new_width * new_height * image->bytes_per_pixel;
    ret = heap_allocate(total_image_size);

    if (!ret)
        return NULL;

    memset(ret, 0, total_image_size);

    old_width = image->width;
    old_height = image->height;

    ret->width = new_width;
    ret->height = new_height;
    ret->bytes_per_pixel = image->bytes_per_pixel;

    /*
     * TODO: Test and fix =)))))
     */
    for (size_t y = 0; y < new_height; y++) {

        /*
         * We want to calculate the x and y positions on the old image for each new
         * x and y where we want to yoink color from
         */
        size_t scaled_y = (y * old_height) / new_height;
        size_t scaled_x = 0;

        for (size_t x = 0; x < new_width; x++) {

            ret->pixel_data[IMG_CLR_BYTE(ret, x, y, 0)] = image->pixel_data[IMG_CLR_BYTE(image, scaled_x / 64, scaled_y, 0)];
            ret->pixel_data[IMG_CLR_BYTE(ret, x, y, 1)] = image->pixel_data[IMG_CLR_BYTE(image, scaled_x / 64, scaled_y, 1)];
            ret->pixel_data[IMG_CLR_BYTE(ret, x, y, 2)] = image->pixel_data[IMG_CLR_BYTE(image, scaled_x / 64, scaled_y, 2)];
            if (ret->bytes_per_pixel == 4)
                ret->pixel_data[IMG_CLR_BYTE(ret, x, y, 3)] = image->pixel_data[IMG_CLR_BYTE(image, scaled_x / 64, scaled_y, 3)];

            scaled_x = (x * 64 * old_width) / new_width;
        }
    }

    return ret;
}
