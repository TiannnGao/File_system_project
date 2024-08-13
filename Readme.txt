UVIC
Summer 2024
CSC 360 Assignment3
Tian Gao
V00937029

For compile my code, just excute "make" on termial

diskinfo.c: 
This program extracts a specified file from a FAT12 disk image and copies it to the current directory. It first opens and maps the disk image to memory. Then, it searches the root directory for the target file, retrieves its size and starting cluster, and copies the file's content to a new file in the current directory by following the FAT12 cluster chain.

diskinfo.c: 
This program retrieves and displays various information about a FAT12 disk image. It opens and maps the disk image to memory, then extracts and prints the OS name, disk label, total disk size, free space, number of files, and FAT-related information.

disklist.c: 
This program lists the contents of the root directory and its subdirectories in a FAT12 disk image. It opens and maps the disk image to memory, then iterates through directory entries to print file and directory details, including name, size, and creation date/time. The program uses recursion to explore and print subdirectory contents.

diskput.c:
This program copies a file from the current Linux directory into a specified directory in a FAT12 disk image. It verifies the existence of the file and directory, checks for sufficient free space, and updates the FAT table and directory entries to reflect the new file
