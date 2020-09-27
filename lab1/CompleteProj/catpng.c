#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "crc.h"
#include "lab_png.h"
#include <arpa/inet.h>
#include "zutil.h"

#define BUF_LEN (256*32)

int main(int argc, char *argv[]) {

    if (argc < 2) {
       printf("Incorrect inputs, must have at least one argument");
    }
 
	FILE *f;
	FILE *IDAT_total_data_file;
	FILE *combined_file;
    U32 total_height = 0;
    uint64_t len_def_total = 0;
    uint64_t len_inf = 0;
    uint64_t len_inf_total = 0;
    int ret = 0;

    IDAT_total_data_file = fopen("TempFile.bin", "ab+");

    for(int i = 1; i < argc; i++){
	    //current IHDR height
        U32 current_height = 0;
        //current IDAT data to be inflated
        uint64_t curr_len_inf = 0;

        //get total height
        f = fopen(argv[i], "rb");
        fseek(f, 20, SEEK_SET);
        fread(&current_height, 4, 1, f);
        total_height += ntohl(current_height);

        //get IDAT length field
        fseek(f, 33, SEEK_SET);
        fread(&curr_len_inf, 4, 1, f);

        //get IDAT data and outputs to a new file
        const int curr_IDAT_length = curr_len_inf;
        U8 curr_IDAT_data[curr_IDAT_length];
        fread(curr_IDAT_data, 1, curr_IDAT_length, f);
        U8 gp_buf_inf[BUF_LEN];
        ret = mem_inf(gp_buf_inf, &len_inf, curr_IDAT_data, curr_len_inf);
        if(ret == 0) { /* success */
            fwrite(gp_buf_inf, 1, len_inf, IDAT_total_data_file);
            len_inf_total += len_inf;
        } else { /* failure */
            printf("mem_def failed\n");
        }

        fclose(f);
	}

    U8 total_inf_data[len_inf_total];
    //store file data to buffer
    fseek(f, 0, SEEK_SET);
    fread(total_inf_data, 1, len_inf_total, IDAT_total_data_file);
    fclose(IDAT_total_data_file);

    //Concate the PNG files
    combined_file = fopen("all.png", "ab+");
    U8 signature[PNG_SIG_SIZE];

    f = fopen(argv[1], "rb");
    //read signature
    fread(signature, 1, PNG_SIG_SIZE, f);
    //write signature
    fwrite(signature, 1, PNG_SIG_SIZE, combined_file); 

    //read IHDR Chunk
	U32 IHDR_length;
    U8 IHDR_type[CHUNK_TYPE_SIZE];
    struct data_IHDR output_data_IHDR;
    U32 computed_IHDR_CRC; 
	U8 IHDR_data[DATA_IHDR_SIZE]; 
    U8 type_data_IHDR[17];

    //read IHDR length field
    fread(&IHDR_length, 4, 1, f);
    IHDR_length = ntohl(IHDR_length);
    IHDR_length = htonl(IHDR_length);
    //write IHDR length field
    fwrite(&IHDR_length, 4, 1, combined_file);
    
    //read IHDR type
    fread(IHDR_type, 1, 4, f);
    //write IHDR type
	fwrite(IHDR_type, 1, 4, combined_file);	

	//read IHDR data
    fread(&output_data_IHDR.width, 4, 1, f);
    output_data_IHDR.width = ntohl(output_data_IHDR.width);
    output_data_IHDR.width = htonl(output_data_IHDR.width);
    fread(&output_data_IHDR.height, 4, 1, f);
    output_data_IHDR.height = htonl(total_height);
    fread(&output_data_IHDR.bit_depth, 1, 1, f);
    fread(&output_data_IHDR.color_type, 1, 1, f);
    fread(&output_data_IHDR.compression, 1, 1, f);
    fread(&output_data_IHDR.filter, 1, 1, f);
    fread(&output_data_IHDR.interlace, 1, 1, f);
    //write IHDR data
    fwrite(&output_data_IHDR.width, 4, 1, combined_file);
    fwrite(&output_data_IHDR.height, 4, 1, combined_file);
    fwrite(&output_data_IHDR.bit_depth, 1, 1, combined_file);
    fwrite(&output_data_IHDR.color_type, 1, 1, combined_file);
    fwrite(&output_data_IHDR.compression, 1, 1, combined_file);
    fwrite(&output_data_IHDR.filter, 1, 1, combined_file);
    fwrite(&output_data_IHDR.interlace, 1, 1, combined_file);

	//Compute and write IHDR CRC
    uint64_t len_td = CHUNK_TYPE_SIZE + DATA_IHDR_SIZE;
    fseek(combined_file, PNG_SIG_SIZE+CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE, SEEK_SET);
    fread(IHDR_data, 1, 13, combined_file);
    memcpy(type_data_IHDR, IHDR_type, CHUNK_TYPE_SIZE);
    memcpy(type_data_IHDR+CHUNK_TYPE_SIZE, IHDR_data, DATA_IHDR_SIZE);
    computed_IHDR_CRC = crc(type_data_IHDR,len_td);
    computed_IHDR_CRC = htonl(computed_IHDR_CRC);
    fseek(combined_file, PNG_SIG_SIZE+CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE+DATA_IHDR_SIZE, SEEK_SET);
    fwrite(&computed_IHDR_CRC, 4, 1, combined_file);

    //read IDAT Chunk
	


	

    

}
    





    // accept inputs	
    char* path = argv[1];

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
