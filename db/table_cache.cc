// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/table_cache.h"

#include "db/filename.h"
#include "leveldb/env.h"
#include "leveldb/table.h"
#include "util/coding.h"

namespace leveldb {

struct TableAndFile {
  RandomAccessFile* file; // 将数据以sstable的格式持久化存储到磁盘上。
  Table* table; // 作为一个数据结构，封装了与sstable相关的逻辑，比如读取，查找，迭代等
};

static void DeleteEntry(const Slice& key, void* value) {
  TableAndFile* tf = reinterpret_cast<TableAndFile*>(value);
  delete tf->table;
  delete tf->file;
  delete tf;
}

static void UnrefEntry(void* arg1, void* arg2) {
  Cache* cache = reinterpret_cast<Cache*>(arg1);
  Cache::Handle* h = reinterpret_cast<Cache::Handle*>(arg2);
  cache->Release(h);
}

TableCache::TableCache(const std::string& dbname, const Options& options,
                       int entries)
    : env_(options.env),
      dbname_(dbname),
      options_(options),
      cache_(NewLRUCache(entries)) {}

TableCache::~TableCache() { 
    delete cache_; 
}

/*
* 实现了查找sstable的功能
* 缓存中查找如果失败，
* 会动态打开文件创建表对象，同时插入缓存
* 最终返回状态
*/
Status TableCache::FindTable(uint64_t file_number, uint64_t file_size,
                             Cache::Handle** handle) {
  // 初始化
  Status s;
  char buf[sizeof(file_number)];
  EncodeFixed64(buf, file_number);
  Slice key(buf, sizeof(buf));

  // 查找
  *handle = cache_->Lookup(key);
  if (*handle == nullptr) { // 检查缓存结果，从文件中查找
    std::string fname = TableFileName(dbname_, file_number); // 构造文件名
    RandomAccessFile* file = nullptr; // 初始化指针
    Table* table = nullptr;

    s = env_->NewRandomAccessFile(fname, &file);
    if (!s.ok()) { // 打开文件失败的错误处理
      std::string old_fname = SSTTableFileName(dbname_, file_number);
      if (env_->NewRandomAccessFile(old_fname, &file).ok()) {
        s = Status::OK();
      }
    }
    if (s.ok()) {
      s = Table::Open(options_, file, file_size, &table);
    }

    if (!s.ok()) { // 错误处理
      assert(table == nullptr);
      delete file;
      // We do not cache error results so that if the error is transient,
      // or somebody repairs the file, we recover automatically.
    } else { // 创建新对象插入缓存
      TableAndFile* tf = new TableAndFile;
      tf->file = file;
      tf->table = table;
      *handle = cache_->Insert(key, tf, 1, &DeleteEntry);
    }
  }
  return s;
}

/*
* 创建一个新的迭代器，遍历指定的sstable
* 通过查找缓存获取表对象
* 创建成功后会注册清理操作管理内存，确保迭代器被销毁时释放资源。
*/
Iterator* TableCache::NewIterator(const ReadOptions& options,
                                  uint64_t file_number, uint64_t file_size,
                                  Table** tableptr) {
  if (tableptr != nullptr) {
    *tableptr = nullptr;
  }
  // 查找指定的sstable
  Cache::Handle* handle = nullptr;
  Status s = FindTable(file_number, file_size, &handle);
  if (!s.ok()) {
    return NewErrorIterator(s);
  }

  // 从缓存句柄中获取tableandfile对象，通过类型转换获取内部的table指针
  Table* table = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
  // 获取迭代器
  Iterator* result = table->NewIterator(options);
  // 清理注册函数
  result->RegisterCleanup(&UnrefEntry, cache_, handle);
  if (tableptr != nullptr) {
    *tableptr = table;
  }
  return result;
}

/*从缓存中获取指定键的值*/
/*
* TableCache 的 Get 方法能够同时查询这两层缓存。
* 首先，它会查找第一层缓存（元数据），获取所需的 SSTable 信息。
* 如果该信息存在，接着会使用这些信息查询第二层缓存（BlockCache），
* 获取实际的数据块。
*/
Status TableCache::Get(const ReadOptions& options, uint64_t file_number,
                       uint64_t file_size, const Slice& k, void* arg,
                       void (*handle_result)(void*, const Slice&,
                                             const Slice&)) {
  Cache::Handle* handle = nullptr;
  Status s = FindTable(file_number, file_size, &handle);
  if (s.ok()) {
    // 查找成功时，从缓存句柄中获取TableAndFile对象，通过类型转换获取内部的table指针
    Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
    s = t->InternalGet(options, k, arg, handle_result); // 获取键值对，同时调用回调函数
    cache_->Release(handle); // 释放缓存句柄，减少引用计数
  }
  return s;
}

/*驱逐指定的sstable*/
void TableCache::Evict(uint64_t file_number) {
  // 创建字符数组，为文件编号编码
  char buf[sizeof(file_number)];
  EncodeFixed64(buf, file_number);
  // 从缓存中删除
  cache_->Erase(Slice(buf, sizeof(buf)));
}

}  // namespace leveldb
