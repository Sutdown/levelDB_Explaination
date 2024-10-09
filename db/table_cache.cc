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
  RandomAccessFile* file; // ��������sstable�ĸ�ʽ�־û��洢�������ϡ�
  Table* table; // ��Ϊһ�����ݽṹ����װ����sstable��ص��߼��������ȡ�����ң�������
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
* ʵ���˲���sstable�Ĺ���
* �����в������ʧ�ܣ�
* �ᶯ̬���ļ����������ͬʱ���뻺��
* ���շ���״̬
*/
Status TableCache::FindTable(uint64_t file_number, uint64_t file_size,
                             Cache::Handle** handle) {
  // ��ʼ��
  Status s;
  char buf[sizeof(file_number)];
  EncodeFixed64(buf, file_number);
  Slice key(buf, sizeof(buf));

  // ����
  *handle = cache_->Lookup(key);
  if (*handle == nullptr) { // ��黺���������ļ��в���
    std::string fname = TableFileName(dbname_, file_number); // �����ļ���
    RandomAccessFile* file = nullptr; // ��ʼ��ָ��
    Table* table = nullptr;

    s = env_->NewRandomAccessFile(fname, &file);
    if (!s.ok()) { // ���ļ�ʧ�ܵĴ�����
      std::string old_fname = SSTTableFileName(dbname_, file_number);
      if (env_->NewRandomAccessFile(old_fname, &file).ok()) {
        s = Status::OK();
      }
    }
    if (s.ok()) {
      s = Table::Open(options_, file, file_size, &table);
    }

    if (!s.ok()) { // ������
      assert(table == nullptr);
      delete file;
      // We do not cache error results so that if the error is transient,
      // or somebody repairs the file, we recover automatically.
    } else { // �����¶�����뻺��
      TableAndFile* tf = new TableAndFile;
      tf->file = file;
      tf->table = table;
      *handle = cache_->Insert(key, tf, 1, &DeleteEntry);
    }
  }
  return s;
}

/*
* ����һ���µĵ�����������ָ����sstable
* ͨ�����һ����ȡ�����
* �����ɹ����ע��������������ڴ棬ȷ��������������ʱ�ͷ���Դ��
*/
Iterator* TableCache::NewIterator(const ReadOptions& options,
                                  uint64_t file_number, uint64_t file_size,
                                  Table** tableptr) {
  if (tableptr != nullptr) {
    *tableptr = nullptr;
  }
  // ����ָ����sstable
  Cache::Handle* handle = nullptr;
  Status s = FindTable(file_number, file_size, &handle);
  if (!s.ok()) {
    return NewErrorIterator(s);
  }

  // �ӻ������л�ȡtableandfile����ͨ������ת����ȡ�ڲ���tableָ��
  Table* table = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
  // ��ȡ������
  Iterator* result = table->NewIterator(options);
  // ����ע�ắ��
  result->RegisterCleanup(&UnrefEntry, cache_, handle);
  if (tableptr != nullptr) {
    *tableptr = table;
  }
  return result;
}

/*�ӻ����л�ȡָ������ֵ*/
/*
* TableCache �� Get �����ܹ�ͬʱ��ѯ�����㻺�档
* ���ȣ�������ҵ�һ�㻺�棨Ԫ���ݣ�����ȡ����� SSTable ��Ϣ��
* �������Ϣ���ڣ����Ż�ʹ����Щ��Ϣ��ѯ�ڶ��㻺�棨BlockCache����
* ��ȡʵ�ʵ����ݿ顣
*/
Status TableCache::Get(const ReadOptions& options, uint64_t file_number,
                       uint64_t file_size, const Slice& k, void* arg,
                       void (*handle_result)(void*, const Slice&,
                                             const Slice&)) {
  Cache::Handle* handle = nullptr;
  Status s = FindTable(file_number, file_size, &handle);
  if (s.ok()) {
    // ���ҳɹ�ʱ���ӻ������л�ȡTableAndFile����ͨ������ת����ȡ�ڲ���tableָ��
    Table* t = reinterpret_cast<TableAndFile*>(cache_->Value(handle))->table;
    s = t->InternalGet(options, k, arg, handle_result); // ��ȡ��ֵ�ԣ�ͬʱ���ûص�����
    cache_->Release(handle); // �ͷŻ��������������ü���
  }
  return s;
}

/*����ָ����sstable*/
void TableCache::Evict(uint64_t file_number) {
  // �����ַ����飬Ϊ�ļ���ű���
  char buf[sizeof(file_number)];
  EncodeFixed64(buf, file_number);
  // �ӻ�����ɾ��
  cache_->Erase(Slice(buf, sizeof(buf)));
}

}  // namespace leveldb
