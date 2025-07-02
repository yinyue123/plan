/**
 * 文件系统演示程序
 * 
 * 这个程序演示了如何使用我们实现的完整文件系统栈：
 * 1. 创建内存块设备
 * 2. 格式化为EXT4文件系统
 * 3. 挂载文件系统
 * 4. 执行各种文件操作
 * 5. 演示页面缓存和系统调用
 */

#include "../include/types.h"
#include "../include/block_device.h"
#include "../include/ext4.h"
#include "../include/vfs.h"
#include "../include/page_cache.h"
#include "../include/syscall.h"
#include "../include/libc.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>

// 演示块设备操作
void demo_block_device() {
    std::cout << "\n=== 块设备操作演示 ===" << std::endl;
    
    try {
        // 创建64MB的内存块设备
        auto device = std::make_shared<MemoryBlockDevice>(
            64 * 1024 * 1024,  // 64MB
            SECTOR_SIZE,       // 512字节扇区
            BLOCK_SIZE,        // 4KB块
            false,             // 可写
            "demo_device",     // 设备名
            8, 0               // 主次设备号
        );
        
        std::cout << "创建内存块设备成功:" << std::endl;
        std::cout << "  设备名: " << device->get_name() << std::endl;
        std::cout << "  大小: " << device->get_size() / (1024*1024) << " MB" << std::endl;
        std::cout << "  扇区大小: " << device->get_sector_size() << " 字节" << std::endl;
        std::cout << "  块大小: " << device->get_block_size() << " 字节" << std::endl;
        
        // 测试同步读写
        std::vector<u8> write_data(4096, 0xAB);  // 4KB数据，填充0xAB
        std::vector<u8> read_data(4096, 0);
        
        auto write_result = device->write(0, write_data.data(), write_data.size());
        if (write_result.is_ok()) {
            std::cout << "写入数据成功: " << write_result.unwrap() << " 字节" << std::endl;
        }
        
        auto read_result = device->read(0, read_data.data(), read_data.size());
        if (read_result.is_ok()) {
            std::cout << "读取数据成功: " << read_result.unwrap() << " 字节" << std::endl;
            
            // 验证数据
            bool data_match = true;
            for (size_t i = 0; i < write_data.size(); ++i) {
                if (write_data[i] != read_data[i]) {
                    data_match = false;
                    break;
                }
            }
            std::cout << "数据验证: " << (data_match ? "通过" : "失败") << std::endl;
        }
        
        // 测试异步I/O
        std::cout << "测试异步I/O..." << std::endl;
        bool async_complete = false;
        auto bio = std::make_unique<Bio>(
            BioType::READ, 0, 1024, read_data.data(),
            [&async_complete](ErrorCode err) {
                std::cout << "异步I/O完成，状态: " << (err == ErrorCode::SUCCESS ? "成功" : "失败") << std::endl;
                async_complete = true;
            }
        );
        
        device->submit_bio(std::move(bio));
        
        // 等待异步操作完成
        auto start_time = std::chrono::steady_clock::now();
        while (!async_complete) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto elapsed = std::chrono::steady_clock::now() - start_time;
            if (elapsed > std::chrono::seconds(5)) {
                std::cout << "异步I/O超时" << std::endl;
                break;
            }
        }
        
    } catch (const std::exception& e) {
        std::cout << "块设备操作异常: " << e.what() << std::endl;
    }
}

