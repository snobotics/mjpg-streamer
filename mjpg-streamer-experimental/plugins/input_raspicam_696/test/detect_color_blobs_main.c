#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <jpeglib.h>
#include "detect_color_blobs.h"

static unsigned char* jpeg_read(const char* filename,
                                unsigned int* width_ptr,
                                unsigned int* height_ptr) {
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL) return NULL;

    struct jpeg_error_mgr jerr;
    struct jpeg_decompress_struct cinfo;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    (void)jpeg_read_header(&cinfo, true);
    (void)jpeg_start_decompress(&cinfo);
    int width = cinfo.output_width;
    int height = cinfo.output_height;

    unsigned char *buf = (unsigned char*)malloc(width * height * 3);
    unsigned char *p = buf;
    int row_stride = width * cinfo.output_components;
    JSAMPARRAY jpeg_buf = (*cinfo.mem->alloc_sarray)((j_common_ptr)&cinfo,
                                                     JPOOL_IMAGE, row_stride,
                                                     1);
    while (cinfo.output_scanline < cinfo.output_height) {
        (void) jpeg_read_scanlines(&cinfo, jpeg_buf, 1);
        int x;
        for (x = 0; x < width; x++) {
            unsigned char g;
            unsigned char b;
            unsigned char r = jpeg_buf[0][cinfo.output_components * x];
            if (cinfo.output_components > 2) {
                g = jpeg_buf[0][cinfo.output_components * x + 1];
                b = jpeg_buf[0][cinfo.output_components * x + 2];
            } else {
                g = r;
                b = r;
            }
            *(p++) = r;
            *(p++) = g;
            *(p++) = b;
        }
    }
    fclose(fp);
    (void) jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    *width_ptr = width;
    *height_ptr = height;
    return buf;
}

static inline unsigned char y_from_rgb(unsigned short r,
                                       unsigned short g,
                                       unsigned short b) {
   return (( 66 * r + 129 * g +  25 * b + 128) >> 8) +  16;
}

static inline unsigned char u_from_rgb(unsigned short r,
                                       unsigned short g,
                                       unsigned short b) {
   return ((-38 * r -  74 * g + 112 * b + 128) >> 8) + 128;
}

static inline unsigned char v_from_rgb(unsigned short r,
                                       unsigned short g,
                                       unsigned short b) {
   return ((112 * r -  94 * g -  18 * b + 128) >> 8) + 128;
}

static int convert_rgb_to_yuv440(unsigned int width,
                                 unsigned int height,
                                 const unsigned char rgb[],
                                 size_t yuv_bytes,
                                 unsigned char yuv[]) {
    unsigned int ii;
    unsigned int jj;
    unsigned char* y = yuv;
    unsigned int pixels = width * height;
    unsigned int required_yuv_bytes = pixels + pixels / 2;
    if (yuv_bytes < required_yuv_bytes) return -1;
    unsigned char* u = &yuv[pixels];
    unsigned char* v = &yuv[pixels + pixels / 4];
    for (ii = 0; ii < height / 4; ++ii) {
        for (jj = 0; jj < width / 4; ++jj) {
            unsigned int yno_00 = ii * (2 * width) + 2 * jj;
            unsigned int yno_01 = yno_00 + 1;
            unsigned int yno_10 = yno_00 + width;
            unsigned int yno_11 = yno_10 + 1;
            unsigned short r00 = rgb[yno_00 * 3 + 0];
            unsigned short g00 = rgb[yno_00 * 3 + 1];
            unsigned short b00 = rgb[yno_00 * 3 + 2];
            unsigned short r01 = rgb[yno_01 * 3 + 0];
            unsigned short g01 = rgb[yno_01 * 3 + 1];
            unsigned short b01 = rgb[yno_01 * 3 + 2];
            unsigned short r10 = rgb[yno_10 * 3 + 0];
            unsigned short g10 = rgb[yno_10 * 3 + 1];
            unsigned short b10 = rgb[yno_10 * 3 + 2];
            unsigned short r11 = rgb[yno_11 * 3 + 0];
            unsigned short g11 = rgb[yno_11 * 3 + 1];
            unsigned short b11 = rgb[yno_11 * 3 + 2];

            y[yno_00] = y_from_rgb(r00, g00, b00);
            y[yno_01] = y_from_rgb(r01, g01, b01);
            y[yno_10] = y_from_rgb(r10, g10, b10);
            y[yno_11] = y_from_rgb(r11, g11, b11);

            unsigned int uvno = ii * width + jj;
            unsigned short u_val = u_from_rgb(r00, g00, b00);
            u_val += u_from_rgb(r01, g01, b01);
            u_val += u_from_rgb(r10, g10, b10);
            u_val += u_from_rgb(r11, g11, b11);
            u[uvno] = (unsigned char)((u_val + 2) / 4);

            unsigned short v_val = v_from_rgb(r00, g00, b00);
            v_val += v_from_rgb(r01, g01, b01);
            v_val += v_from_rgb(r10, g10, b10);
            v_val += v_from_rgb(r11, g11, b11);
            v[uvno] = (unsigned char)((v_val + 2) / 4);
        }
    }
    return required_yuv_bytes;
}



