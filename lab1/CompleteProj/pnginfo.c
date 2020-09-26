#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "crc.h"
#include "lab_png.h"


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

	fseek(fp, 20, whence);
    fread(&four_bytes, 4, 1, fp);
	int a = four_bytes;
	printf("---------------\n");
	printf("%d\n", a);
	printf("---------------\n");


	return 1;
}

int main(int argc, char *argv[])
{

    if (argc < 2) {
       printf("Incorrect inputs, must have at least one argument");
	}

    // accept inputs	
    char* path = argv[1];

	FILE *f;
	U8 sig[PNG_SIG_SIZE];

    //open file
	f = fopen(path, "rb");

    if(f == NULL){
       printf("Unable to open file!\n");
       exit(1);
    }

    //read signature
    fread(sig, 1, PNG_SIG_SIZE, f);

	int png_signature_is_correct = is_png(sig, PNG_SIG_SIZE);

	//if signature not correct, output error
	if(!png_signature_is_correct){
       printf("%s: Not a PNG file\n", path);
    } else {
       
    }

    struct data_IHDR output_data_IHDR;

    //get IHDR data
	get_png_data_IHDR(&output_data_IHDR, f, 8, SEEK_SET);
    
    //get width
	int width = 0;
	
	//get length
    int length = 0;

	printf("%s: %d x %d", path, width, length);

	fclose(f);

    return 0;
}
