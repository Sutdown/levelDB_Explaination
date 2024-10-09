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
���ļ�������leveldb�еĹ������������࣬����ʵ�����ݿ�Ĺ��������ܣ����粼¡��������
���Ĺ�����ͨ�������Ͷ�ȡ�����������Ż�LevelDB�Ĳ������ܡ�
FilterBlockBuilder�ฺ�𴴽���������
FilterBlockReader�ฺ��ʹ�ù��������ж�ĳ�����Ƿ�������ض����ݿ��С�
ͨ����Ч���ù�������LevelDB�ܹ����ٲ���Ҫ�Ĵ��̶�ȡ��������ݷ���Ч�ʡ�
*/
namespace leveldb {

/*������࣬������������Ĳ��ԡ�����û��ֱ�ӵ�ʵ��*/
class FilterPolicy;

// A FilterBlockBuilder is used to construct all of the filters for a
// particular Table.  It generates a single string which is stored as
// a special block in the Table.
//
// The sequence of calls to FilterBlockBuilder must match the regexp:
//      (StartBlock AddKey*)* Finish
/*����һ��filterPolicyָ�룬ָ������������*/
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

  const FilterPolicy* policy_; // ���������ԡ�
  std::string keys_;             // ��չƽ�ļ����ݡ�
  std::vector<size_t> start_;    // ÿ������ keys_ �е���ʼ������
  std::string result_;           // ��ǰ����Ĺ��������ݡ�
  std::vector<Slice> tmp_keys_;  // policy_->CreateFilter() argument ��ʱ�洢
  std::vector<uint32_t> filter_offsets_; // ����������ĸ������ݡ�
};

/*���� FilterPolicy ָ��͹��������ݵ���Ƭ����ʼ����ȡ��*/
class FilterBlockReader {
 public:
  // REQUIRES: "contents" and *policy must stay live while *this is live.
  FilterBlockReader(const FilterPolicy* policy, const Slice& contents);
  /*
  �������ļ��Ƿ����ƥ��ָ��ƫ���������ݿ顣
  ������� true����ʾ�����ܴ����ڸÿ��У�
  �������ȷ���ÿ��в����ڸü���
  */
  bool KeyMayMatch(uint64_t block_offset, const Slice& key);

 private:
  const FilterPolicy* policy_;
  const char* data_;    // Pointer to filter data (at block-start)
  const char* offset_;  // Pointer to beginning of offset array (at block-end)
  size_t num_;          // ƫ�������е���Ŀ����
  size_t base_lg_;      // ������� (see kFilterBaseLg in .cc file)
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_FILTER_BLOCK_H_
