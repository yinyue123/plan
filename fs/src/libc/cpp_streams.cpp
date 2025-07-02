#include "../../include/libc.h"
#include <sys/stat.h>

namespace fs_libc {

// FileBuf implementation
FileBuf::FileBuf(const char* filename, std::ios_base::openmode mode) 
    : fd_(-1), buffer_(nullptr), buffer_size_(8192), owns_fd_(true) {
    int flags = 0;
    if (mode & std::ios_base::in && mode & std::ios_base::out) {
        flags = O_RDWR;
    } else if (mode & std::ios_base::out) {
        flags = O_WRONLY | O_CREAT;
    } else {
        flags = O_RDONLY;
    }
    
    if (mode & std::ios_base::trunc) flags |= O_TRUNC;
    if (mode & std::ios_base::app) flags |= O_APPEND;
    
    fd_ = ::open(filename, flags, 0644);
    if (fd_ >= 0) {
        buffer_ = new char[buffer_size_];
    }
}

FileBuf::~FileBuf() {
    if (owns_fd_ && fd_ >= 0) {
        ::close(fd_);
    }
    delete[] buffer_;
}

// Virtual function implementations for streambuf
std::streambuf::int_type FileBuf::underflow() {
    if (fd_ < 0) return traits_type::eof();
    
    ssize_t n = ::read(fd_, buffer_, buffer_size_);
    if (n <= 0) return traits_type::eof();
    
    setg(buffer_, buffer_, buffer_ + n);
    return traits_type::to_int_type(*gptr());
}

std::streambuf::int_type FileBuf::uflow() {
    int_type ch = underflow();
    if (ch != traits_type::eof()) {
        gbump(1);
    }
    return ch;
}

std::streambuf::int_type FileBuf::overflow(int_type c) {
    if (fd_ < 0) return traits_type::eof();
    
    if (c != traits_type::eof()) {
        char ch = traits_type::to_char_type(c);
        if (::write(fd_, &ch, 1) != 1) {
            return traits_type::eof();
        }
    }
    return c;
}

int FileBuf::sync() {
    if (fd_ >= 0) {
        return ::fsync(fd_);
    }
    return -1;
}

std::streambuf::pos_type FileBuf::seekoff(off_type off, std::ios_base::seekdir way, std::ios_base::openmode) {
    if (fd_ < 0) return pos_type(off_type(-1));
    
    int whence = SEEK_SET;
    if (way == std::ios_base::cur) whence = SEEK_CUR;
    else if (way == std::ios_base::end) whence = SEEK_END;
    
    off_t result = ::lseek(fd_, off, whence);
    return result == -1 ? pos_type(off_type(-1)) : pos_type(result);
}

std::streambuf::pos_type FileBuf::seekpos(pos_type sp, std::ios_base::openmode which) {
    return seekoff(sp, std::ios_base::beg, which);
}

// Directory iterator implementation
directory_iterator::directory_iterator(const std::string& path) 
    : dir_(nullptr), current_(nullptr), path_(path) {
    dir_ = ::opendir(path.c_str());
    if (dir_) {
        ++(*this);  // Read first entry
    }
}

directory_iterator::~directory_iterator() {
    if (dir_) {
        ::closedir(dir_);
    }
}

directory_iterator::directory_iterator(directory_iterator&& other) noexcept
    : dir_(other.dir_), current_(other.current_), path_(std::move(other.path_)) {
    other.dir_ = nullptr;
    other.current_ = nullptr;
}

directory_iterator& directory_iterator::operator=(directory_iterator&& other) noexcept {
    if (this != &other) {
        if (dir_) ::closedir(dir_);
        dir_ = other.dir_;
        current_ = other.current_;
        path_ = std::move(other.path_);
        other.dir_ = nullptr;
        other.current_ = nullptr;
    }
    return *this;
}

directory_iterator& directory_iterator::operator++() {
    if (dir_) {
        current_ = ::readdir(dir_);
    }
    return *this;
}

directory_iterator directory_iterator::operator++(int) {
    directory_iterator tmp = std::move(*this);
    ++(*this);
    return tmp;
}

bool directory_iterator::operator==(const directory_iterator& other) const {
    return dir_ == other.dir_ && current_ == other.current_;
}

bool directory_iterator::operator!=(const directory_iterator& other) const {
    return !(*this == other);
}

// File system operations
file_status status(const std::string& path) {
    struct stat st;
    if (::stat(path.c_str(), &st) == 0) {
        return file_status(st);
    }
    return file_status();
}

bool exists(const std::string& path) {
    return status(path).is_valid();
}

bool is_regular_file(const std::string& path) {
    return status(path).is_regular_file();
}

bool is_directory(const std::string& path) {
    return status(path).is_directory();
}

std::uintmax_t file_size(const std::string& path) {
    return status(path).file_size();
}

} // namespace fs_libc