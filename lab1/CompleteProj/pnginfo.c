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

int main(int argc, char *argv[])
{

	#define BUF_LEN2 (256*32)

    if (argc < 2) {
       printf("Incorrect inputs, must have at least one argument");
	}

    // accept inputs	
    char* path = argv[1];

	FILE *f;
	U8 sig[PNG_SIG_SIZE];
	U8 type[4];

	//crc we get from file
	U32 crc_out;
	//crc we get from crc function
	U32 crc_val;
	//length of type_concate_data
	uint64_t len_td;

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
	   fclose(f);
	   return 0;
    }

    struct data_IHDR output_data_IHDR;

    //get IHDR data
	get_png_data_IHDR(&output_data_IHDR, f, 16, SEEK_SET);
    
    //get width
	int width = 0;
	
	//get length
    int length = 0;

	width = get_png_width(&output_data_IHDR);
	length = get_png_height(&output_data_IHDR);

	printf("%s: %d x %d\n", path, width, length);

	//get IHDR CRC
	fseek(f,12,SEEK_SET);
	fread(type,1,4,f);
	U8 data_IHDR[13];
	fread(data_IHDR,1,13,f);
	fread(&crc_out,4,1,f);
	crc_out = htonl(crc_out);
	//check IHDR CRC value
	len_td = CHUNK_TYPE_SIZE + DATA_IHDR_SIZE;
	U8 type_data_IHDR[17];
	memcpy(type_data_IHDR,type,4);
	strcat(type_data_IHDR,data_IHDR);
	crc_val = crc(type_data_IHDR,len_td);
    if (crc_out != crc_val){
		printf("IHDR chunk CRC error: computed %x, expected %x\n",crc_out,crc_val);
	    fclose(f);
		return 0;
	}


	fclose(f);

    return 0;
}
