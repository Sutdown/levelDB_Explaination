// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// A filter block is stored near the end of a Table file.  It contains
// filters (e.g., bloom filters) for all data blocks in the table combined
// into a single filter block.

#ifndef STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_
#define STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "leveldb/slice.h"
#include "util/hash.h"

/*
该文件定义了leveldb中的过滤器块的相关类，用于实现数据块的过滤器功能，比如布隆过滤器。
核心功能是通过构建和读取过滤器块来优化LevelDB的查找性能。
FilterBlockBuilder类负责创建过滤器，
FilterBlockReader类负责使用过滤器来判断某个键是否可能在特定数据块中。
通过有效利用过滤器，LevelDB能够减少不必要的磁盘读取，提高数据访问效率。
*/
namespace leveldb {

/*抽象基类，负责定义过滤器的策略。它并没有直接的实现*/
class FilterPolicy;

// A FilterBlockBuilder is used to construct all of the filters for a
// particular Table.  It generates a single string which is stored as
// a special block in the Table.
//
// The sequence of calls to FilterBlockBuilder must match the regexp:
//      (StartBlock AddKey*)* Finish
/*接受一个filterPolicy指针，指定过滤器策略*/
class FilterBlockBuilder {
 public:
  explicit FilterBlockBuilder(const FilterPolicy*);

  FilterBlockBuilder(const FilterBlockBuilder&) = delete;
  FilterBlockBuilder& operator=(const FilterBlockBuilder&) = delete;

  void StartBlock(uint64_t block_offset);
  void AddKey(const Slice& key);
  Slice Finish();

 private:
  void GenerateFilter();

  const FilterPolicy* policy_; // 过滤器策略。
  std::string keys_;             // 已展平的键内容。
  std::vector<size_t> start_;    // 每个键在 keys_ 中的起始索引。
  std::string result_;           // 当前计算的过滤器数据。
  std::vector<Slice> tmp_keys_;  // policy_->CreateFilter() argument 临时存储
  std::vector<uint32_t> filter_offsets_; // 计算过滤器的辅助数据。
};

/*接收 FilterPolicy 指针和过滤器数据的切片，初始化读取器*/
class FilterBlockReader {
 public:
  // REQUIRES: "contents" and *policy must stay live while *this is live.
  FilterBlockReader(const FilterPolicy* policy, const Slice& contents);
  /*
  检查给定的键是否可能匹配指定偏移量的数据块。
  如果返回 true，表示键可能存在于该块中，
  否则可以确定该块中不存在该键。
  */
  bool KeyMayMatch(uint64_t block_offset, const Slice& key);

 private:
  const FilterPolicy* policy_;
  const char* data_;    // Pointer to filter data (at block-start)
  const char* offset_;  // Pointer to beginning of offset array (at block-end)
  size_t num_;          // 偏移数组中的条目数。
  size_t base_lg_;      // 编码参数 (see kFilterBaseLg in .cc file)
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_
