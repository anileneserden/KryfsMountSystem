#ifndef KRYFS_HPP
#define KRYFS_HPP

#include <cstdint>
#include <string>
#include <cstdio>
#include <vector>

#define KRYFS_MAGIC 0x4B525946 // "KRYF"
#define BLOCK_SIZE 512
#define MAX_FILENAME 32

#pragma pack(push, 1)

// Çekirdekteki kryfs_superblock_t ile birebir aynı
struct KryfsSuperblock {
    uint32_t magic;
    uint32_t total_sectors;
    uint32_t inode_count;
    uint32_t block_size;
    char volume_name[32];
};

// Çekirdekteki kryfs_inode_t ile birebir aynı
struct KryfsInode {
    uint32_t inode_id;
    char filename[MAX_FILENAME];
    uint32_t size;
    uint32_t first_block;
    uint8_t is_used;
    uint8_t is_directory;
};

#pragma pack(pop)

// Disk İmajı Yönetim Fonksiyonları
bool kryfs_load_image(const std::string& img_path);
void kryfs_save_image();
void kryfs_close_image();

// Global Disk Yapıları (Dinamik)
extern std::string g_img_path;
extern FILE* g_img_file;
extern KryfsSuperblock g_kry_sb;
extern std::vector<KryfsInode> g_kry_inodes;

#endif