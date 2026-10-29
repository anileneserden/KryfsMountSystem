#ifndef LNRFS_HPP
#define LNRFS_HPP

#include <cstdint>
#include <string>
#include <cstdio>
#include <vector>

#define LNRFS_MAGIC 0x4C4E5246 // "LNRF"
#define BLOCK_SIZE 512
#define LNRFS_MAX_FILENAME 256

#pragma pack(push, 1)

// Çekirdekteki lnrfs_superblock_t ile birebir aynı
struct LnrfsSuperblock {
    uint32_t magic;              // Dosya sistemi imzası (0x4C4E5246)
    uint32_t total_blocks;       // Toplam blok sayısı
    uint32_t inode_table_block;  // Inode tablosunun başlangıç bloğu
    uint32_t data_block_start;   // Veri bloklarının başlangıç bloğu
};

// Çekirdekteki lnrfs_inode_t ile birebir aynı
struct LnrfsInode {
    uint8_t  is_used;                        // Bu inode dolu mu? (1 = Dolu, 0 = Boş)
    uint8_t  is_directory;                   // Dizin mi, dosya mı?
    uint32_t size;                           // Dosya boyutu (bayt cinsinden)
    uint32_t start_block;                    // Verinin başladığı ilk blok indeksi
    char     filename[LNRFS_MAX_FILENAME];   // Dosya veya klasör adı
};

#pragma pack(pop)

// Disk İmajı Yönetim Fonksiyonları
bool lnrfs_load_image(const std::string& img_path);
void lnrfs_save_image();
void lnrfs_close_image();

// Global Disk Yapıları (Dinamik)
extern std::string g_img_path;
extern FILE* g_img_file;
extern LnrfsSuperblock g_kry_sb;
extern std::vector<LnrfsInode> g_kry_inodes;

#endif