int main(int argc, const char* argv[]) {
    if (argc != 9) {
        fprintf(stderr,
                "usage: img_in y_low y_high u_low u_high v_low v_high\n");
        return -1;
    }

    const char* in_fn = argv[1];
    int y_low = atoi(argv[2]);
    int y_high = atoi(argv[3]);
    int u_low = atoi(argv[4]);
    int u_high = atoi(argv[5]);
    int v_low = atoi(argv[6]);
    int v_high = atoi(argv[7]);

    unsigned int cols = 0;
    unsigned int rows = 0;
    unsigned int yuv_bytes;
    unsigned char* yuv;
    size_t in_fn_len = strlen(in_fn);
    if (in_fn_len < 4 || strcmp(&in_fn[in_fn_len - 4], ".yuv") == 0) {
        // This is a .yuv file.

        // Read the .yuv file.

        FILE* fp = fopen(in_fn, "rb");
        if (fp == NULL) {
            fprintf(stderr, "can't open %s for reading\n", in_fn);
            return -1;
        }
        const size_t MAX_YUV_BYTES = 13000000;
        yuv = (unsigned char*)malloc(MAX_YUV_BYTES);
        yuv_bytes = fread(yuv, 1, MAX_YUV_BYTES, fp);
        fclose(fp);

        // Guess the image size from the file size.

        unsigned int ii;
        const unsigned int img_size_table[][2] = { { 1920, 1080 }, 
                                                   { 2592, 1944 },
                                                   { 1296,  972 },
                                                   { 1296,  730 },
                                                   {  640,  480 },
                                                   { 3280, 2464 },
                                                   { 1640, 1232 },
                                                   { 1640,  922 },
                                                   { 1280,  720 } };
#define IMG_SIZE_COUNT (sizeof(img_size_table) / sizeof(img_size_table[0]))
        for (ii = 0; ii < IMG_SIZE_COUNT; ++ii) {
            unsigned int w = img_size_table[ii][0];
            unsigned int h = img_size_table[ii][1];
            if (yuv_bytes == w * h * 3 / 2) {
                cols = w;
                rows = h;
                break;
            }
        }
        if (ii == IMG_SIZE_COUNT) {
            fprintf(stderr, "read %u bytes from %s; unrecognized file size\n",
                    yuv_bytes, in_fn);
            return -1;
        }
    } else {
        // Assume it's a .jpg file.

        // Read the .jpg file.

        unsigned char* rgb = jpeg_read(in_fn, &cols, &rows);
        if (rgb == NULL) {
            fprintf(stderr, "can't read %s\n", in_fn);
            return -1;
        }

        // Convert it to YUV440 format.

        unsigned int pixels = cols * rows;
        yuv_bytes = pixels * 3 / 2;
        yuv = (unsigned char*)malloc(yuv_bytes);
        convert_rgb_to_yuv440(cols, rows, rgb, yuv_bytes, yuv);
        free(rgb);
    }

#define MAX_RUNS 10000
#define MAX_BLOBS 1000
    Blob_List bl = blob_list_init(MAX_RUNS, MAX_BLOBS);
    int err_no = detect_color_blobs(&bl, y_low, u_low, u_high, v_low, v_high,
                                    true, cols, rows, yuv);
    unsigned char red[3] = { 255, 0, 255 };
    draw_bounding_boxes(&bl, 30, red, cols, rows, yuv);
    blob_list_deinit(&bl);

    FILE* fp = fopen("out.yuv", "w");
    if (fp == NULL) {
        fprintf(stderr, "can't open out.yuv for writing\n");
        return -1;
    }
    fwrite(yuv, 1, yuv_bytes, fp);
    free(yuv);
    return 0;
}