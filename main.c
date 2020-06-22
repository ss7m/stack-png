/* 
 * vim: tabstop=8 shiftwidth=8
 * trying out kernel coding style
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <png.h>

#define STACK_PNG_VERSION "1.6.37"
#define PNG_SIG_LENGTH 8

#define stack_image_row_size(image) (sizeof(uint8_t) * 3 * (image)->width)
#define max(A, B) ((A) > (B) ? (A) : (B))
#define DEBUG_BOOL(b) printf("%s\n", (b) ? "true" : "false")

struct stack_png {
        png_struct *png;
        png_info *info;
        FILE *file;
};

/*
 * 8-bit RGB image data
 */
struct stack_image {
        size_t width;
        size_t height;
        uint8_t *data;
};

struct stack_pixel {
        uint8_t r;
        uint8_t g;
        uint8_t b;
};

/*
 * Reads the first 8 bytes and checks them against the magic png header
 * Should probably be followed with a call to "png_set_sig_bytes"
 */
int stack_check_if_png(FILE *file) {
        uint8_t bytes[PNG_SIG_LENGTH];
        fread(bytes, sizeof(uint8_t), PNG_SIG_LENGTH, file);
        return png_sig_cmp(bytes, 0, PNG_SIG_LENGTH) == 0;
}

/*
 * struct stack_png reading functions
 */

void stack_png_reader_init(struct stack_png *reader, char *file_name) {
        png_struct *png;
        png_info *info;
        FILE *file;

        png = png_create_read_struct(STACK_PNG_VERSION, NULL, NULL, NULL);
        if (png == NULL) {
                fprintf(stderr, "Unable to create read struct\n");
                exit(EXIT_FAILURE);
        }

        info = png_create_info_struct(png);
        if (info == NULL) {
                fprintf(stderr, "Unable to create info struct\n");
                exit(EXIT_FAILURE);
        }

        file = fopen(file_name, "rb");
        if (file == NULL) {
                fprintf(stderr, "Unable to open file %s for reading\n", file_name);
                exit(EXIT_FAILURE);
        } else if (!stack_check_if_png(file)) {
                fprintf(stderr, "File %s is not a png file\n", file_name);
                exit(EXIT_FAILURE);
        }

        png_init_io(png, file);
        png_set_sig_bytes(png, PNG_SIG_LENGTH);
        png_read_png(
                png, info,
                PNG_TRANSFORM_STRIP_16 |
                PNG_TRANSFORM_PACKING |
                PNG_TRANSFORM_GRAY_TO_RGB |
                PNG_TRANSFORM_STRIP_ALPHA,
                NULL);

        reader->png = png;
        reader->info = info;
        reader->file = file;
}

void stack_png_reader_destroy(struct stack_png *reader) {
        png_destroy_read_struct(&reader->png, &reader->info, NULL);
        fclose(reader->file);
}

void stack_png_reader_load_image(struct stack_png *reader, struct stack_image *image) {
        size_t i, row_size;
        uint8_t **image_rows;

        image->width = png_get_image_width(reader->png, reader->info);
        image->height = png_get_image_height(reader->png, reader->info);
        row_size = stack_image_row_size(image);
        image->data = malloc(row_size * image->height);

        image_rows = png_get_rows(reader->png, reader->info);
        for(i = 0; i < image->height; i++) {
                memcpy(
                        image->data + i * row_size,
                        image_rows[i],
                        row_size);
        }
}

/*
 * struct stack_png writing functions
 */

void stack_png_writer_init(struct stack_png *writer, char *file_name) {
        png_struct *png;
        png_info *info;
        FILE *file;

        png = png_create_write_struct(STACK_PNG_VERSION, NULL, NULL, NULL);
        if (png == NULL) {
                fprintf(stderr, "Unable to create write struct\n");
                exit(EXIT_FAILURE);
        }

        info = png_create_info_struct(png);
        if (info == NULL) {
                fprintf(stderr, "Unable to create info struct\n");
                exit(EXIT_FAILURE);
        }

        file = fopen(file_name, "wb");
        if (file == NULL) {
                fprintf(stderr, "Unable to open file %s for writing\n", file_name);
                exit(EXIT_FAILURE);
        }

        png_init_io(png, file);
        writer->png = png;
        writer->info = info;
        writer->file = file;
}

