#include "fat32.h"
#include "printk.h"
#include "virtio.h"
#include "string.h"
#include "mbr.h"
#include "mm.h"

struct fat32_bpb fat32_header;
struct fat32_volume fat32_volume;

uint8_t fat32_buf[VIRTIO_BLK_SECTOR_SIZE];
uint8_t fat32_table_buf[VIRTIO_BLK_SECTOR_SIZE];

uint64_t cluster_to_sector(uint64_t cluster) {
    return (cluster - 2) * fat32_volume.sec_per_cluster + fat32_volume.first_data_sec;
}

uint32_t next_cluster(uint64_t cluster) {
    uint64_t fat_offset = cluster * 4;
    uint64_t fat_sector = fat32_volume.first_fat_sec + fat_offset / VIRTIO_BLK_SECTOR_SIZE;
    virtio_blk_read_sector(fat_sector, fat32_table_buf);
    int index_in_sector = fat_offset % (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
    return *(uint32_t*)(fat32_table_buf + index_in_sector);
}

void fat32_init(uint64_t lba, uint64_t size) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    fat32_volume.first_fat_sec = 0/* to calculate */;
    fat32_volume.sec_per_cluster = 0/* to calculate */;
    fat32_volume.first_data_sec = 0/* to calculate */;
    fat32_volume.fat_sz = 0/* to calculate */;
}

int is_fat32(uint64_t lba) {
    virtio_blk_read_sector(lba, (void*)&fat32_header);
    if (fat32_header.boot_sector_signature != 0xaa55) {
        return 0;
    }
    return 1;
}

int next_slash(const char* path) {  // util function to be used in fat32_open_file
    int i = 0;
    while (path[i] != '\0' && path[i] != '/') {
        i++;
    }
    if (path[i] == '\0') {
        return -1;
    }
    return i;
}

void to_upper_case(char *str) {     // util function to be used in fat32_open_file
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] -= 32;
        }
    }
}

/*struct fat32_file fat32_open_file(const char *path) {
    struct fat32_file file;
    
    return file;
}*/
struct fat32_file fat32_open_file(const char *path) {
    struct fat32_file file;
    /* todo: open the file according to path */
    char filename[9];
    memset(filename, ' ', 9);
    filename[8] = '\0';

    if (strlen(path) - 7 > 8)
        memcpy(filename, path + 7, 8);
    else 
        memcpy(filename, path + 7, strlen(path) - 7);
    to_upper_case(filename);

    uint64_t sector = fat32_volume.first_data_sec;

    virtio_blk_read_sector(sector, fat32_buf);
    struct fat32_dir_entry *entry = (struct fat32_dir_entry *)fat32_buf;

    for (int entry_index = 0; entry_index < fat32_volume.sec_per_cluster * FAT32_ENTRY_PER_SECTOR; ++entry_index) {
        char name[8];
        memcpy(name, entry->name, 8);
        for (int k = 0; k < 9; ++k)     //to upper case
            if (name[k] <= 'z' && name[k] >= 'a') 
                name[k] += 'A' - 'a';
        if (memcmp(filename, name, 8) == 0) {
            file.cluster = ((uint32_t)entry->starthi << 16) | entry->startlow;
            file.dir.index = entry_index;
            file.dir.cluster = 2;
            return file;
        }
        ++entry;
    }
    printk("[S] file not found!\n");

    return file;
}
//refer
int64_t fat32_lseek(struct file* file, int64_t offset, uint64_t whence) {
    if (whence == SEEK_SET) {
        file->cfo = 0/* to calculate */;
    } else if (whence == SEEK_CUR) {
        file->cfo = 0/* to calculate */;
    } else if (whence == SEEK_END) {
        /* Calculate file length */
        file->cfo = 0/* to calculate */;
    } else {
        printk("fat32_lseek: whence not implemented\n");
        while (1);
    }
    return file->cfo;
}

uint64_t fat32_table_sector_of_cluster(uint32_t cluster) {
    return fat32_volume.first_fat_sec + cluster / (VIRTIO_BLK_SECTOR_SIZE / sizeof(uint32_t));
}

int64_t fat32_read(struct file* file, void* buf, uint64_t len) {
    /* todo: read content to buf, and return read length */
    return 0;
}

int64_t fat32_write(struct file* file, const void* buf, uint64_t len) {
    /* todo: fat32_write */
    return 0;
}