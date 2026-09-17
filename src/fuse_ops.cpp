#define FUSE_USE_VERSION 31
#include <fuse.h>
#include <cstring>
#include <errno.h>
#include <iostream>
#include <unistd.h>
#include "kryfs.hpp"

// Dosya sistemi ilk mount edildiğinde çalışır
static void* kfs_init(struct fuse_conn_info* conn, struct fuse_config* cfg) {
    std::cout << "[FUSE DEBUG] KRYFS dosya sistemi basariyla monte edildi." << std::endl;
    return nullptr;
}

// Dosya veya dizin bilgilerini döndürür
static int kfs_getattr(const char* path, struct stat* stbuf, struct fuse_file_info* fi) {
    std::memset(stbuf, 0, sizeof(struct stat));

    if (std::strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        return 0;
    }

    const char* filename = path + 1;

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

// Dizin içeriğini listeleme
static int kfs_readdir(const char* path, void* buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info* fi, enum fuse_readdir_flags flags) {
    std::string dir_path = path;
    if (dir_path == "/") {
        dir_path = "";
    } else if (dir_path.rfind('/', 0) == 0) {
        dir_path = dir_path.substr(1);
    }

    if (!dir_path.empty()) {
        bool dir_found = false;
        for (size_t i = 0; i < g_kry_inodes.size(); i++) {
            if (g_kry_inodes[i].is_used && g_kry_inodes[i].is_directory) {
                if (std::strcmp(g_kry_inodes[i].filename, dir_path.c_str()) == 0) {
                    dir_found = true;
                    break;
                }
            }
        }
        if (!dir_found) return -ENOENT;
    }

    if (offset == 0) {
        if (filler(buf, ".", NULL, 1, (fuse_fill_dir_flags)0)) return 0;
    }
    if (offset <= 1) {
        if (filler(buf, "..", NULL, 2, (fuse_fill_dir_flags)0)) return 0;
    }

    std::string prefix = dir_path.empty() ? "" : dir_path + "/";

    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (g_kry_inodes[i].is_used) {
            std::string fname(g_kry_inodes[i].filename);
            std::string sub_name;

            if (prefix.empty()) {
                if (fname.find('/') == std::string::npos) sub_name = fname;
            } else {
                if (fname.rfind(prefix, 0) == 0) {
                    std::string temp = fname.substr(prefix.length());
                    if (!temp.empty() && temp.find('/') == std::string::npos) sub_name = temp;
                }
            }

            if (!sub_name.empty()) {
                off_t next_off = i + 3;
                if (offset < next_off) {
                    if (filler(buf, sub_name.c_str(), NULL, next_off, (fuse_fill_dir_flags)0)) break;
                }
            }
        }
    }
    return 0;
}

// Dosyadan okuma (Düzeltilmiş senkronizasyon ile)
static int kfs_read(const char* path, char* buf, size_t size, off_t offset,
                    struct fuse_file_info* fi) {
    std::string target_path = path;
    if (target_path.rfind('/', 0) == 0) {
        target_path = target_path.substr(1);
    }

    KryfsInode* target_inode = nullptr;
    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        g_kry_inodes[i].filename[sizeof(g_kry_inodes[i].filename) - 1] = '\0';
        if (g_kry_inodes[i].is_used && !g_kry_inodes[i].is_directory) {
            if (std::strcmp(g_kry_inodes[i].filename, target_path.c_str()) == 0) {
                target_inode = &g_kry_inodes[i];
                break;
            }
        }
    }

    if (!target_inode) return -ENOENT;

    if (offset >= (off_t)target_inode->size) return 0;
    if (offset + size > target_inode->size) {
        size = target_inode->size - offset;
    }
    if (size == 0) return 0;
    if (!g_img_file) return -EIO;

    off_t disk_offset = (target_inode->first_block * BLOCK_SIZE) + offset;

    // Tamponu temizle ve dosya tanımlayıcısını senkronize et
    std::fflush(g_img_file);
    fsync(fileno(g_img_file));

    if (std::fseek(g_img_file, disk_offset, SEEK_SET) != 0) {
        return -EIO;
    }

    size_t bytes_read = std::fread(buf, 1, size, g_img_file);
    if (bytes_read == 0 && std::ferror(g_img_file)) {
        return -EIO;
    }

    std::string read_content(buf, bytes_read);
    std::cout << "[FUSE DEBUG] kfs_read basarili: Yol=" << path 
              << ", DiskOfset=" << disk_offset
              << ", Okunan Boyut=" << bytes_read << " bayt"
              << ", Icerik=[" << read_content << "]" << std::endl;

    return bytes_read;
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
    
    // ÇAKIŞMAYI ÖNLEMEK İÇİN GÜVENLİ BLOK OFSETİ:
    // İlk 256 blok metadata (Superblock + Inode Tablosu) için ayrıldı.
    // Veri blokları güvenli bir şekilde 256. bloktan (veya sonrasından) başlar.
    g_kry_inodes[free_index].first_block = 256 + free_index;
    
    std::strncpy(g_kry_inodes[free_index].filename, filename, MAX_FILENAME - 1);
    g_kry_inodes[free_index].filename[MAX_FILENAME - 1] = '\0';

    kryfs_save_image();
    return 0;
}

