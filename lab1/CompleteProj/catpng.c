#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "crc.h"
#include "lab_png.h"
#include <arpa/inet.h>
#include "zutil.h"

int main(int argc, char *argv[]) {

    if (argc < 2) {
       printf("Incorrect inputs, must have at least one argument\n");
	   return 0;
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
        //current IHDR width
        U32 current_width = 0;
        //current IDAT data to be inflated
        U32 curr_len_inf = 0;
        uint64_t curr_len_inf_long = 0;

        f = fopen(argv[i], "rb");
        //get width 
        fseek(f, 16, SEEK_SET);
        fread(&current_width, 4, 1, f);
        current_width = ntohl(current_width);

        //get total height
        fread(&current_height, 4, 1, f);
        current_height = ntohl(current_height);
        total_height += current_height;

        //get IDAT length field
        fseek(f, 33, SEEK_SET);
        fread(&curr_len_inf, 4, 1, f);
        curr_len_inf = ntohl(curr_len_inf);

        //get IDAT data and outputs to a new file
        const int curr_IDAT_length = curr_len_inf;
        U8 curr_IDAT_data[curr_IDAT_length];
		fseek(f, 41, SEEK_SET);
        fread(curr_IDAT_data, 1, curr_IDAT_length, f);
        const int inf_length = current_height*(current_width*4 + 1);  //Height*(Width*4 + 1)
        U8 gp_buf_inf[inf_length];
		curr_len_inf_long = curr_len_inf;
        ret = mem_inf(gp_buf_inf, &len_inf, curr_IDAT_data, curr_len_inf_long);
        if(ret == 0) { /* success */
            fwrite(gp_buf_inf, 1, (int)len_inf, IDAT_total_data_file);
            len_inf_total += len_inf;
        } else { /* failure */
            printf("mem_def failed\n");
        }

        fclose(f);
	}

    const int len_inf_total_size = (int)len_inf_total;
    U8 total_inf_data[len_inf_total_size];
    //store file data to buffer
    fseek(IDAT_total_data_file, 0, SEEK_SET);
    fread(total_inf_data, 1, (int)len_inf_total, IDAT_total_data_file);
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
    
    //compute and write IHDR CRC
    uint64_t len_td_IHDR = CHUNK_TYPE_SIZE + DATA_IHDR_SIZE;
    fseek(combined_file, PNG_SIG_SIZE+CHUNK_LEN_SIZE+CHUNK_TYPE_SIZE, SEEK_SET);
    fread(IHDR_data, 1, 13, combined_file);
    memcpy(type_data_IHDR, IHDR_type, CHUNK_TYPE_SIZE);
    memcpy(type_data_IHDR+CHUNK_TYPE_SIZE, IHDR_data, DATA_IHDR_SIZE);
    computed_IHDR_CRC = crc(type_data_IHDR,len_td_IHDR);
    computed_IHDR_CRC = htonl(computed_IHDR_CRC);
    fwrite(&computed_IHDR_CRC, 4, 1, combined_file);
    
    //read IDAT Chunk
    U8 IDAT_type[CHUNK_TYPE_SIZE];
    U32 len_def_total_size = 0; 
    U32 computed_IDAT_CRC;
	U32 IDAT_length;
    
    //read initial IDAT length and type field 
    fseek(f, CHUNK_CRC_SIZE, SEEK_CUR);
    fread(&IDAT_length, 1, 4, f);
    IDAT_length = ntohl(IDAT_length);
    fread(IDAT_type, 1, 4, f);
    //get, set IDAT length field, type field and data field
    //compress data field
    const int def_source_len = (int)len_inf_total;
    U8 gp_buf_def[def_source_len];
    ret = mem_def(gp_buf_def, &len_def_total, total_inf_data, len_inf_total, Z_DEFAULT_COMPRESSION);
    if(ret == 0){ /* success */
        len_def_total_size = (uint32_t)len_def_total;
        len_def_total_size = htonl(len_def_total_size);
        fwrite(&len_def_total_size, 4, 1, combined_file);         //length
        fwrite(IDAT_type, 1, 4, combined_file);                   //type
		len_def_total_size = ntohl(len_def_total_size);
        fwrite(gp_buf_def, 1, len_def_total_size, combined_file); //data
    } else { /* failure */
        printf("mem_def failed\n");
    }

    //compute and write IDAT CRC
    const int type_data_size = CHUNK_TYPE_SIZE + len_def_total_size;
    U8 type_data_IDAT[type_data_size];
    uint64_t len_td_IDAT = CHUNK_TYPE_SIZE + len_def_total_size;
    memcpy(type_data_IDAT, IDAT_type, 4); 
    memcpy(type_data_IDAT+CHUNK_TYPE_SIZE, gp_buf_def, len_def_total_size);
    computed_IDAT_CRC = crc(type_data_IDAT, len_td_IDAT); 
    computed_IDAT_CRC = htonl(computed_IDAT_CRC);
    fwrite(&computed_IDAT_CRC, 4, 1, combined_file);
    
    //read IEND Chunk
    U32 IEND_length;
    U8 IEND_type[CHUNK_TYPE_SIZE];
    U32 IEND_CRC;
    
    //read IEND length field
    fseek(f, IDAT_length+CHUNK_CRC_SIZE, SEEK_CUR);
    fread(&IEND_length, 4, 1, f);
    IEND_length = ntohl(IEND_length);
    IEND_length = htonl(IEND_length);	
    //write IEND length field
    fwrite(&IEND_length, 4, 1, combined_file);

    //read IEND type
    fread(IEND_type, 1, 4, f);
    //write IEND type
    fwrite(IEND_type, 1, 4, combined_file);
    
    //read IEND CRC
    fread(&IEND_CRC, 4, 1, f);
    IEND_CRC = ntohl(IEND_CRC);
    IEND_CRC = htonl(IEND_CRC);
    //write IEND CRC  
    fwrite(&IEND_CRC, 4, 1, combined_file);
    
    fclose(combined_file);
    fclose(f);
    //remove helper file
    remove("TempFile.bin");
    return 0;
}
