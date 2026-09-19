#ifndef KRYFS_HPP
#define KRYFS_HPP

#include <cstdint>
#include <string>
#include <cstdio>
#include <vector>

#define KRYFS_MAGIC 0x4B525946 // "KRYF"
#define BLOCK_SIZE 512
#define KRYFS_MAX_FILENAME 32

#pragma pack(push, 1)

// Çekirdekteki kryfs_superblock_t ile birebir aynı
struct KryfsSuperblock {
    uint32_t magic;              // Dosya sistemi imzası (0x4B525946)
    uint32_t total_blocks;       // Toplam blok sayısı
    uint32_t inode_table_block;  // Inode tablosunun başlangıç bloğu
    uint32_t data_block_start;   // Veri bloklarının başlangıç bloğu
};

// Çekirdekteki kryfs_inode_t ile birebir aynı
struct KryfsInode {
    uint8_t  is_used;                        // Bu inode dolu mu? (1 = Dolu, 0 = Boş)
    uint8_t  is_directory;                   // Dizin mi, dosya mı?
    uint32_t size;                           // Dosya boyutu (bayt cinsinden)
    uint32_t start_block;                    // Verinin başladığı ilk blok indeksi
    char     filename[KRYFS_MAX_FILENAME];   // Dosya veya klasör adı
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