// Dosyaya yazma (Güçlendirilmiş disk flush ve fsync)
static int kfs_write(const char* path, const char* buf, size_t size, off_t offset,
                     struct fuse_file_info* fi) {
    std::string target_path = path;
    if (target_path.rfind('/', 0) == 0) {
        target_path = target_path.substr(1);
    }

    KryfsInode* target_inode = nullptr;
    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        g_kry_inodes[i].filename[sizeof(g_kry_inodes[i].filename) - 1] = '\0';
        if (g_kry_inodes[i].is_used && !g_kry_inodes[i].is_directory) {
            if (std::strcmp(g_kry_inodes[i].filename, target_path.c_str()) == 0) {
                target_inode = &g_kry_inodes[i];
                break;
            }
        }
    }

    if (!target_inode) return -ENOENT;
    if (!g_img_file) return -EIO;

    off_t disk_offset = (target_inode->first_block * BLOCK_SIZE) + offset;

    if (std::fseek(g_img_file, disk_offset, SEEK_SET) != 0) {
        return -EIO;
    }

    size_t bytes_written = std::fwrite(buf, 1, size, g_img_file);
    if (bytes_written == 0 && std::ferror(g_img_file)) {
        return -EIO;
    }

    if (offset + bytes_written > target_inode->size) {
        target_inode->size = offset + bytes_written;
    }

    // Değişiklikleri fiziksel olarak imaj dosyasına yaz
    std::fflush(g_img_file);
    fsync(fileno(g_img_file));
    
    kryfs_save_image(); // Inode güncellemelerini de kaydet

    std::string written_content(buf, bytes_written);
    std::cout << "[FUSE DEBUG] kfs_write basarili: Yol=" << path 
              << ", DiskOfset=" << disk_offset
              << ", Yazilan Boyut=" << bytes_written << " bayt"
              << ", Icerik=[" << written_content << "]" << std::endl;

    return bytes_written;
}

// Dosya silme
static int kfs_unlink(const char* path) {
    const char* filename = path + 1;
    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (g_kry_inodes[i].is_used && std::strcmp(g_kry_inodes[i].filename, filename) == 0) {
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

static int kfs_rmdir(const char* path) {
    const char* dirname = path + 1;
    for (size_t i = 0; i < g_kry_inodes.size(); i++) {
        if (g_kry_inodes[i].is_used && g_kry_inodes[i].is_directory && std::strcmp(g_kry_inodes[i].filename, dirname) == 0) {
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

static int kfs_mkdir(const char* path, mode_t mode) {
    const char* dirname = path + 1;
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
    g_kry_inodes[free_index].is_directory = 1;
    g_kry_inodes[free_index].size = 0;
    g_kry_inodes[free_index].first_block = 0;
    std::strncpy(g_kry_inodes[free_index].filename, dirname, MAX_FILENAME - 1);
    g_kry_inodes[free_index].filename[MAX_FILENAME - 1] = '\0';

    kryfs_save_image();
    return 0;
}

static int kfs_utimens(const char* path, const struct timespec tv[2], struct fuse_file_info* fi) {
    return 0;
}

void init_fuse_operations(struct fuse_operations* ops) {
    ops->init     = kfs_init;
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