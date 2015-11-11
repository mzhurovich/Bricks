/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*******************************************************************************/

#ifndef BLOCKS_LEVELDB_LEVELDB_H
#define BLOCKS_LEVELDB_LEVELDB_H

#include "exceptions.h"

#include "leveldb/db.h"
#include "../../../Bricks/template/pod.h"

namespace blocks {
namespace LevelDB {

class LevelDB {
 public:
  explicit LevelDB(const std::string& path) : path_(path) {
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::DB* db;
    leveldb::Status status = leveldb::DB::Open(options, path, &db);
    if(!status.ok()) {
      throw DBOpenFailedException();
    }
    db_.reset(db); 
  }

  std::string Get(const leveldb::Slice& key) {
    std::string value;
    leveldb::Status status = db_->Get(leveldb::ReadOptions(), key, &value);
    if (!status.ok()) {
      if (status.IsNotFound()) {
        throw KeyNotFoundException();
      } else {
        throw GetFailedException();
      }
    }
    return value;
  }

  void Set(const leveldb::Slice& key, const leveldb::Slice& value) {
    leveldb::Status status = db_->Put(leveldb::WriteOptions(), key, value);
    if (!status.ok()) {
      throw SetFailedException();
    }
  }

  void Delete(const leveldb::Slice& key) {
    leveldb::Status status = db_->Delete(leveldb::WriteOptions(), key);
    if (!status.ok()) {
      throw DeleteFailedException();
    }
  }

 private:
  std::string path_;
  std::unique_ptr<leveldb::DB> db_;
};

}  // namespace LevelDB
}  // namespace blocks

#endif  // BLOCKS_LEVELDB_LEVELDB_H
