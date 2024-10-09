// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Thread-safe (provides internal synchronization)

#ifndef STORAGE_LEVELDB_DB_TABLE_CACHE_H_
#define STORAGE_LEVELDB_DB_TABLE_CACHE_H_

#include <cstdint>
#include <string>

#include "db/dbformat.h"
#include "leveldb/cache.h"
#include "leveldb/table.h"
#include "port/port.h"

namespace leveldb {

class Env;

class TableCache {
 public:
  // dbname，配置选项，最大条目
  TableCache(const std::string& dbname, const Options& options, int entries);

  // 避免多处引用统一资源造成的内存管理问题
  TableCache(const TableCache&) = delete;
  TableCache& operator=(const TableCache&) = delete;

  // 析构函数
  ~TableCache();

  // Return an iterator for the specified file number (the corresponding
  // file length must be exactly "file_size" bytes).  If "tableptr" is
  // non-null, also sets "*tableptr" to point to the Table object
  // underlying the returned iterator, or to nullptr if no Table object
  // underlies the returned iterator.  The returned "*tableptr" object is owned
  // by the cache and should not be deleted, and is valid for as long as the
  // returned iterator is live.
  // 为指定的文件号创建一个迭代器
  Iterator* NewIterator(const ReadOptions& options, uint64_t file_number,
                        uint64_t file_size, Table** tableptr = nullptr);

  // If a seek to internal key "k" in specified file finds an entry,
  // call (*handle_result)(arg, found_key, found_value).
  // 查找键值对，回调函数
  Status Get(const ReadOptions& options, uint64_t file_number,
             uint64_t file_size, const Slice& k, void* arg,
             void (*handle_result)(void*, const Slice&, const Slice&));

  // 驱逐指定文件编号的条目
  // Evict any entry for the specified file number
  void Evict(uint64_t file_number);

 private:
  // 通过filename找到sstable文件，filesize可以用于验证或者缓存的查找，handle**用于返回找到的缓存项
  Status FindTable(uint64_t file_number, uint64_t file_size, Cache::Handle**);

  Env* const env_; // 指向环境对象的指针
  const std::string dbname_; // 存储数据库的名称或路径
  const Options& options_; // 对于leveldb的配置选项的引用
  Cache* cache_; // 指向缓存对象的指针
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_TABLE_CACHE_H_
