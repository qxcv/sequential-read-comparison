#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <cstring>

constexpr size_t CHUNK_SIZE = 1 << 20; // 1MB

// 16KiB is less realistic for deep learning, but has the advantage of fitting
// into L1 cache, so it's a more realistic choice for algorithms that process
// only a tiny chunk of data at a time (e.g. counting lines).
// constexpr size_t CHUNK_SIZE = 1 << 14; // 16KiB

// Process a chunk of data by XORing all longs together
long process_chunk(char* chunk, size_t size) {
    long result = 0;
    long* data = reinterpret_cast<long*>(chunk);
    size_t numLongs = size / sizeof(long);

    for (size_t i = 0; i < numLongs; ++i) {
        result ^= data[i];
    }

    return result;
}


void readMethod(const char* filePath) {
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    // this doesn't help with 1MiB blocks, but helps a lot
    // with 16KiB blocks (normally 16KiB blocks are 50% slower
    // than 1MiB blocks, but this equalizes the runtimes)
    posix_fadvise(fd, 0, 0, POSIX_FADV_SEQUENTIAL);

    char* buffer = new char[CHUNK_SIZE];
    ssize_t bytesRead;

    long res = 0;
    while ((bytesRead = read(fd, buffer, CHUNK_SIZE)) > 0) {
        res ^= process_chunk(buffer, CHUNK_SIZE);
    }

    std::cout << "Result: " << res << std::endl;

    close(fd);
    delete[] buffer;
}

void mmapMethod(const char* filePath) {
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("Error getting file size");
        exit(EXIT_FAILURE);
    }

    char* data = static_cast<char*>(mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) {
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }

    long res = 0;
    for (size_t offset = 0; offset < sb.st_size; offset += CHUNK_SIZE) {
        // Process data + offset
        res ^= process_chunk(data + offset, CHUNK_SIZE);
        madvise(data + offset, CHUNK_SIZE, MADV_FREE);
    }

    std::cout << "Result: " << res << std::endl;

    munmap(data, sb.st_size);
    close(fd);
}

void mmapOptMethod(const char* filePath) {
    int fd = open(filePath, O_RDONLY);
    if (fd < 0) {
        perror("Error opening file");
        exit(EXIT_FAILURE);
    }

    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("Error getting file size");
        exit(EXIT_FAILURE);
    }

    char* data = static_cast<char*>(mmap(nullptr, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0));
    if (data == MAP_FAILED) {
        perror("Error mmapping the file");
        exit(EXIT_FAILURE);
    }

    // Optimizations
    madvise(data, sb.st_size, MADV_SEQUENTIAL); // Optimize for sequential access
    // madvise(data, sb.st_size, MADV_WILLNEED);   // Hint that we will need this data
    // Consider MADV_HUGEPAGE if your access patterns and system configuration benefit from it

    long res = 0;
    for (size_t offset = 0; offset < sb.st_size; offset += CHUNK_SIZE) {
        // Process data + offset here
        res ^= process_chunk(data + offset, CHUNK_SIZE);
        madvise(data + offset, CHUNK_SIZE, MADV_FREE); // Release pages after use
    }
    std::cout << "Result: " << res << std::endl;

    munmap(data, sb.st_size);
    close(fd);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [read|mmap|mmap_opt] <file_path>" << std::endl;
        return EXIT_FAILURE;
    }

    const char* method = argv[1];
    const char* filePath = argv[2];

    if (strcmp(method, "read") == 0) {
        readMethod(filePath);
    } else if (strcmp(method, "mmap") == 0) {
        mmapMethod(filePath);
    } else if (strcmp(method, "mmap_opt") == 0) {
        mmapOptMethod(filePath);
    } else {
        std::cerr << "Invalid method. Choose 'read', 'mmap', or 'mmap_opt'." << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