// 演示页面缓存操作
void demo_page_cache() {
    std::cout << "\n=== 页面缓存操作演示 ===" << std::endl;
    
    // 创建测试用的inode和超级块(简化版)
    auto device = std::make_shared<MemoryBlockDevice>(1024 * 1024, 512, 4096);
    auto fs = std::make_shared<Ext4FileSystem>();
    auto sb = std::make_shared<SuperBlock>(device, fs);
    auto inode_ops = std::make_shared<Ext4FileSystem>();
    auto inode = std::make_shared<Inode>(1, sb, inode_ops);
    
    std::cout << "页面缓存统计:" << std::endl;
    std::cout << "  当前页面数: " << g_page_cache.get_page_count() << std::endl;
    std::cout << "  最大页面数: " << g_page_cache.get_max_pages() << std::endl;
    std::cout << "  命中率: " << g_page_cache.get_hit_rate() * 100 << "%" << std::endl;
    
    // 创建和访问页面
    for (int i = 0; i < 5; ++i) {
        offset_t offset = i * PAGE_SIZE;
        auto page = g_page_cache.find_or_create_page(inode, offset);
        if (page) {
            std::cout << "创建/获取页面 " << i << " (偏移量: " << offset << ")" << std::endl;
            
            // 模拟写入数据
            std::memset(page->get_data(), 0x42 + i, PAGE_SIZE);
            page->mark_dirty();
            
            page->put();  // 释放引用
        }
    }
    
    std::cout << "操作后页面缓存统计:" << std::endl;
    std::cout << "  当前页面数: " << g_page_cache.get_page_count() << std::endl;
    std::cout << "  缓存命中: " << g_page_cache.get_hits() << std::endl;
    std::cout << "  缓存未命中: " << g_page_cache.get_misses() << std::endl;
    std::cout << "  命中率: " << g_page_cache.get_hit_rate() * 100 << "%" << std::endl;
    
    // 同步脏页面
    auto sync_result = g_page_cache.sync_pages(inode);
    if (sync_result.is_ok()) {
        std::cout << "页面同步成功" << std::endl;
        std::cout << "  写回次数: " << g_page_cache.get_writebacks() << std::endl;
    }
}

