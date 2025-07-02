# Linux文件系统实现项目Makefile
# 使用C++17标准，包含所有必要的组件

# 编译器设置
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -g
INCLUDES = -Iinclude
LDFLAGS = -lpthread

# 目录设置
SRC_DIR = src
INCLUDE_DIR = include
BUILD_DIR = build
EXAMPLES_DIR = examples
TESTS_DIR = tests

# 源文件
BLOCK_SOURCES = $(SRC_DIR)/block/block_device.cpp
CACHE_SOURCES = $(SRC_DIR)/cache/page_cache.cpp
VFS_SOURCES = $(SRC_DIR)/vfs/inode.cpp $(SRC_DIR)/vfs/dentry.cpp $(SRC_DIR)/vfs/super_block.cpp $(SRC_DIR)/vfs/file.cpp $(SRC_DIR)/vfs/vfs.cpp
EXT4_SOURCES = $(SRC_DIR)/ext4/ext4.cpp $(SRC_DIR)/ext4/ext4_inode.cpp $(SRC_DIR)/ext4/ext4_dir.cpp
SYSCALL_SOURCES = $(SRC_DIR)/syscall/syscall.cpp $(SRC_DIR)/syscall/fd_table.cpp
LIBC_SOURCES = $(SRC_DIR)/libc/libc.cpp $(SRC_DIR)/libc/stdio.cpp $(SRC_DIR)/libc/cpp_streams.cpp
SWAP_SOURCES = $(SRC_DIR)/swap/swap.cpp

ALL_SOURCES = $(BLOCK_SOURCES) $(CACHE_SOURCES) $(VFS_SOURCES) $(EXT4_SOURCES) $(SYSCALL_SOURCES) $(LIBC_SOURCES) $(SWAP_SOURCES)

# 对象文件
OBJECTS = $(ALL_SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

# 目标
LIBRARY = $(BUILD_DIR)/libfs.a
DEMO = $(BUILD_DIR)/filesystem_demo
TEST = $(BUILD_DIR)/basic_test

# 默认目标
.PHONY: all clean demo test library

all: library demo test

# 创建构建目录
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/block
	mkdir -p $(BUILD_DIR)/cache
	mkdir -p $(BUILD_DIR)/vfs
	mkdir -p $(BUILD_DIR)/ext4
	mkdir -p $(BUILD_DIR)/syscall
	mkdir -p $(BUILD_DIR)/libc
	mkdir -p $(BUILD_DIR)/swap

# 编译对象文件
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# 创建静态库
$(LIBRARY): $(OBJECTS)
	ar rcs $@ $^
	ranlib $@

# 编译演示程序
$(DEMO): $(EXAMPLES_DIR)/filesystem_demo.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -lfs $(LDFLAGS) -o $@

# 编译测试程序
$(TEST): $(TESTS_DIR)/basic_test.cpp $(LIBRARY)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< -L$(BUILD_DIR) -lfs $(LDFLAGS) -o $@

# 只编译库
library: $(LIBRARY)

# 只编译演示程序
demo: $(DEMO)

# 只编译测试程序
test: $(TEST)

# 运行演示程序
run-demo: $(DEMO)
	./$(DEMO)

# 运行测试
run-test: $(TEST)
	./$(TEST)

# 清理构建文件
clean:
	rm -rf $(BUILD_DIR)

# 安装头文件和库(可选)
install: $(LIBRARY)
	mkdir -p /usr/local/include/fs
	mkdir -p /usr/local/lib
	cp -r $(INCLUDE_DIR)/* /usr/local/include/fs/
	cp $(LIBRARY) /usr/local/lib/
	ldconfig

# 卸载
uninstall:
	rm -rf /usr/local/include/fs
	rm -f /usr/local/lib/libfs.a
	ldconfig

# 生成文档(需要doxygen)
docs:
	doxygen Doxyfile

# 代码格式化(需要clang-format)
format:
	find $(SRC_DIR) $(INCLUDE_DIR) $(EXAMPLES_DIR) $(TESTS_DIR) -name "*.cpp" -o -name "*.h" | xargs clang-format -i

# 静态分析(需要cppcheck)
check:
	cppcheck --enable=all --std=c++17 $(INCLUDES) $(SRC_DIR) $(INCLUDE_DIR)

# 内存检查(需要valgrind)
memcheck: $(TEST)
	valgrind --leak-check=full --show-leak-kinds=all ./$(TEST)

# 性能分析(需要perf)
profile: $(DEMO)
	perf record -g ./$(DEMO)
	perf report

# 显示帮助信息
help:
	@echo "Linux文件系统实现项目构建帮助"
	@echo ""
	@echo "主要目标:"
	@echo "  all        - 编译所有组件(库、演示程序、测试)"
	@echo "  library    - 编译静态库"
	@echo "  demo       - 编译演示程序"
	@echo "  test       - 编译测试程序"
	@echo "  clean      - 清理构建文件"
	@echo ""
	@echo "运行目标:"
	@echo "  run-demo   - 运行演示程序"
	@echo "  run-test   - 运行测试程序"
	@echo ""
	@echo "开发工具:"
	@echo "  format     - 格式化代码(需要clang-format)"
	@echo "  check      - 静态代码分析(需要cppcheck)"
	@echo "  memcheck   - 内存泄漏检查(需要valgrind)"
	@echo "  profile    - 性能分析(需要perf)"
	@echo "  docs       - 生成文档(需要doxygen)"
	@echo ""
	@echo "安装:"
	@echo "  install    - 安装库和头文件到系统"
	@echo "  uninstall  - 从系统卸载"

# 依赖关系
$(BUILD_DIR)/block/block_device.o: $(INCLUDE_DIR)/types.h $(INCLUDE_DIR)/block_device.h
$(BUILD_DIR)/cache/page_cache.o: $(INCLUDE_DIR)/types.h $(INCLUDE_DIR)/page_cache.h $(INCLUDE_DIR)/vfs.h
$(BUILD_DIR)/vfs/inode.o: $(INCLUDE_DIR)/types.h $(INCLUDE_DIR)/vfs.h $(INCLUDE_DIR)/page_cache.h
$(BUILD_DIR)/ext4/ext4.o: $(INCLUDE_DIR)/types.h $(INCLUDE_DIR)/ext4.h $(INCLUDE_DIR)/vfs.h
$(BUILD_DIR)/syscall/syscall.o: $(INCLUDE_DIR)/types.h $(INCLUDE_DIR)/syscall.h $(INCLUDE_DIR)/vfs.h
$(BUILD_DIR)/libc/libc.o: $(INCLUDE_DIR)/types.h $(INCLUDE_DIR)/libc.h $(INCLUDE_DIR)/syscall.h

.SUFFIXES: .cpp .o