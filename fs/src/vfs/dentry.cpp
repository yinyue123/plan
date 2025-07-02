#include "../../include/vfs.h"

// Dentry实现
Dentry::Dentry(const std::string& name, SharedPtr<Inode> inode, SharedPtr<Dentry> parent)
    : name_(name), inode_(inode), parent_(parent), ref_count_(1) {
}

Dentry::~Dentry() {
}

SharedPtr<Dentry> Dentry::lookup_child(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto iter = children_.find(name);
    if (iter != children_.end()) {
        return iter->second;
    }
    return nullptr;
}

void Dentry::add_child(SharedPtr<Dentry> child) {
    std::lock_guard<std::mutex> lock(mutex_);
    children_[child->get_name()] = child;
}

void Dentry::remove_child(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    children_.erase(name);
}

std::string Dentry::get_path() const {
    if (auto parent = parent_.lock()) {
        return parent->get_path() + "/" + name_;
    }
    return name_;
}

std::vector<SharedPtr<Dentry>> Dentry::list_children() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<SharedPtr<Dentry>> result;
    for (const auto& pair : children_) {
        result.push_back(pair.second);
    }
    return result;
}