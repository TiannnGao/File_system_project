#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

char *os_info(char *image, char *os) {
    // Retrieve OS name from boot sector
    strncpy(os, &image[3], 8); // OS info starts at byte 3, length is 8 bytes
    os[8] = '\0'; // Null-terminate the string
    return os;
}

char *disk_label(char *image, char *label) {
    // Retrieve disk label from boot sector or root directory
    strncpy(label, &image[43], 11); // Check if disk label is in boot sector
    if (label[0] == ' ') { // Retrieve disk label from root directory
        int root_dir = 0x00002600; // Root directory starts at 0x00002600
        int attribute;
        while (root_dir < 0x00004200) { // 33 predefined sectors, each 512 bytes
            attribute = image[root_dir + 11]; // Attribute at byte 11
            if (attribute == 0x08) { // If attribute is 0x08, it's a volume label
                strncpy(label, &image[root_dir], 11);
                break;
            }
            root_dir += 32; // Move to next sector
        }
    }
    label[11] = '\0'; // Null-terminate the string
    return label;
}

int free_size(char *image) {
    // Calculate the free size in the disk
    int free_sectors = 0;
    int lower_bits, upper_bits, fat_entry;
    int total_sectors = image[19] + (image[20] << 8);
    int n = 2; // Start checking from the third sector

    while (n + 31 < total_sectors) { // Skip reserved sectors
        if (n % 2 == 0) { // Even FAT entry
            lower_bits = image[512 + 1 + 3 * n / 2] & 0x0F; // Lower 4 bits
            fat_entry = (lower_bits << 8) + (image[512 + 3 * n / 2] & 0xFF);
        } else { // Odd FAT entry
            upper_bits = image[512 + 3 * n / 2] & 0xF0; // Upper 4 bits
            fat_entry = (upper_bits >> 4) + ((image[512 + 1 + 3 * n / 2] & 0xFF) << 4);
        }
        if (fat_entry == 0x00) {
            free_sectors++;
        }
        n++;
    }
    return free_sectors * 512; // Return free size in bytes
}

int count_file(char *image, char *root) {
    // Count the total number of files
    int file_count = 0;

    while (image[0] != 0x00) { // Check if there's any content in the file name section
        if (image[0] != '.' && image[11] != 0x0F && (image[11] & 0x08) != 0x08) {
            if (image[11] != 0x10) { // Not a directory
                file_count++;
            } else {
                file_count += count_file(root + ((image[26] + 12) * 512), root); 
            }
        }
        image += 32; // Move to next directory entry
    }
    return file_count;
}

int main(int argc, char *argv[]) {
    // Main function to retrieve disk information and display it
    int file_descriptor;
    struct stat file_status;

    char os_name[9];
    char volume_label[12];
    int free_disk_space;
    int file_number;

    file_descriptor = open(argv[1], O_RDWR); // Open the disk image
    fstat(file_descriptor, &file_status);

    char *mapped_image = mmap(NULL, file_status.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
    if (mapped_image == MAP_FAILED) {
        printf("Error: failed to map memory\n");
        exit(1);
    }

    os_info(mapped_image, os_name);
    printf("OS Name: %s\n", os_name);
    disk_label(mapped_image, volume_label);
    printf("Label of the disk: %s\n", volume_label);
    printf("Total Size of the disk: %lu\n", (uint64_t)file_status.st_size);
    printf("Free size of the disk: %d\n", free_size(mapped_image));
    printf("==============\n");
    file_number = count_file(&mapped_image[0x2600], &mapped_image[0x2600]); // Start from root directory
    printf("The number of files in the disk: %d\n", file_number);
    printf("==============\n");
    printf("Number of FAT copies: %d\n", mapped_image[16]);
    printf("Sectors per FAT: %d\n", mapped_image[22] + (mapped_image[23] << 8));

    munmap(mapped_image, file_status.st_size);
    close(file_descriptor);
    return 0;
}
