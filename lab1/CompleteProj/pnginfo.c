#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "crc.h"
#include "lab_png.h"
#include <arpa/inet.h>

int main(int argc, char *argv[]) {

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
	crc_out = ntohl(crc_out);
	//check IHDR CRC value
	len_td = CHUNK_TYPE_SIZE + DATA_IHDR_SIZE;
	U8 type_data_IHDR[17];
	memcpy(type_data_IHDR,type,4);
	memcpy(type_data_IHDR+4,data_IHDR,13);
	crc_val = crc(type_data_IHDR,len_td);
    if (crc_out != crc_val){
		printf("IHDR chunk CRC error: computed %x, expected %x\n",crc_val,crc_out);
	    fclose(f);
		return 0;
	}

	//get IDAT CRC
	U32 IDAT_len;
	fread(&IDAT_len,4,1,f);
	IDAT_len = ntohl(IDAT_len);
	const int IDAT_data_len = IDAT_len;
	const int IDAT_td_len = IDAT_data_len+CHUNK_TYPE_SIZE;

	fread(type,1,4,f);
	U8 IDAT_data[IDAT_data_len];
	fread(IDAT_data,1,IDAT_data_len,f);
	fread(&crc_out,4,1,f);
	crc_out=ntohl(crc_out);
	//check IDAT crc value
	len_td = CHUNK_TYPE_SIZE+IDAT_data_len;
	//IDAT_td contains type+data
	U8 IDAT_td[IDAT_td_len];
	memcpy(IDAT_td,type,4);
	memcpy(IDAT_td+4,IDAT_data,IDAT_data_len);
	crc_val = crc(IDAT_td,len_td);
	if (crc_out != crc_val){
		printf("IDAT chunk CRC error: computed %x, expected %x\n",crc_val,crc_out);
		fclose(f);
		return 0;
	}
	
	//get IEND CRC
	fseek(f,4,SEEK_CUR);
	fread(type,1,4,f);
	fread(&crc_out,4,1,f);
	crc_out = ntohl(crc_out);
	crc_val = crc(type,CHUNK_TYPE_SIZE);
	if (crc_out != crc_val){
		printf("IEND chunk CRC error: computed %x, expected %x\n",crc_val,crc_out);
		fclose(f);
		return 0;
	}

	fclose(f);

    return 0;
}
