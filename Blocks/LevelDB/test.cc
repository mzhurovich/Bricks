#include "leveldb.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/file/file.h"

#include "../../3rdparty/gtest/gtest-main-with-dflags.h"

DEFINE_string(leveldb_test_tmpdir, ".current", "Local path for the test to create temporary files in.");

using blocks::LevelDB::KeyNotFoundException;

TEST(LevelDB, SmokeTest) {
  const std::string db_path = bricks::FileSystem::JoinPath(FLAGS_leveldb_test_tmpdir, "test_db");
  const auto dir_remover = bricks::FileSystem::ScopedRmDir(db_path);
  blocks::LevelDB::LevelDB db(db_path);
  db.Set("1", "100");
  db.Set("2", "1000");
  EXPECT_EQ("100", db.Get("1"));
  EXPECT_EQ("1000", db.Get("2"));

  db.Set("2", "1001");
  EXPECT_EQ("1001", db.Get("2"));

  db.Delete("2");
  ASSERT_THROW(db.Get("2"), KeyNotFoundException);
}
