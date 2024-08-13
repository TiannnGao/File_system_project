#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define timeOffset 14 // Offset of creation time in directory entry
#define dateOffset 16 // Offset of creation date in directory entry

static unsigned int bytes_per_sector;

int get_size(char *image) {
    // Retrieve the file size from the directory entry
    int size = (image[28] & 0xFF) +
               ((image[29] & 0xFF) << 8) +
               ((image[30] & 0xFF) << 16) +
               ((image[31] & 0xFF) << 24);
    return size;
}

void print_date_time(char *directory_entry_start_pos) {
    // Print the creation date and time from the directory entry
    int creation_time, creation_date;
    int hours, minutes, day, month, year;

    creation_time = *(unsigned short *)(directory_entry_start_pos + timeOffset);
    creation_date = *(unsigned short *)(directory_entry_start_pos + dateOffset);

    year = ((creation_date & 0xFE00) >> 9) + 1980; // Year stored as value since 1980
    month = (creation_date & 0x1E0) >> 5;          // Month stored in the middle four bits
    day = (creation_date & 0x1F);                  // Day stored in the low five bits

    printf("%d-%02d-%02d ", year, month, day);

    hours = (creation_time & 0xF800) >> 11; // Hours stored in the high five bits
    minutes = (creation_time & 0x7E0) >> 5; // Minutes stored in the middle six bits

    printf("%02d:%02d\n", hours, minutes);
}

char *print_file(char *image, char *original) {
    // Print the file or directory details and recursively explore subdirectories
    int file_size;
    char file_name[12];
    char file_extension[4];
    char file_type;
    char *sub_directory_ptr = NULL;
    int name_index;

    do {
        while (image[0] != 0x00) {
            if (image[26] != 0x00 && image[26] != 0x01 && image[11] != 0x0F && (image[11] & 0x08) != 0x08) {
                file_size = 0;

                if (image[11] == 0x10) { // Check if it's a directory
                    file_type = 'D';
                    if (image[0] == '.' || image[1] == '.' || image[0] == 1 || image[1] == 1 || image[0] == 0 || image[1] == 0) {
                        image += 32; // Skip empty directory entries
                        continue;
                    }
                } else {
                    file_type = 'F';
                    file_size = get_size(image);
                }

                // Copy the file name
                for (name_index = 0; image[name_index] != ' ' && name_index < 8; name_index++) {
                    file_name[name_index] = image[name_index];
                }
                file_name[name_index] = '\0';

                // Copy the file extension
                for (name_index = 0; name_index < 3; name_index++) {
                    file_extension[name_index] = image[8 + name_index];
                }
                file_extension[name_index] = '\0';

                // Concatenate file name and extension
                if (file_type == 'F') {
                    strcat(file_name, ".");
                }
                strcat(file_name, file_extension);

                if ((image[11] & 0x08) == 0) {
                    printf("%c %10u %20s ", file_type, file_size, file_name);
                    print_date_time(image);
                }

                if (image[11] == 0x10) { // Recursion for subdirectory
                    sub_directory_ptr = image;
                    image = print_file(image + 32, original);
                    char sub_dir_name[9];
                    strncpy(sub_dir_name, sub_directory_ptr, 8);
                    sub_dir_name[8] = '\0';
                    printf("\n");
                    printf("%s\n", sub_dir_name); // Print subdirectory name
                    printf("=============\n");
                    image = print_file(original + (sub_directory_ptr[26] + 12) * 512, original);
                }
            }
            image += 32; // Move to next directory entry
        }
        sub_directory_ptr = image;
    } while (sub_directory_ptr[0] != 0x00);

    return sub_directory_ptr;
}


int main(int argc, char *argv[]) {
    // Main function to print the root directory and its contents
    int file_descriptor;
    struct stat file_status;

    file_descriptor = open(argv[1], O_RDWR); // Open the disk image file
    fstat(file_descriptor, &file_status);

    char *mapped_memory = mmap(NULL, file_status.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
    if (mapped_memory == MAP_FAILED) {
        printf("Error: failed to map memory\n");
        exit(1);
    }

    printf("Root\n");
    printf("==============\n");
    print_file(mapped_memory + 0x2600, mapped_memory + 0x2600);

    munmap(mapped_memory, file_status.st_size);
    close(file_descriptor);

    return 0;
}
