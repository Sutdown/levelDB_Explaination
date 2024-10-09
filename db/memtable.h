// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_MEMTABLE_H_
#define STORAGE_LEVELDB_DB_MEMTABLE_H_

#include <string>

#include "db/dbformat.h"
#include "db/skiplist.h"
#include "leveldb/db.h"
#include "util/arena.h"

namespace leveldb {

class InternalKeyComparator;
class MemTableIterator;

class MemTable {
 public:
  // MemTables are reference counted.  The initial reference count
  // is zero and the caller must call Ref() at least once.
  explicit MemTable(const InternalKeyComparator& comparator);

  MemTable(const MemTable&) = delete;
  MemTable& operator=(const MemTable&) = delete;

  // Increase reference count.
  void Ref() { ++refs_; }

  // Drop reference count.  Delete if no more references exist.
  void Unref() {
    --refs_;
    assert(refs_ >= 0);
    if (refs_ <= 0) {
      delete this;
    }
  }

  // Returns an estimate of the number of bytes of data in use by this
  // data structure. It is safe to call when MemTable is being modified.
  size_t ApproximateMemoryUsage();

  // Return an iterator that yields the contents of the memtable.
  //
  // The caller must ensure that the underlying MemTable remains live
  // while the returned iterator is live.  The keys returned by this
  // iterator are internal keys encoded by AppendInternalKey in the
  // db/format.{h,cc} module.
  Iterator* NewIterator();

  // Add an entry into memtable that maps key to value at the
  // specified sequence number and with the specified type.
  // Typically value will be empty if type==kTypeDeletion.
  void Add(SequenceNumber seq, ValueType type, const Slice& key,
           const Slice& value);

  // If memtable contains a value for key, store it in *value and return true.
  // If memtable contains a deletion for key, store a NotFound() error
  // in *status and return true.
  // Else, return false.
  bool Get(const LookupKey& key, std::string* value, Status* s);

 private:
  friend class MemTableIterator;
  friend class MemTableBackwardIterator;  // 迭代器可以直接访问
 /*
 这里可以想到一个问题，在skiplist中，是将迭代器作为skiplist内嵌的一个类，但是这里是采用友元的方式，那么如何评价这两种方式？
 1.友元可以保持清晰的接口，将迭代器和mentable的实现分配，是代码易于维护，同时如果其它类的迭代器也可以用这个，那么可以直接被复用。但是友元会增加类之间的依赖性，设计更加复杂。
 2.内嵌迭代器比较适用于迭代器和类的关系极其紧密，并且迭代器功能简单的时候。
 所以整体来说，迭代器逻辑复杂并且和类相对独立，建议友元；迭代器逻辑简单并且和类紧密相关，建议内嵌。
 */

  struct KeyComparator {
    const InternalKeyComparator comparator;  // 定义键的比较规则
    explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) {}
    int operator()(const char* a, const char* b) const;  // 重载实现具体逻辑
  };

  typedef SkipList<const char*, KeyComparator> Table;  // 存储类型，比较类型

  ~MemTable();  // 设计的目的是为了确保 MemTable 的对象只能通过调用 Unref()
                // 方法来删除，防止外部代码直接删除对象，确保内存管理的安全性。

  KeyComparator comparator_;
  int refs_;     // 引用计数，管理memtable的生命周期
  /*
  类似于shared_ptr，当有多个组件需要同时访问memtable实例时
  使用引用计数可以安全的共享memtable，同时也避免了重复删除，实现了可靠的内存管理
   */
  Arena arena_;  // 内存池
  Table table_;  // 键值对
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_MEMTABLE_H_
