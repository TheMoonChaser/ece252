#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include "lab_png.h"
#include <arpa/inet.h>

void readAllFile(int* counter, char *fileName){
    DIR *p_dir;
    struct dirent *p_dirent;
    char str[64];

    if((p_dir = opendir(fileName)) == NULL) {
        sprintf(str, "opendir(%s)", fileName);
		perror(str);
		exit(2);
    }

	while((p_dirent = readdir(p_dir)) != NULL) {
        if(strcmp(p_dirent->d_name,".")==0 || strcmp(p_dirent->d_name,"..")==0)    ///current dir OR parrent dir
		    continue;
        //if regular file
        else if(p_dirent->d_type == 8) {
			FILE *f;
			U8 sig[PNG_SIG_SIZE];

			//get absolute path
            memset(str,'\0', sizeof(fileName));
            strcpy(str, fileName);
            strcat(str, "/");
            strcat(str, p_dirent->d_name);

            //open file
            f = fopen(str, "rb");

            if(f != NULL){
                //read signature
                fread(sig, 1, PNG_SIG_SIZE, f);

                int png_signature_is_correct = is_png(sig, PNG_SIG_SIZE);

                //if signature not correct, output error
                if(png_signature_is_correct){
				    *counter += 1;
                    printf("%s/%s\n", fileName, p_dirent->d_name);
                    fclose(f);
                } else {
				    fclose(f);
			    }
			}
		}
        //if symbolic link
        else if(p_dirent->d_type == 10)
            continue;
        //if directory
        else if(p_dirent->d_type == 4) {
            memset(str,'\0', sizeof(fileName));
            strcpy(str, fileName);
            strcat(str, "/");
            strcat(str, p_dirent->d_name);
            readAllFile(counter, str);
        }	
	}

	if(closedir(p_dir) != 0){
		perror("closedir");
		exit(3);
    }
}


int main(int argc, char *argv[]){
    //counts number of files found
    int counter = 0;

    if(argc == 1){
        fprintf(stderr, "Usage: %s <directory name>\n", argv[0]);
        exit(1);
    }	

    //Find files under the directory
    readAllFile(&counter, argv[1]);

    if(counter == 0){
        printf("findpng: No PNG file found\n");		
    }

	return 0;
}
