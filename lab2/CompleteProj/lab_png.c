#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "crc.h"
#include "lab_png.h"
#include <arpa/inet.h>

int is_png(U8 *buf, size_t n){
    U8 correct_sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    for(size_t i = 0; i < n; i++){
       if(correct_sig[i] != buf[i]){
          return 0;
       }
    }
    return 1;
}

int get_png_height(struct data_IHDR *buf){
    return buf->height;
}

int get_png_width(struct data_IHDR *buf){
    return buf->width;
}

int get_png_data_IHDR(struct data_IHDR *out, FILE *fp, long offset, int whence){
    U32 four_bytes;
    U8 one_byte;
    fseek(fp, offset, whence);

    fread(&four_bytes, 4, 1, fp);
    out->width = htonl(four_bytes);
    fread(&four_bytes,4,1,fp);
    out->height = htonl(four_bytes);
    fread(&one_byte,1,1,fp);
    out->bit_depth = htonl(one_byte);
    fread(&one_byte,1,1,fp);
    out->color_type = htonl(one_byte);
    fread(&one_byte,1,1,fp);
    out->compression = htonl(one_byte);
    fread(&one_byte,1,1,fp);
    out->filter = htonl(one_byte);
    fread(&one_byte,1,1,fp);
    out->interlace = htonl(one_byte);

    return 1;
}

