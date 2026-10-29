#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <iostream>
#include <cstring>
#include <vector>
#include <cstdlib>
#include "lnrfs.hpp"

// fuse_ops.cpp içerisindeki operasyon bağlayıcı fonksiyon
void init_fuse_operations(struct fuse_operations* ops);

void print_usage(const char* prog_name) {
    std::cout << "Kullanım:\n";
    std::cout << "  Mount etmek için:   " << prog_name << " -m <disk_imaji.img> <montaj_dizini> [-d]\n";
    std::cout << "  Umount etmek için:  " << prog_name << " -u <montaj_dizini>\n";
    std::cout << "  Formatlamak için:   " << prog_name << " <disk_imaji.img> --format\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
    }

    // FORMAT İŞLEMİ: ./kfs <disk_imaji> --format
    if (argc >= 3 && std::string(argv[2]) == "--format") {
        std::string img_path = argv[1];
        std::cout << "[LNRFS] Disk formatlanıyor ve Superblock yazılıyor: " << img_path << std::endl;

        // Format işlemi imajı oluşturur veya mevcut içeriği sıfırlar.
        FILE* img_file = std::fopen(img_path.c_str(), "wb+");
        if (!img_file) {
            std::cerr << "[LNRFS Hata] İmaj dosyası açılamadı: " << img_path << std::endl;
            return 1;
        }

        // Global imaj işaretçisini bağla
        g_img_file = img_file;

        // 2. Superblock alanını kernel formatıyla aynı şekilde doldur.
        g_kry_sb.magic = LNRFS_MAGIC; // 0x4B525946
        g_kry_sb.total_blocks = 2048;
        g_kry_sb.inode_table_block = 1;
        g_kry_sb.data_block_start = 256;

        size_t inode_table_bytes =
            (g_kry_sb.data_block_start - g_kry_sb.inode_table_block) * BLOCK_SIZE;
        g_kry_inodes.clear();
        g_kry_inodes.resize(inode_table_bytes / sizeof(LnrfsInode));

        // İmajı toplam blok boyutuna getir.
        std::fseek(img_file, (g_kry_sb.total_blocks * BLOCK_SIZE) - 1, SEEK_SET);
        std::fputc(0, img_file);

        // 3. Dosyanın başına gidip superblock'u ve sıfır inode tablosunu yaz.
        std::fseek(img_file, 0, SEEK_SET);
        std::fwrite(&g_kry_sb, sizeof(LnrfsSuperblock), 1, img_file);

        // 4. Global inode tablosunu sıfırla (first_block yerine start_block kullanılıyor)
        for (size_t i = 0; i < g_kry_inodes.size(); i++) {
            g_kry_inodes[i].is_used = 0;
            g_kry_inodes[i].is_directory = 0;
            g_kry_inodes[i].size = 0;
            g_kry_inodes[i].start_block = 0;
            g_kry_inodes[i].filename[0] = '\0';
        }

        // 5. Inode tablosunu imaj dosyasına kaydet
        lnrfs_save_image();

        // 6. Dosyayı kapat
        std::fclose(img_file);
        g_img_file = nullptr;

        std::cout << "[LNRFS] Disk başarıyla formatlandı ve Superblock oluşturuldu!" << std::endl;
        return 0;
    }

    std::string mode = argv[1];

    // UNMOUNT İŞLEMİ (-u)
    if (mode == "-u") {
        std::string mount_point = argv[2];
        std::string cmd = "fusermount3 -u " + mount_point;
        std::cout << "Unmount ediliyor: " << mount_point << std::endl;
        int ret = std::system(cmd.c_str());
        return ret;
    } 
    // MOUNT İŞLEMİ (-m)
    else if (mode == "-m") {
        if (argc < 4) {
            print_usage(argv[0]);
            return 1;
        }
        std::string img_path = argv[2];
        std::string mount_point = argv[3];

        // -d (debug) bayrağı kontrolü
        bool debug_mode = false;
        for (int i = 4; i < argc; i++) {
            if (std::string(argv[i]) == "-d") {
                debug_mode = true;
                break;
            }
        }

        // Disk imajını yükle
        if (!lnrfs_load_image(img_path)) {
            std::cerr << "[LNRFS Hata] İmaj yüklenemedi veya sihirli bayt (magic number) uyuşmuyor!" << std::endl;
            return 1;
        }

        // FUSE için argüman listesi hazırla
        std::vector<char*> fuse_argv;
        fuse_argv.push_back(argv[0]);
        
        char flag_f[] = "-f"; // Ön planda tutar
        char flag_d[] = "-d"; // FUSE debug modu

        if (debug_mode) {
            fuse_argv.push_back(flag_f);
            fuse_argv.push_back(flag_d);
            std::cout << "LnrfsMountSystem DEBUG modunda ön planda başlatılıyor..." << std::endl;
        } else {
            std::cout << "LnrfsMountSystem arka planda başlatılıyor..." << std::endl;
        }
        
        fuse_argv.push_back(argv[3]); // mount_point

        struct fuse_operations ops;
        std::memset(&ops, 0, sizeof(ops));
        init_fuse_operations(&ops);

        int ret = fuse_main(fuse_argv.size(), fuse_argv.data(), &ops, NULL);

        // Çıkışta imajı güvenli kapat
        lnrfs_close_image();

        return ret;
    }
    else {
        print_usage(argv[0]);
        return 1;
    }
}