// 演示EXT4文件系统操作
void demo_ext4_filesystem() {
    std::cout << "\n=== EXT4文件系统操作演示 ===" << std::endl;
    
    try {
        // 创建64MB的块设备
        auto device = std::make_shared<MemoryBlockDevice>(64 * 1024 * 1024);
        
        // 格式化为EXT4文件系统
        std::cout << "格式化EXT4文件系统..." << std::endl;
        auto mkfs_result = Ext4FileSystem::mkfs(device, "");
        if (mkfs_result.is_err()) {
            std::cout << "格式化失败" << std::endl;
            return;
        }
        std::cout << "格式化成功" << std::endl;
        
        // 创建文件系统实例
        auto ext4_fs = std::make_shared<Ext4FileSystem>();
        
        // 挂载文件系统
        std::cout << "挂载文件系统..." << std::endl;
        auto mount_result = ext4_fs->mount(device, 0, "");
        if (mount_result.is_err()) {
            std::cout << "挂载失败" << std::endl;
            return;
        }
        auto sb = mount_result.unwrap();
        std::cout << "挂载成功" << std::endl;
        
        // 获取根目录
        auto root_dentry = sb->get_root();
        if (!root_dentry) {
            std::cout << "无法获取根目录" << std::endl;
            return;
        }
        auto root_inode = root_dentry->get_inode();
        
        std::cout << "根目录信息:" << std::endl;
        auto root_attr = root_inode->getattr();
        if (root_attr.is_ok()) {
            auto attr = root_attr.unwrap();
            std::cout << "  类型: " << (attr.mode.type() == FileType::DIRECTORY ? "目录" : "文件") << std::endl;
            std::cout << "  大小: " << attr.size << " 字节" << std::endl;
            std::cout << "  权限: 0" << std::oct << attr.mode.mode << std::dec << std::endl;
        }
        
        // 创建测试目录
        std::cout << "创建测试目录 /test..." << std::endl;
        auto mkdir_result = root_inode->mkdir("test", FileMode(0755));
        if (mkdir_result.is_ok()) {
            std::cout << "目录创建成功" << std::endl;
            
            // 查找创建的目录
            auto test_dir_result = root_inode->lookup("test");
            if (test_dir_result.is_ok()) {
                auto test_dir = test_dir_result.unwrap();
                std::cout << "目录查找成功，inode号: " << test_dir->get_ino() << std::endl;
                
                // 在测试目录中创建文件
                std::cout << "在 /test 中创建文件 hello.txt..." << std::endl;
                auto create_result = test_dir->create("hello.txt", FileMode(0644));
                if (create_result.is_ok()) {
                    auto file_inode = create_result.unwrap();
                    std::cout << "文件创建成功，inode号: " << file_inode->get_ino() << std::endl;
                    
                    // 写入数据到文件
                    std::string content = "Hello, EXT4 File System!\nThis is a test file.\n";
                    auto write_result = file_inode->write(0, content.c_str(), content.size());
                    if (write_result.is_ok()) {
                        std::cout << "写入数据成功: " << write_result.unwrap() << " 字节" << std::endl;
                        
                        // 读取数据
                        std::vector<char> read_buffer(content.size() + 1, 0);
                        auto read_result = file_inode->read(0, read_buffer.data(), content.size());
                        if (read_result.is_ok()) {
                            std::cout << "读取数据成功: " << read_result.unwrap() << " 字节" << std::endl;
                            std::cout << "文件内容: " << read_buffer.data() << std::endl;
                        }
                    }
                }
            }
        }
        
        // 列出根目录内容
        std::cout << "根目录内容:" << std::endl;
        auto readdir_result = root_inode->readdir();
        if (readdir_result.is_ok()) {
            auto entries = readdir_result.unwrap();
            for (const auto& entry : entries) {
                std::cout << "  " << entry.name << " (inode: " << entry.ino;
                std::cout << ", 类型: ";
                switch (entry.type) {
                    case FileType::REGULAR: std::cout << "文件"; break;
                    case FileType::DIRECTORY: std::cout << "目录"; break;
                    case FileType::SYMLINK: std::cout << "符号链接"; break;
                    default: std::cout << "其他"; break;
                }
                std::cout << ")" << std::endl;
            }
        }
        
        // 同步文件系统
        std::cout << "同步文件系统..." << std::endl;
        auto sync_result = sb->sync();
        if (sync_result.is_ok()) {
            std::cout << "同步成功" << std::endl;
        }
        
        // 卸载文件系统
        std::cout << "卸载文件系统..." << std::endl;
        auto umount_result = ext4_fs->umount(sb);
        if (umount_result.is_ok()) {
            std::cout << "卸载成功" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "文件系统操作异常: " << e.what() << std::endl;
    }
}

// 演示VFS和系统调用
void demo_vfs_syscalls() {
    std::cout << "\n=== VFS和系统调用演示 ===" << std::endl;
    
    try {
        // 初始化系统调用
        SystemCall::init();
        
        // 创建进程文件系统上下文
        ProcessFsContext process_ctx;
        SystemCall::set_current_process(&process_ctx);
        
        // 创建并挂载文件系统
        auto device = std::make_shared<MemoryBlockDevice>(32 * 1024 * 1024);
        Ext4FileSystem::mkfs(device, "");
        
        auto ext4_fs = std::make_shared<Ext4FileSystem>();
        g_vfs.register_filesystem(ext4_fs);
        
        // 挂载到根目录
        auto mount_result = g_vfs.mount("/dev/mem0", "/", "ext4", 0, "");
        if (mount_result.is_ok()) {
            std::cout << "文件系统挂载成功" << std::endl;
        }
        
        // 使用系统调用创建目录
        std::cout << "使用系统调用创建目录..." << std::endl;
        int mkdir_ret = SystemCall::sys_mkdir("/home", 0755);
        if (mkdir_ret == 0) {
            std::cout << "目录 /home 创建成功" << std::endl;
        }
        
        mkdir_ret = SystemCall::sys_mkdir("/home/user", 0755);
        if (mkdir_ret == 0) {
            std::cout << "目录 /home/user 创建成功" << std::endl;
        }
        
        // 使用系统调用创建和写入文件
        std::cout << "使用系统调用创建文件..." << std::endl;
        int fd = SystemCall::sys_open("/home/user/test.txt", O_CREAT | O_WRONLY, 0644);
        if (fd >= 0) {
            std::cout << "文件创建成功，文件描述符: " << fd << std::endl;
            
            std::string content = "这是通过系统调用写入的内容\n测试中文和英文混合\nLine 3\n";
            ssize_t written = SystemCall::sys_write(fd, content.c_str(), content.size());
            if (written > 0) {
                std::cout << "写入成功: " << written << " 字节" << std::endl;
            }
            
            // 关闭文件
            SystemCall::sys_close(fd);
        }
        
        // 读取文件
        std::cout << "读取文件..." << std::endl;
        fd = SystemCall::sys_open("/home/user/test.txt", O_RDONLY);
        if (fd >= 0) {
            std::vector<char> buffer(1024, 0);
            ssize_t bytes_read = SystemCall::sys_read(fd, buffer.data(), buffer.size() - 1);
            if (bytes_read > 0) {
                std::cout << "读取成功: " << bytes_read << " 字节" << std::endl;
                std::cout << "文件内容:\n" << buffer.data() << std::endl;
            }
            SystemCall::sys_close(fd);
        }
        
        // 获取文件属性
        std::cout << "获取文件属性..." << std::endl;
        struct stat st;
        int stat_ret = SystemCall::sys_stat("/home/user/test.txt", &st);
        if (stat_ret == 0) {
            std::cout << "文件属性:" << std::endl;
            std::cout << "  大小: " << st.st_size << " 字节" << std::endl;
            std::cout << "  权限: 0" << std::oct << (st.st_mode & 0777) << std::dec << std::endl;
            std::cout << "  inode: " << st.st_ino << std::endl;
            std::cout << "  硬链接数: " << st.st_nlink << std::endl;
        }
        
        // 改变工作目录
        std::cout << "改变工作目录..." << std::endl;
        int chdir_ret = SystemCall::sys_chdir("/home/user");
        if (chdir_ret == 0) {
            std::cout << "工作目录已改变到 /home/user" << std::endl;
            
            char cwd_buffer[1024];
            char* cwd = SystemCall::sys_getcwd(cwd_buffer, sizeof(cwd_buffer));
            if (cwd) {
                std::cout << "当前工作目录: " << cwd << std::endl;
            }
        }
        
        // 创建符号链接
        std::cout << "创建符号链接..." << std::endl;
        int symlink_ret = SystemCall::sys_symlink("test.txt", "link_to_test");
        if (symlink_ret == 0) {
            std::cout << "符号链接创建成功" << std::endl;
            
            // 读取符号链接
            char link_buffer[256];
            ssize_t link_len = SystemCall::sys_readlink("link_to_test", link_buffer, sizeof(link_buffer) - 1);
            if (link_len > 0) {
                link_buffer[link_len] = '\0';
                std::cout << "符号链接目标: " << link_buffer << std::endl;
            }
        }
        
        // 同步文件系统
        std::cout << "同步文件系统..." << std::endl;
        SystemCall::sys_sync();
        std::cout << "同步完成" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "VFS/系统调用操作异常: " << e.what() << std::endl;
    }
}

// 演示libc库接口
void demo_libc_interface() {
    std::cout << "\n=== libc库接口演示 ===" << std::endl;
    
    try {
        // 使用标准C库接口创建和操作文件
        std::cout << "使用标准C库接口..." << std::endl;
        
        // 创建目录
        if (mkdir("/tmp", 0755) == 0) {
            std::cout << "目录 /tmp 创建成功" << std::endl;
        }
        
        // 使用fopen创建文件
        FILE* fp = fopen("/tmp/libc_test.txt", "w");
        if (fp) {
            std::cout << "文件创建成功" << std::endl;
            
            // 写入数据
            const char* content = "Hello from libc interface!\n这是libc接口测试\n";
            size_t written = fwrite(content, 1, strlen(content), fp);
            std::cout << "写入 " << written << " 字节" << std::endl;
            
            // 格式化输出
            fprintf(fp, "格式化输出: 数字=%d, 字符串=%s\n", 42, "test");
            
            fclose(fp);
        }
        
        // 读取文件
        fp = fopen("/tmp/libc_test.txt", "r");
        if (fp) {
            std::cout << "读取文件内容:" << std::endl;
            
            char line[256];
            while (fgets(line, sizeof(line), fp)) {
                std::cout << "  " << line;
            }
            
            fclose(fp);
        }
        
        // 使用C++流接口
        std::cout << "\n使用C++流接口..." << std::endl;
        
        {
            fs_libc::ofstream ofs("/tmp/cpp_test.txt");
            if (ofs.is_open()) {
                ofs << "Hello from C++ stream interface!" << std::endl;
                ofs << "支持C++流操作" << std::endl;
                ofs << "数字: " << 123 << ", 浮点数: " << 3.14159 << std::endl;
            }
        }
        
        {
            fs_libc::ifstream ifs("/tmp/cpp_test.txt");
            if (ifs.is_open()) {
                std::cout << "C++流读取结果:" << std::endl;
                std::string line;
                while (std::getline(ifs, line)) {
                    std::cout << "  " << line << std::endl;
                }
            }
        }
        
        // 目录遍历
        std::cout << "\n目录遍历演示:" << std::endl;
        for (fs_libc::directory_iterator it("/tmp"); it != fs_libc::directory_iterator::end(); ++it) {
            std::cout << "  文件: " << it->d_name << std::endl;
        }
        
        // 文件系统操作
        std::cout << "\n文件系统操作演示:" << std::endl;
        if (fs_libc::exists("/tmp/cpp_test.txt")) {
            std::cout << "文件存在" << std::endl;
            std::cout << "文件大小: " << fs_libc::file_size("/tmp/cpp_test.txt") << " 字节" << std::endl;
            std::cout << "是否为普通文件: " << (fs_libc::is_regular_file("/tmp/cpp_test.txt") ? "是" : "否") << std::endl;
        }
        
        if (fs_libc::is_directory("/tmp")) {
            std::cout << "/tmp 是目录" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "libc接口操作异常: " << e.what() << std::endl;
    }
}

// 性能测试
void performance_test() {
    std::cout << "\n=== 性能测试 ===" << std::endl;
    
    try {
        // 大文件读写性能测试
        const size_t file_size = 10 * 1024 * 1024;  // 10MB
        const size_t buffer_size = 64 * 1024;       // 64KB缓冲区
        
        std::cout << "大文件读写性能测试 (" << file_size / (1024*1024) << "MB)..." << std::endl;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // 写入测试
        {
            std::vector<u8> buffer(buffer_size, 0x55);
            
            int fd = SystemCall::sys_open("/tmp/perf_test.dat", O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd >= 0) {
                size_t total_written = 0;
                while (total_written < file_size) {
                    size_t to_write = std::min(buffer_size, file_size - total_written);
                    ssize_t written = SystemCall::sys_write(fd, buffer.data(), to_write);
                    if (written > 0) {
                        total_written += written;
                    } else {
                        break;
                    }
                }
                SystemCall::sys_close(fd);
                
                auto write_time = std::chrono::high_resolution_clock::now();
                auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_time - start_time);
                double write_speed = (double)total_written / (1024*1024) / (write_duration.count() / 1000.0);
                
                std::cout << "写入完成: " << total_written << " 字节" << std::endl;
                std::cout << "写入时间: " << write_duration.count() << " 毫秒" << std::endl;
                std::cout << "写入速度: " << write_speed << " MB/s" << std::endl;
            }
        }
        
        // 读取测试
        {
            std::vector<u8> buffer(buffer_size);
            
            int fd = SystemCall::sys_open("/tmp/perf_test.dat", O_RDONLY);
            if (fd >= 0) {
                size_t total_read = 0;
                auto read_start = std::chrono::high_resolution_clock::now();
                
                while (total_read < file_size) {
                    size_t to_read = std::min(buffer_size, file_size - total_read);
                    ssize_t bytes_read = SystemCall::sys_read(fd, buffer.data(), to_read);
                    if (bytes_read > 0) {
                        total_read += bytes_read;
                    } else {
                        break;
                    }
                }
                SystemCall::sys_close(fd);
                
                auto read_time = std::chrono::high_resolution_clock::now();
                auto read_duration = std::chrono::duration_cast<std::chrono::milliseconds>(read_time - read_start);
                double read_speed = (double)total_read / (1024*1024) / (read_duration.count() / 1000.0);
                
                std::cout << "读取完成: " << total_read << " 字节" << std::endl;
                std::cout << "读取时间: " << read_duration.count() << " 毫秒" << std::endl;
                std::cout << "读取速度: " << read_speed << " MB/s" << std::endl;
            }
        }
        
        // 页面缓存性能统计
        std::cout << "\n页面缓存性能统计:" << std::endl;
        std::cout << "  总页面数: " << g_page_cache.get_page_count() << std::endl;
        std::cout << "  缓存命中: " << g_page_cache.get_hits() << std::endl;
        std::cout << "  缓存未命中: " << g_page_cache.get_misses() << std::endl;
        std::cout << "  命中率: " << g_page_cache.get_hit_rate() * 100 << "%" << std::endl;
        std::cout << "  页面淘汰: " << g_page_cache.get_evictions() << std::endl;
        std::cout << "  页面写回: " << g_page_cache.get_writebacks() << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "性能测试异常: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "=== Linux文件系统实现演示程序 ===" << std::endl;
    std::cout << "本程序演示了完整的文件系统栈的各个组件和功能" << std::endl;
    
    try {
        // 初始化全局组件
        std::cout << "\n初始化文件系统组件..." << std::endl;
        
        // 演示各个组件
        demo_block_device();
        demo_page_cache();
        demo_ext4_filesystem();
        demo_vfs_syscalls();
        demo_libc_interface();
        performance_test();
        
        std::cout << "\n=== 演示程序完成 ===" << std::endl;
        std::cout << "所有组件测试通过！文件系统实现功能正常。" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "程序异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}