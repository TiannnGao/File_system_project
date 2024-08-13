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
#include <time.h>

#define ROOT_DIR_OFFSET 0x2600
#define ROOT_DIR_SIZE 0x2000 // Size of the root directory (0x4200 - 0x2600)
#define SECTOR_SIZE 512
#define FAT_OFFSET 0x200
#define DATA_OFFSET 0x4200

// 函数原型声明
int fat_convert(char *image, int fat_index);
int find_empty_cluster(char *image);
void write_fat_entry(char *image, int cluster, int value);
void write_directory_entry(char *image, char *file_name, int cluster, int size, int dir_offset);
int find_directory(char *image, char *path, int dir_offset);
int check_free_space(char *image, int file_size);

int main(int argc, char *argv[]) {
    // Main function to write a file into the disk image
    int file_descriptor, input_file_descriptor;
    struct stat file_status;

    if (argc != 3 && argc != 4) {
        printf("Usage: %s <disk image> <filename> [<path>]\n", argv[0]);
        return 1;
    }

    file_descriptor = open(argv[1], O_RDWR); // Open the disk image
    if (file_descriptor < 0) {
        perror("Error opening disk image");
        return 1;
    }

    if (fstat(file_descriptor, &file_status) < 0) {
        perror("Error getting file status");
        close(file_descriptor);
        return 1;
    }

    char *mapped_image = mmap(NULL, file_status.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, file_descriptor, 0);
    if (mapped_image == MAP_FAILED) {
        perror("Error mapping memory");
        close(file_descriptor);
        return 1;
    }

    input_file_descriptor = open(argv[2], O_RDONLY);
    if (input_file_descriptor < 0) {
        perror("Error opening input file");
        munmap(mapped_image, file_status.st_size);
        close(file_descriptor);
        printf("File not found.\n");
        return 1;
    }

    struct stat input_file_status;
    if (fstat(input_file_descriptor, &input_file_status) < 0) {
        perror("Error getting input file status");
        close(input_file_descriptor);
        munmap(mapped_image, file_status.st_size);
        close(file_descriptor);
        return 1;
    }

    // Check if there is enough free space
    if (!check_free_space(mapped_image, input_file_status.st_size)) {
        printf("No enough free space in the disk image.\n");
        close(input_file_descriptor);
        munmap(mapped_image, file_status.st_size);
        close(file_descriptor);
        return 1;
    }

    // Determine the target directory
    int dir_offset = ROOT_DIR_OFFSET;
    if (argc == 4) {
        dir_offset = find_directory(mapped_image, argv[3], ROOT_DIR_OFFSET);
        if (dir_offset < 0) {
            printf("The directory not found.\n");
            close(input_file_descriptor);
            munmap(mapped_image, file_status.st_size);
            close(file_descriptor);
            return 1;
        }
    }

    int cluster = find_empty_cluster(mapped_image);
    if (cluster < 0) {
        printf("No empty cluster found\n");
        close(input_file_descriptor);
        munmap(mapped_image, file_status.st_size);
        close(file_descriptor);
        return 1;
    }

    // Write file data to the data area
    char buffer[SECTOR_SIZE];
    int bytes_read, sector = 0;
    while ((bytes_read = read(input_file_descriptor, buffer, SECTOR_SIZE)) > 0) {
        memcpy(mapped_image + DATA_OFFSET + (cluster - 2) * SECTOR_SIZE + sector * SECTOR_SIZE, buffer, bytes_read);
        sector++;
    }

    // Update FAT table
    write_fat_entry(mapped_image, cluster, 0xFFF); // Mark as end of file

    // Update directory entry
    write_directory_entry(mapped_image, argv[2], cluster, input_file_status.st_size, dir_offset);

    close(input_file_descriptor);
    munmap(mapped_image, file_status.st_size);
    close(file_descriptor);
    return 0;
}

int fat_convert(char *image, int fat_index) {
    // Convert FAT12 entry to actual cluster value
    int lower_bits, upper_bits, entry_value;

    if (fat_index % 2 == 0) {
        lower_bits = image[512 + 1 + 3 * fat_index / 2] & 0x0F; // Lower 4 bits
        entry_value = (lower_bits << 8) + (image[512 + 3 * fat_index / 2] & 0xFF);
    } else {
        upper_bits = image[512 + 3 * fat_index / 2] & 0xF0; // Upper 4 bits
        entry_value = (upper_bits >> 4) + ((image[512 + 1 + 3 * fat_index / 2] & 0xFF) << 4);
    }
    return entry_value;
}

