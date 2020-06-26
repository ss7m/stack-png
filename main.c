/* 
 * vim: tabstop=8 shiftwidth=8
 * trying out kernel coding style
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <time.h>

#include <png.h>

extern int optind;
extern char *optarg;

#define STACK_PNG_VERSION "1.6.37"
#define PNG_SIG_LENGTH 8

#define stack_image_row_size(image) (sizeof(uint8_t) * 3 * (image)->width)
#define stack_png_reader_row_size(reader) (sizeof(uint8_t) * 3 * (reader)->width)
#define max(A, B) ((A) > (B) ? (A) : (B))
#define DEBUG_BOOL(b) printf("%s\n", (b) ? "true" : "false")
#define TIME(stmt) do { \
        clock_t t = clock(); \
        stmt \
        t = clock() - t; \
        printf("%f\n", t / ((double) CLOCKS_PER_SEC)); \
        } while(0);

#define true 1
#define false 0

struct stack_png_reader {
        png_struct *png;
        png_info *info;
        size_t width;
        size_t height;
};

struct stack_png_writer {
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

void stack_png_reader_init(struct stack_png_reader *reader, char *file_name) {
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
        reader->width = png_get_image_width(png, info);
        reader->height = png_get_image_height(png, info);
        fclose(file);
}

void stack_png_reader_destroy(struct stack_png_reader *reader) {
        png_destroy_read_struct(&reader->png, &reader->info, NULL);
}

void stack_png_reader_load_image(struct stack_png_reader *reader, struct stack_image *image) {
        size_t i, row_size;
        uint8_t **image_rows;

        image->width = reader->width;
        image->height = reader->height;
        row_size = stack_image_row_size(image);
        image->data = malloc(row_size * image->height);

        image_rows = png_get_rows(reader->png, reader->info);
        for(i = 0; i < image->height; i++)
                memcpy(image->data + i * row_size, image_rows[i], row_size);
}

/*
 * struct stack_png writing functions
 */

void stack_png_writer_init(struct stack_png_writer *writer, char *file_name) {
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
        png_set_compression_level(png, 2);
        writer->png = png;
        writer->info = info;
        writer->file = file;
}

void stack_png_writer_destroy(struct stack_png_writer *writer) {
        png_destroy_write_struct(&writer->png, &writer->info);
        fclose(writer->file);
}

void stack_png_writer_save_image(struct stack_png_writer *writer, struct stack_image *image) {
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
void stack_image_paste_png_reader(struct stack_image *dest,
                struct stack_png_reader *src,
                size_t x, size_t y) {
        size_t i, dest_row_size, src_row_size;
        uint8_t **src_rows, *dest_location;

        dest_row_size = stack_image_row_size(dest);
        src_row_size = stack_png_reader_row_size(src);

        src_rows = png_get_rows(src->png, src->info);

        dest_location = dest->data + y * dest_row_size + 3 * x;

        for(i = 0; i < src->height; i++) {
                memcpy(dest_location + i * dest_row_size,
                        src_rows[i],
                        src_row_size);
        }
}

int stack_parse_int(char *s) {
        int n = 0, c;
        while (('0' ^ (c = *(s++))) < 10)
                n = n * 10 + c - '0';
        return n;
}

int main(int argc, char **argv) {
        size_t width, height, y;
        int i, option, num_images, gap = 0, long_index;
        struct stack_image result;
        struct stack_png_writer writer;
        struct stack_png_reader *readers;
        char *out_file = "out.png";
        struct option long_options[] = {
                { "help",      false, NULL, 'h' },
                { "output",    true,  NULL, 'o' },
                { "gap-width", true,  NULL, 'g' },
                { 0,           0,     0,     0  }
        };

        /* Read cmd line options */
        while ((option = getopt_long(argc, argv, "ho:g:", long_options, &long_index)) != -1) {
                if (option == 0)
                        option = long_options[long_index].val;

                switch (option) {
                case 'o':
                        out_file = optarg;
                        break;
                case 'g':
                        gap = stack_parse_int(optarg);
                        break;
                case 'h':
                        printf("Usage: stack-png [OPTION]... [FILE]...\n");
                        printf("    Vertically stack a list of png files\n");
                        printf("\n");
                        printf("OPTIONS\n");
                        printf("-h --help        Print this message and exit\n");
                        printf("-o --output=FILE Write the generated png to FILE\n");
                        printf("                 Default: out.png\n");
                        printf("-g --gap-width=n Place an n pixel gap between each file\n");
                        printf("                 Default: 0\n");
                        return EXIT_SUCCESS;
                default:
                        return EXIT_FAILURE;
                }
        }

        /* load up images; calculate size of output */
        num_images = argc - optind;

        if (num_images <= 0) {
                fprintf(stderr, "Please give at least 1 file\n");
                return EXIT_FAILURE;
        }

        readers = malloc(sizeof(struct stack_png_reader) * num_images);
        width = 0;
        height = gap * (num_images - 1);
        for (i = 0; i < num_images; i++) {
                stack_png_reader_init(&readers[i], argv[i+optind]);
                width = max(width, readers[i].width);
                height += readers[i].height;
        }

        /* paste all the images together */
        y = 0;
        stack_image_init_blank(&result, width, height);
        for (i = 0; i < num_images; i++) {
                size_t x = (width - readers[i].width) / 2;
                stack_image_paste_png_reader(&result, &readers[i], x, y);
                stack_png_reader_destroy(&readers[i]);
                y += readers[i].height + gap;
        }

        stack_png_writer_init(&writer, out_file);
        stack_png_writer_save_image(&writer, &result);

        stack_png_writer_destroy(&writer);
        stack_image_destroy(&result);
        free(readers);
        return EXIT_SUCCESS;
}
