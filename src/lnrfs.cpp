#include "lnrfs.hpp"
#include <iostream>
#include <cstring>

std::string g_img_path = "";
FILE* g_img_file = nullptr;
LnrfsSuperblock g_kry_sb;
std::vector<LnrfsInode> g_kry_inodes;

bool lnrfs_load_image(const std::string& img_path) {
    g_img_path = img_path;
    g_img_file = std::fopen(img_path.c_str(), "r+b");
    
    if (!g_img_file) {
        std::cerr << "Hata: Disk imaji acilamadi: " << img_path << std::endl;
        return false;
    }

    // Superblock oku
    std::fread(&g_kry_sb, sizeof(LnrfsSuperblock), 1, g_img_file);

    if (g_kry_sb.magic != LNRFS_MAGIC) {
        std::cerr << "Hata: Gecersiz LNRFS imaji (Magic Number uyusmuyor)!" << std::endl;
        std::fclose(g_img_file);
        return false;
    }

    if (g_kry_sb.inode_table_block >= g_kry_sb.data_block_start ||
        g_kry_sb.data_block_start >= g_kry_sb.total_blocks) {
        std::cerr << "Hata: LNRFS bolum sinirlari gecersiz." << std::endl;
        std::fclose(g_img_file);
        g_img_file = nullptr;
        return false;
    }

    std::cout << "Superblock basariyla okundu." << std::endl;
    std::cout << "-> Toplam Blok Sayisi: " << g_kry_sb.total_blocks << std::endl;
    std::cout << "-> Inode Tablo Bloku: " << g_kry_sb.inode_table_block << std::endl;

    size_t inode_table_bytes =
        (g_kry_sb.data_block_start - g_kry_sb.inode_table_block) * BLOCK_SIZE;
    size_t inode_count = inode_table_bytes / sizeof(LnrfsInode);
    if (inode_count == 0) {
        std::cerr << "Hata: LNRFS inode tablosu bos." << std::endl;
        std::fclose(g_img_file);
        g_img_file = nullptr;
        return false;
    }
    g_kry_inodes.resize(inode_count);

    // Inode tablosunu çekirdeğin bildirdiği bloktan (inode_table_block * BLOCK_SIZE) oku
    std::fseek(g_img_file, g_kry_sb.inode_table_block * BLOCK_SIZE, SEEK_SET);
    if (std::fread(g_kry_inodes.data(), sizeof(LnrfsInode), inode_count, g_img_file) != inode_count) {
        std::cerr << "Hata: LNRFS inode tablosu okunamadi." << std::endl;
        std::fclose(g_img_file);
        g_img_file = nullptr;
        return false;
    }
    
    // --- DEBUG: Aktif inode'ları kontrol et ---
    int active_count = 0;
    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        g_kry_inodes[i].filename[LNRFS_MAX_FILENAME - 1] = '\0';
        if (g_kry_inodes[i].is_used) {
            std::cout << "[LNRFS] Aktif Dosya/Klasor Bulundu -> Index: " << i 
                      << ", Isim: " << g_kry_inodes[i].filename 
                      << ", Boyut: " << g_kry_inodes[i].size << " bayt" << std::endl;
            active_count++;
        }
    }
    std::cout << "Toplam aktif dosya sayisi: " << active_count << std::endl;

    std::cout << "LNRFS imaji basariyla yuklendi ve inode'lar okundu." << std::endl;
    return true;
}

void lnrfs_save_image() {
    if (!g_img_file) return;

    // Superblock yaz (0. bloğun başına)
    std::rewind(g_img_file);
    std::fwrite(&g_kry_sb, sizeof(LnrfsSuperblock), 1, g_img_file);

    // Inode tablosunu doğru bloğa yaz
    std::fseek(g_img_file, g_kry_sb.inode_table_block * BLOCK_SIZE, SEEK_SET);
    std::fwrite(g_kry_inodes.data(), sizeof(LnrfsInode), g_kry_inodes.size(), g_img_file);
    
    std::fflush(g_img_file);
}

void lnrfs_close_image() {
    if (g_img_file) {
        lnrfs_save_image();
        std::fclose(g_img_file);
        g_img_file = nullptr;
    }
}