int find_empty_cluster(char *image) {
    // Find an empty cluster in the FAT table
    for (int i = 2; i < 2849; i++) { // FAT12 has 2849 clusters
        int entry = fat_convert(image, i);
        if (entry == 0x000) {
            return i;
        }
    }
    return -1; // No empty cluster found
}

void write_fat_entry(char *image, int cluster, int value) {
    // Write a value into the FAT table for the given cluster
    if (cluster % 2 == 0) {
        image[FAT_OFFSET + 1 + 3 * cluster / 2] = (value >> 8) & 0x0F;
        image[FAT_OFFSET + 3 * cluster / 2] = value & 0xFF;
    } else {
        image[FAT_OFFSET + 3 * cluster / 2] = (value << 4) & 0xF0;
        image[FAT_OFFSET + 1 + 3 * cluster / 2] = (value >> 4) & 0xFF;
    }
}

void write_directory_entry(char *image, char *file_name, int cluster, int size, int dir_offset) {
    // Write a directory entry for the new file
    for (int i = 0; i < ROOT_DIR_SIZE; i += 32) {
        if (image[dir_offset + i] == 0x00 || image[dir_offset + i] == 0xE5) {
            // Process file name and extension
            char name[8] = "        ";
            char ext[3] = "   ";
            char *dot = strrchr(file_name, '.');
            if (dot != NULL) {
                strncpy(name, file_name, dot - file_name);
                strncpy(ext, dot + 1, 3);
            } else {
                strncpy(name, file_name, 8);
            }
            for (int j = 0; j < 8; j++) name[j] = toupper(name[j]);
            for (int j = 0; j < 3; j++) ext[j] = toupper(ext[j]);

            memcpy(image + dir_offset + i, name, 8);
            memcpy(image + dir_offset + i + 8, ext, 3);

            // Set cluster and size
            image[dir_offset + i + 26] = cluster & 0xFF;
            image[dir_offset + i + 27] = (cluster >> 8) & 0xFF;
            image[dir_offset + i + 28] = size & 0xFF;
            image[dir_offset + i + 29] = (size >> 8) & 0xFF;
            image[dir_offset + i + 30] = (size >> 16) & 0xFF;
            image[dir_offset + i + 31] = (size >> 24) & 0xFF;

            // Set creation date and time to last modification time
            struct stat st;
            stat(file_name, &st);
            struct tm *tm = localtime(&st.st_mtime);

            int year = tm->tm_year + 1900;
            int month = tm->tm_mon + 1;
            int day = tm->tm_mday;
            int hours = tm->tm_hour;
            int minutes = tm->tm_min;

            int creation_date = ((year - 1980) << 9) | (month << 5) | day;
            int creation_time = (hours << 11) | (minutes << 5);

            image[dir_offset + i + 16] = creation_date & 0xFF;
            image[dir_offset + i + 17] = (creation_date >> 8) & 0xFF;
            image[dir_offset + i + 14] = creation_time & 0xFF;
            image[dir_offset + i + 15] = (creation_time >> 8) & 0xFF;

            break;
        }
    }
}

int find_directory(char *image, char *path, int dir_offset) {
    // Split the path and traverse directories
    char *token = strtok(path, "/");
    while (token != NULL) {
        int found = 0;
        for (int i = 0; i < ROOT_DIR_SIZE; i += 32) {
            if (image[dir_offset + i] != 0x00 && image[dir_offset + i] != 0xE5) {
                char name[9] = "        ";
                memcpy(name, image + dir_offset + i, 8);
                for (int j = 0; j < 8; j++) name[j] = toupper(name[j]);
                if (strncmp(name, token, 8) == 0 && (image[dir_offset + i + 11] & 0x10)) {
                    // Found directory
                    int cluster = (image[dir_offset + i + 27] << 8) | image[dir_offset + i + 26];
                    dir_offset = DATA_OFFSET + (cluster - 2) * SECTOR_SIZE;
                    found = 1;
                    break;
                }
            }
        }
        if (!found) return -1; // Directory not found
        token = strtok(NULL, "/");
    }
    return dir_offset;
}

int check_free_space(char *image, int file_size) {
    int clusters_needed = (file_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int free_clusters = 0;
    for (int i = 2; i < 2849; i++) {
        if (fat_convert(image, i) == 0x000) {
            free_clusters++;
            if (free_clusters >= clusters_needed) return 1;
        }
    }
    return 0;
}
