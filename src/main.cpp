#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <iostream>
#include <cstring>
#include <vector>
#include <cstdlib>
#include "kryfs.hpp"

// fuse_ops.cpp içerisindeki operasyon bağlayıcı fonksiyon
void init_fuse_operations(struct fuse_operations* ops);

void print_usage(const char* prog_name) {
    std::cout << "Kullanım:\n";
    std::cout << "  Mount etmek için:   " << prog_name << " -m <disk_imaji.img> <montaj_dizini> [-d]\n";
    std::cout << "  Umount etmek için:  " << prog_name << " -u <montaj_dizini>\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage(argv[0]);
        return 1;
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
        if (!kryfs_load_image(img_path)) {
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
            std::cout << "KryfsMountSystem DEBUG modunda ön planda başlatılıyor..." << std::endl;
        } else {
            std::cout << "KryfsMountSystem arka planda başlatılıyor..." << std::endl;
        }
        
        fuse_argv.push_back(argv[3]); // mount_point

        struct fuse_operations ops;
        std::memset(&ops, 0, sizeof(ops));
        init_fuse_operations(&ops);

        int ret = fuse_main(fuse_argv.size(), fuse_argv.data(), &ops, NULL);

        // Çıkışta imajı güvenli kapat
        kryfs_close_image();

        return ret;
    }
    else {
        print_usage(argv[0]);
        return 1;
    }
}