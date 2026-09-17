cmake_minimum_required(VERSION 3.10)
project(KryfsMountSystem)

set(CMAKE_CXX_STANDARD 17)

# FUSE3 paketini bul
find_package(PkgConfig REQUIRED)
pkg_check_modules(FUSE3 REQUIRED fuse3)

# Header dosyalarının aranacağı dizin
include_directories(include ${FUSE3_INCLUDE_DIRS})
link_directories(${FUSE3_LIBRARY_DIRS})

# Kaynak dosyalar (src/ altındakiler)
add_executable(kfs 
    src/main.cpp 
    src/kryfs.cpp 
    src/fuse_ops.cpp
)

target_link_libraries(kfs ${FUSE3_LIBRARIES})
target_compile_options(kfs PRIVATE ${FUSE3_CFLAGS_OTHER})