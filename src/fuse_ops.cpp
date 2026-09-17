#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <cstring>
#include <errno.h>
#include <iostream>
#include "kryfs.hpp"

// Dosya veya dizin bilgilerini döndürür
static int kfs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    std::memset(stbuf, 0, sizeof(struct stat));

    // Kök dizin (/) kontrolü
    if (std::strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    const char* filename = path + 1; // Baştaki '/' işaretini atla

    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (g_kry_inodes[i].is_used && std::strcmp(g_kry_inodes[i].filename, filename) == 0) {
            if (g_kry_inodes[i].is_directory) {
                stbuf->st_mode = S_IFDIR | 0755;
                stbuf->st_nlink = 2;
            } else {
                stbuf->st_mode = S_IFREG | 0644;
                stbuf->st_nlink = 1;
                stbuf->st_size = g_kry_inodes[i].size;
            }
            return 0;
        }
    }

    return -ENOENT;
}

static int kfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    if (std::strcmp(path, "/") != 0) return -ENOENT;

    filler(buf, ".", NULL, 0, (fuse_fill_dir_flags)0);
    filler(buf, "..", NULL, 0, (fuse_fill_dir_flags)0);

    std::cout << "[FUSE DEBUG] readdir cagrildi. Toplam inode sayisi: " << g_kry_inodes.size() << std::endl;

    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (g_kry_inodes[i].is_used) {
            std::cout << "[FUSE DEBUG] Bulunan dosya/klasor: " << g_kry_inodes[i].filename << std::endl;
            filler(buf, g_kry_inodes[i].filename, NULL, 0, (fuse_fill_dir_flags)0);
        }
    }

    return 0;
}

// Yeni dosya oluşturma
static int kfs_create(const char* path, mode_t mode, struct fuse_file_info* fi) {
    const char* filename = path + 1;

    int free_index = -1;
    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (!g_kry_inodes[i].is_used) {
            free_index = i;
            break;
        }
    }

    if (free_index == -1) return -ENOSPC;

    g_kry_inodes[free_index].inode_id = free_index + 1;
    g_kry_inodes[free_index].is_used = 1;
    g_kry_inodes[free_index].is_directory = 0;
    g_kry_inodes[free_index].size = 0;
    g_kry_inodes[free_index].first_block = 0;
    std::strncpy(g_kry_inodes[free_index].filename, filename, MAX_FILENAME - 1);
    g_kry_inodes[free_index].filename[MAX_FILENAME - 1] = '\0';

    kryfs_save_image();
    return 0;
}

// Dosyaya yazma
static int kfs_write(const char* path, const char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    const char* filename = path + 1;
    
    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (g_kry_inodes[i].is_used && std::strcmp(g_kry_inodes[i].filename, filename) == 0) {
            g_kry_inodes[i].size = offset + size;
            kryfs_save_image();
            return size;
        }
    }

    return -ENOENT;
}

// Dosyadan okuma
static int kfs_read(const char* path, char* buf, size_t size, off_t offset, struct fuse_file_info* fi) {
    const char* filename = path + 1;

    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (g_kry_inodes[i].is_used && std::strcmp(g_kry_inodes[i].filename, filename) == 0) {
            std::string dummy_data = "KryonOS KRYFS Test Verisi\n";
            size_t len = dummy_data.size();
            
            if (offset < len) {
                if (offset + size > len) size = len - offset;
                std::memcpy(buf, dummy_data.c_str() + offset, size);
                return size;
            }
            return 0;
        }
    }

    return -ENOENT;
}

// Dosya silme
static int kfs_unlink(const char* path) {
    const char* filename = path + 1;

    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (g_kry_inodes[i].is_used && std::strcmp(g_kry_inodes[i].filename, filename) == 0) {
            // İnode alanını sıfırlayarak boşa çıkar
            g_kry_inodes[i].is_used = 0;
            g_kry_inodes[i].size = 0;
            g_kry_inodes[i].first_block = 0;
            g_kry_inodes[i].filename[0] = '\0';
            
            kryfs_save_image();
            return 0;
        }
    }

    return -ENOENT;
}

// Dizin (klasör) silme
static int kfs_rmdir(const char* path) {
    const char* dirname = path + 1; // Baştaki '/' işaretini atla

    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (g_kry_inodes[i].is_used && g_kry_inodes[i].is_directory && std::strcmp(g_kry_inodes[i].filename, dirname) == 0) {
            // Klasör inode alanını sıfırlayarak boşa çıkar
            g_kry_inodes[i].is_used = 0;
            g_kry_inodes[i].size = 0;
            g_kry_inodes[i].first_block = 0;
            g_kry_inodes[i].filename[0] = '\0';
            
            kryfs_save_image();
            return 0;
        }
    }

    return -ENOENT; // Dizin bulunamadı
}

// Yeni dizin (klasör) oluşturma
static int kfs_mkdir(const char* path, mode_t mode) {
    const char* dirname = path + 1; // Baştaki '/' işaretini atla

    int free_index = -1;
    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (!g_kry_inodes[i].is_used) {
            free_index = i;
            break;
        }
    }

    if (free_index == -1) return -ENOSPC; // Disk üzerinde boş inode kalmadı

    g_kry_inodes[free_index].inode_id = free_index + 1;
    g_kry_inodes[free_index].is_used = 1;
    g_kry_inodes[free_index].is_directory = 1; // Klasör olduğunu belirtiyoruz
    g_kry_inodes[free_index].size = 0;
    g_kry_inodes[free_index].first_block = 0;
    std::strncpy(g_kry_inodes[free_index].filename, dirname, MAX_FILENAME - 1);
    g_kry_inodes[free_index].filename[MAX_FILENAME - 1] = '\0';

    kryfs_save_image();
    return 0;
}

// Zaman damgası güncelleme
static int kfs_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info* fi) {
    return 0;
}

// FUSE Operasyon Yapısı
void init_fuse_operations(struct fuse_operations* ops) {
    ops->getattr  = kfs_getattr;
    ops->readdir  = kfs_readdir;
    ops->create   = kfs_create;
    ops->write    = kfs_write;
    ops->read     = kfs_read;
    ops->unlink   = kfs_unlink;
    ops->mkdir    = kfs_mkdir;
    ops->rmdir    = kfs_rmdir;
    ops->utimens  = kfs_utimens;
}