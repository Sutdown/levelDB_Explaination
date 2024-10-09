// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "table/filter_block.h"

#include "leveldb/filter_policy.h"
#include "util/coding.h"

namespace leveldb {

// See doc/table_format.md for an explanation of the filter block format.

// Generate new filter every 2KB of data
static const size_t kFilterBaseLg = 11;
static const size_t kFilterBase = 1 << kFilterBaseLg;

/*构造函数，一些需要初始化的私有成员*/
FilterBlockBuilder::FilterBlockBuilder(const FilterPolicy* policy)
    : policy_(policy) {}

/*
* 根据区块偏移量决定何时生成新的过滤器
*/
void FilterBlockBuilder::StartBlock(uint64_t block_offset) {
  uint64_t filter_index = (block_offset / kFilterBase);
  assert(filter_index >= filter_offsets_.size());
  while (filter_index > filter_offsets_.size()) {
    GenerateFilter();
  }
}

/*将键添加到当前过滤器*/
void FilterBlockBuilder::AddKey(const Slice& key) {
  Slice k = key;
  start_.push_back(keys_.size());
  keys_.append(k.data(), k.size());
}

/*完成当前过滤器构建*/
Slice FilterBlockBuilder::Finish() {
  if (!start_.empty()) {
    GenerateFilter();
  }

  // Append array of per-filter offsets
  const uint32_t array_offset = result_.size();
  for (size_t i = 0; i < filter_offsets_.size(); i++) {
    PutFixed32(&result_, filter_offsets_[i]);
  }

  PutFixed32(&result_, array_offset);
  result_.push_back(kFilterBaseLg);  // Save encoding parameter in result
  return Slice(result_);
}

/*
* 生成当前键集的过滤器，并将其追加到结果中。
* 在没有键的情况下，会快速路径返回当前过滤器的偏移。
*/
void FilterBlockBuilder::GenerateFilter() {
  const size_t num_keys = start_.size();
  if (num_keys == 0) {
    // Fast path if there are no keys for this filter
    filter_offsets_.push_back(result_.size());
    return;
  }

  // Make list of keys from flattened key structure
  start_.push_back(keys_.size());  // Simplify length computation
  tmp_keys_.resize(num_keys);
  for (size_t i = 0; i < num_keys; i++) {
    const char* base = keys_.data() + start_[i]; // 计算每个键在keys中的基地址
    size_t length = start_[i + 1] - start_[i]; // 长度
    tmp_keys_[i] = Slice(base, length);
  }

  // 从键中提取信息生成过滤器
  // Generate filter for current set of keys and append to result_.
  filter_offsets_.push_back(result_.size());
  policy_->CreateFilter(&tmp_keys_[0], static_cast<int>(num_keys), &result_);

  tmp_keys_.clear();
  keys_.clear();
  start_.clear();
}

/*
* 负责读取过滤块并检查某个键是否可能在特定区块中。
* 在构造函数中，它解析过滤块的内容并设置相应的成员变量。
*/
FilterBlockReader::FilterBlockReader(const FilterPolicy* policy, const Slice& contents) : policy_(policy), data_(nullptr), offset_(nullptr), num_(0), base_lg_(0) {
  size_t n = contents.size(); // 过滤内容
  if (n < 5) return;  // 1 byte for base_lg_ and 4 for start of offset array
  
  base_lg_ = contents[n - 1]; // 编码参数
  uint32_t last_word = DecodeFixed32(contents.data() + n - 5); // 偏移数组在原始数据中的起始位置
  if (last_word > n - 5) return;
  
  data_ = contents.data();
  offset_ = data_ + last_word; // 现指向偏移数组的位置
  //FilterBlockBuilder时，偏移量可能是动态生成并存储在某个结构中
  num_ = (n - 5 - last_word) / 4; // 偏移数组总字节数/4=过滤器的实际数量
}

/*KeyMayMatch 方法通过查找特定区块的过滤器，来判断某个键是否可能匹配。*/
/*数据块偏移量，要检查的键*/
bool FilterBlockReader::KeyMayMatch(uint64_t block_offset, const Slice& key) {
  uint64_t index = block_offset >> base_lg_;
  if (index < num_) {
    // 当前过滤器的起始和结束位置
    uint32_t start = DecodeFixed32(offset_ + index * 4);
    uint32_t limit = DecodeFixed32(offset_ + index * 4 + 4);
    if (start <= limit && limit <= static_cast<size_t>(offset_ - data_)) {
      // start到limit是实际的过滤器内容
      Slice filter = Slice(data_ + start, limit - start);
      return policy_->KeyMayMatch(key, filter);
    } else if (start == limit) {
      // Empty filters do not match any keys
      return false;
    }
  }
  return true;  // Errors are treated as potential matches 可能存在
}

}  // namespace leveldb