void stack_png_writer_destroy(struct stack_png *writer) {
        png_destroy_write_struct(&writer->png, &writer->info);
        fclose(writer->file);
}

void stack_png_writer_save_image(struct stack_png *writer, struct stack_image *image) {
        uint8_t **data;
        size_t row_size, i;

        row_size = stack_image_row_size(image);
        data = malloc(sizeof(uint8_t *) * image->height);
        for (i = 0; i < image->height; i++)
                data[i] = image->data + i * row_size;

        png_set_IHDR(
                writer->png,
                writer->info,
                image->width,
                image->height,
                8, /* bit depth of 8 */
                PNG_COLOR_TYPE_RGB,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_DEFAULT,
                PNG_FILTER_TYPE_DEFAULT);
        png_set_rows(writer->png, writer->info, data);
        png_write_png(
                writer->png,
                writer->info,
                PNG_TRANSFORM_IDENTITY,
                NULL);
        free(data);
}

/*
 * struct stack_image functions
 */

void stack_image_destroy(struct stack_image *image) {
        free(image->data);
}

struct stack_pixel stack_image_get_pixel(struct stack_image *image, size_t x, size_t y) {
        struct stack_pixel pixel;
        size_t row_size;

        row_size = stack_image_row_size(image);
        pixel.r = image->data[row_size * y + 3 * x];
        pixel.g = image->data[row_size * y + 3 * x + 1];
        pixel.b = image->data[row_size * y + 3 * x + 2];

        return pixel;
}

void stack_image_set_pixel(struct stack_image *image,
                struct stack_pixel pixel,
                size_t x, size_t y) {
        size_t row_size = stack_image_row_size(image);
        image->data[row_size * y + 3 * x]     = pixel.r;
        image->data[row_size * y + 3 * x + 1] = pixel.g;
        image->data[row_size * y + 3 * x + 2] = pixel.b;
}

void stack_image_init_blank(struct stack_image *image, size_t width, size_t height) {
        size_t row_size;

        image->width = width;
        image->height = height;
        row_size = stack_image_row_size(image);
        image->data = calloc(row_size * height, sizeof(uint8_t));
}

/*
 * Does no safety checks
 */
void stack_image_paste(struct stack_image *dest, struct stack_image *src,
                size_t x, size_t y) {
        size_t i, dest_row_size, src_row_size;
        uint8_t *dest_location;

        dest_row_size = stack_image_row_size(dest);
        src_row_size = stack_image_row_size(src);
        dest_location = dest->data + y * dest_row_size + 3 * x;

        for(i = 0; i < src->height; i++) {
                memcpy(
                        dest_location + i * dest_row_size,
                        src->data + i * src_row_size,
                        src_row_size);
        }
}

int main(int argc, char **argv) {
        size_t width = 0, height = 0, y;
        int i;
        struct stack_image result, *images;
        struct stack_png writer;

        if (argc == 1) {
                fprintf(stderr, "Send at least 1 argument\n");
                return EXIT_FAILURE;
        }

        images = malloc(sizeof(struct stack_image) * (argc - 1));
        for (i = 0; i < argc - 1; i++) {
                struct stack_png reader;
                stack_png_reader_init(&reader, argv[i+1]);
                stack_png_reader_load_image(&reader, &images[i]);
                width = max(width, images[i].width);
                height += images[i].height;
                stack_png_reader_destroy(&reader);
        }

        stack_image_init_blank(&result, width, height);

        y = 0;
        for (i = 0; i < argc - 1; i++) {
                size_t x = (width - images[i].width) / 2;
                stack_image_paste(&result, &images[i], x, y);
                y += images[i].height;
        }

        stack_png_writer_init(&writer, "out.png");
        stack_png_writer_save_image(&writer, &result);

        stack_png_writer_destroy(&writer);
        stack_image_destroy(&result);
        for (i = 0; i < argc - 1; i++)
                stack_image_destroy(&images[i]);
        free(images);
        return EXIT_SUCCESS;
}
