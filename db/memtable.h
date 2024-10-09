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
  friend class MemTableBackwardIterator;  // ����������ֱ�ӷ���
 /*
 ��������뵽һ�����⣬��skiplist�У��ǽ���������Ϊskiplist��Ƕ��һ���࣬���������ǲ�����Ԫ�ķ�ʽ����ô������������ַ�ʽ��
 1.��Ԫ���Ա��������Ľӿڣ�����������mentable��ʵ�ַ��䣬�Ǵ�������ά����ͬʱ���������ĵ�����Ҳ�������������ô����ֱ�ӱ����á�������Ԫ��������֮��������ԣ���Ƹ��Ӹ��ӡ�
 2.��Ƕ�������Ƚ������ڵ���������Ĺ�ϵ������ܣ����ҵ��������ܼ򵥵�ʱ��
 ����������˵���������߼����Ӳ��Һ�����Զ�����������Ԫ���������߼��򵥲��Һ��������أ�������Ƕ��
 */

  struct KeyComparator {
    const InternalKeyComparator comparator;  // ������ıȽϹ���
    explicit KeyComparator(const InternalKeyComparator& c) : comparator(c) {}
    int operator()(const char* a, const char* b) const;  // ����ʵ�־����߼�
  };

  typedef SkipList<const char*, KeyComparator> Table;  // �洢���ͣ��Ƚ�����

  ~MemTable();  // ��Ƶ�Ŀ����Ϊ��ȷ�� MemTable �Ķ���ֻ��ͨ������ Unref()
                // ������ɾ������ֹ�ⲿ����ֱ��ɾ������ȷ���ڴ����İ�ȫ�ԡ�

  KeyComparator comparator_;
  int refs_;     // ���ü���������memtable����������
  /*
  ������shared_ptr�����ж�������Ҫͬʱ����memtableʵ��ʱ
  ʹ�����ü������԰�ȫ�Ĺ���memtable��ͬʱҲ�������ظ�ɾ����ʵ���˿ɿ����ڴ����
   */
  Arena arena_;  // �ڴ��
  Table table_;  // ��ֵ��
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_MEMTABLE_H_
