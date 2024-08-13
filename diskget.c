#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

int find_file(char *image, char *file) {
    // Check if the file we want to copy exists
    int root_dir = 0x2600; // Start from root directory
    char filename[12];
    char file_ext[4];
    int index;

    while (root_dir < 0x4200) { // Check the root directory only
        index = 0;
        while (image[root_dir + index] != ' ' && index < 8) {
            filename[index] = image[root_dir + index]; // Copy file name
            index++;
        }
        filename[index] = '\0';
        
        for (index = 0; index < 3; index++) {
            file_ext[index] = image[root_dir + 8 + index]; // Copy file extension
        }
        file_ext[index] = '\0';
        
        strcat(filename, ".");
        strcat(filename, file_ext);
        
        if (image[root_dir] == '.' || image[root_dir + 11] == 0x0F || 
            (image[root_dir + 11] & 0x08) == 0x08 || image[root_dir + 26] == 0x01 || image[root_dir + 26] == 0x0F) {
            root_dir += 32;
            continue;
        }
        
        if (strcmp(filename, file) == 0) {
            return root_dir;
        }
        root_dir += 32;    
    }
    return 0;
}

int fat_convert(char *image, int fat_index) {
    // Convert FAT12 entry to actual value
    int low_bits;
    int high_bits;
    int full_value;

    if (fat_index % 2 == 0) {
        low_bits = image[512 + 1 + 3 * fat_index / 2] & 0x0F; // Low 4 bits
        full_value = (low_bits << 8) + (image[512 + 3 * fat_index / 2] & 0xFF);
    } else {
        high_bits = image[512 + 3 * fat_index / 2] & 0xF0; // High 4 bits
        full_value = (high_bits >> 4) + ((image[512 + 1 + 3 * fat_index / 2] & 0xFF) << 4);
    }
    return full_value;
}

int get_size(char *image, int entry_index) {
    // Retrieve the file size from directory entry
    int file_size;
    file_size = (image[entry_index + 28] & 0xFF) +
                ((image[entry_index + 29] & 0xFF) << 8) +
                ((image[entry_index + 30] & 0xFF) << 16) +
                ((image[entry_index + 31] & 0xFF) << 24);
    return file_size;
} 

void copy_file(char *image, char *file, int size, int start_cluster) {
    // Copy the file content to a new file with the same name
    FILE *new_file;
    new_file = fopen(file, "wb"); // Create new file

    int sector_index;
    int physical_address;
    int next_cluster = fat_convert(image, start_cluster); // Get FAT table value
    int remaining_bytes;
    
    while (next_cluster != 0xFFF) {
        physical_address = (start_cluster + 31) * 512; // Convert to physical address
        for (sector_index = 0; sector_index < 512; sector_index++) {
            fputc(image[physical_address + sector_index], new_file); // Write content
        }
        start_cluster = next_cluster;
        next_cluster = fat_convert(image, start_cluster); // Traverse next sector
    }

    remaining_bytes = size - (size / 512) * 512; // Copy remaining data
    physical_address = (start_cluster + 31) * 512;
    for (sector_index = 0; sector_index < remaining_bytes; sector_index++) {
        fputc(image[physical_address + sector_index], new_file);
    }
    fclose(new_file);
    return;
}

int main(int argc, char *argv[]) {
    // Main function to handle file copying
    int file_descriptor;
    struct stat file_stat;

    int idx;
    char search_file[12];
    int file_found;
    int starting_cluster;
    int size_of_file;

    file_descriptor = open(argv[1], O_RDWR); // Open disk image file
    fstat(file_descriptor, &file_stat);

    char *mapped_image = mmap(NULL, file_stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0); 
    if (mapped_image == MAP_FAILED) {
        printf("Error: failed to map memory\n");
        exit(1);
    }

    for (idx = 0; argv[2][idx] != ' ' && idx < 12; idx++) {
        search_file[idx] = toupper(argv[2][idx]);
    }
    search_file[idx] = '\0';

    file_found = find_file(mapped_image, search_file);
    if (file_found != 0) {
        starting_cluster = (mapped_image[file_found + 26] & 0xFF) + ((mapped_image[file_found + 27] << 8) & 0xFF);
        size_of_file = get_size(mapped_image, file_found);
        copy_file(mapped_image, search_file, size_of_file, starting_cluster);
    } else {
        printf("File not found\n");
    }

    munmap(mapped_image, file_stat.st_size);
    close(file_descriptor);
    return 0;
}
