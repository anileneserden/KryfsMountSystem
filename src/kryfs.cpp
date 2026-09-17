#include "kryfs.hpp"
#include <iostream>
#include <cstring>

std::string g_img_path = "";
FILE* g_img_file = nullptr;
KryfsSuperblock g_kry_sb;
std::vector<KryfsInode> g_kry_inodes;

bool kryfs_load_image(const std::string& img_path) {
    g_img_path = img_path;
    g_img_file = std::fopen(img_path.c_str(), "r+b");
    
    if (!g_img_file) {
        std::cerr << "Hata: Disk imaji acilamadi: " << img_path << std::endl;
        return false;
    }

    // Superblock oku
    std::fread(&g_kry_sb, sizeof(KryfsSuperblock), 1, g_img_file);

    // --- İLK NON-ZERO (DOLU) ALANI TARK ET ---
    std::rewind(g_img_file);
    std::vector<unsigned char> scan_buf(4096); // İlk 4 KB'ı oku
    std::fread(scan_buf.data(), 1, 4096, g_img_file);
    
    std::cout << "[SCANNER] Sıfırdan farklı ilk baytlar:" << std::endl;
    int found_count = 0;
    for(size_t i = sizeof(KryfsSuperblock); i < scan_buf.size(); i++) {
        if(scan_buf[i] != 0x00) {
            std::printf("Offset %zu (0x%zX) -> 0x%02X\n", i, i, scan_buf[i]);
            found_count++;
            if(found_count > 20) { // Sadece ilk 20 anlamlı baytı göster
                std::cout << "... ve devamı var." << std::endl;
                break;
            }
        }
    }
    if(found_count == 0) {
        std::cout << "UYARI: Superblock sonrasında ilk 4 KB tamamen boş (hiç dosya yazılmamış görünüyor)!" << std::endl;
    }
    std::cout << "-----------------------------------" << std::endl;
    // -----------------------------------------

    if (g_kry_sb.magic != KRYFS_MAGIC) {
        std::cerr << "Hata: Gecersiz KRYFS imaji (Magic Number uyusmuyor)!" << std::endl;
        std::fclose(g_img_file);
        return false;
    }

    std::cout << "Superblock basariyla okundu. Inode sayisi: " << g_kry_sb.inode_count << std::endl;

    // Dinamik olarak inode dizisini boyutlandır
    g_kry_inodes.resize(g_kry_sb.inode_count);

    // Inode tablosunun başlangıcı (1. blok / 512. bayt)
    std::fseek(g_img_file, sizeof(KryfsSuperblock), SEEK_SET);

    // Inode tablosunu oku
    std::fread(g_kry_inodes.data(), sizeof(KryfsInode), g_kry_sb.inode_count, g_img_file);
    
    // --- DEBUG: Aktif inode'ları kontrol et ---
    int active_count = 0;
    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (g_kry_inodes[i].is_used) {
            std::cout << "[KRYFS] Aktif Dosya/Klasor Bulundu -> Inode ID: " << g_kry_inodes[i].inode_id 
                      << ", Isim: " << g_kry_inodes[i].filename 
                      << ", Boyut: " << g_kry_inodes[i].size << " bayt" << std::endl;
            active_count++;
        }
    }
    std::cout << "Toplam aktif dosya sayisi: " << active_count << std::endl;
    // ------------------------------------------

    std::cout << "KRYFS imaji basariyla yuklendi ve inode'lar okundu." << std::endl;
    return true;
}

void kryfs_save_image() {
    if (!g_img_file) return;

    // Superblock yaz
    std::rewind(g_img_file);
    std::fwrite(&g_kry_sb, sizeof(KryfsSuperblock), 1, g_img_file);

    // Inode tablosunu doğru ofsete yaz
    std::fseek(g_img_file, sizeof(KryfsSuperblock), SEEK_SET);
    std::fwrite(g_kry_inodes.data(), sizeof(KryfsInode), g_kry_sb.inode_count, g_img_file);
    
    std::fflush(g_img_file);
}

void kryfs_close_image() {
    if (g_img_file) {
        kryfs_save_image();
        std::fclose(g_img_file);
        g_img_file = nullptr;
    }
}