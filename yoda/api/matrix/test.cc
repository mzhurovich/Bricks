/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>
          (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#include "../../yoda.h"
#include "../../test_types.h"

#include "../../../../Bricks/3party/gtest/gtest-main.h"

TEST(YodaMatrixEntry, Smoke) {
  typedef yoda::API<YodaTestEntryBase, yoda::MatrixEntry<MatrixCell>> TestAPI;
  TestAPI api("YodaMatrixEntrySmokeTest");

  // Add the first key-value pair.
  // Use `UnsafeStream()`, since generally the only way to access the underlying stream is to make API calls.
  api.UnsafeStream().Emplace(new MatrixCell(5, "x", -1));

  while (!api.CaughtUp()) {
    // Spin lock, for the purposes of this test.
    // Ensure that the data has reached the the processor that maintains the in-memory state of the API.
  }

  EXPECT_EQ(-1, api.AsyncGet(5, "x").get().value);
  EXPECT_EQ(-1, api.Get(5, "x").value);

  // Callback version.
  struct CallbackTest {
    explicit CallbackTest(const size_t row,
                          const std::string& col,
                          const int value,
                          const bool expect_success = true)
        : row(row), col(col), value(value), expect_success(expect_success) {}

    void found(const MatrixCell& entry) const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_TRUE(expect_success);
      EXPECT_EQ(row, entry.row);
      EXPECT_EQ(col, entry.col);
      EXPECT_EQ(value, entry.value);
    }

    void not_found(const size_t row, const std::string& col) const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_FALSE(expect_success);
      EXPECT_EQ(this->row, row);
      EXPECT_EQ(this->col, col);
    }

    void added() const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_TRUE(expect_success);
    }

    void already_exists() const {
      ASSERT_FALSE(called);
      called = true;
      EXPECT_FALSE(expect_success);
    }

    const size_t row;
    const std::string col;
    const int value;
    const bool expect_success;
    mutable bool called = false;
  };

  const CallbackTest cbt1(5, "x", -1);
  api.AsyncGet(typename yoda::MatrixEntry<MatrixCell>::T_ROW(5),
               typename yoda::MatrixEntry<MatrixCell>::T_COL("x"),
               std::bind(&CallbackTest::found, &cbt1, std::placeholders::_1),
               std::bind(&CallbackTest::not_found, &cbt1, std::placeholders::_1, std::placeholders::_2));
  while (!cbt1.called) {
    ;  // Spin lock.
  }

  ASSERT_THROW(api.AsyncGet(5, "y").get(), typename yoda::MatrixEntry<MatrixCell>::T_CELL_NOT_FOUND_EXCEPTION);
  ASSERT_THROW(api.AsyncGet(5, "y").get(), yoda::CellNotFoundCoverException);
  ASSERT_THROW(api.Get(1, "x"), typename yoda::MatrixEntry<MatrixCell>::T_CELL_NOT_FOUND_EXCEPTION);
  ASSERT_THROW(api.Get(1, "x"), yoda::CellNotFoundCoverException);
  const CallbackTest cbt2(123, "no_entry", 0, false);
  api.AsyncGet(123,
               "no_entry",
               std::bind(&CallbackTest::found, &cbt2, std::placeholders::_1),
               std::bind(&CallbackTest::not_found, &cbt2, std::placeholders::_1, std::placeholders::_2));
  while (!cbt2.called) {
    ;  // Spin lock.
  }

  // Add three more key-value pairs, this time via the API.
  api.AsyncAdd(MatrixCell(5, "y", 15)).wait();
  api.Add(MatrixCell(1, "x", -9));
  const CallbackTest cbt3(42, "the_answer", 1);
  api.AsyncAdd(typename yoda::MatrixEntry<MatrixCell>::T_ENTRY(42, "the_answer", 1),
               std::bind(&CallbackTest::added, &cbt3),
               std::bind(&CallbackTest::already_exists, &cbt3));
  while (!cbt3.called) {
    ;  // Spin lock.
  }

  EXPECT_EQ(15, api.Get(5, "y").value);
  EXPECT_EQ(-9, api.Get(1, "x").value);
  EXPECT_EQ(1, api.Get(42, "the_answer").value);

  // Check that default policy doesn't allow overwriting on Add().
  ASSERT_THROW(api.AsyncAdd(MatrixCell(5, "y", 8)).get(),
               typename yoda::MatrixEntry<MatrixCell>::T_CELL_ALREADY_EXISTS_EXCEPTION);
  ASSERT_THROW(api.AsyncAdd(MatrixCell(5, "y", 100)).get(), yoda::CellAlreadyExistsCoverException);
  ASSERT_THROW(api.Add(MatrixCell(1, "x", 2)),
               typename yoda::MatrixEntry<MatrixCell>::T_CELL_ALREADY_EXISTS_EXCEPTION);
  ASSERT_THROW(api.Add(MatrixCell(1, "x", 2)), yoda::CellAlreadyExistsCoverException);
  const CallbackTest cbt4(42, "the_answer", 0, false);
  api.AsyncAdd(typename yoda::MatrixEntry<MatrixCell>::T_ENTRY(42, "the_answer", 0),
               std::bind(&CallbackTest::added, &cbt4),
               std::bind(&CallbackTest::already_exists, &cbt4));
  while (!cbt4.called) {
    ;  // Spin lock.
  }

  // Test user function accessing the underlying container.
  typedef yoda::ActualContainer<yoda::MatrixEntry<MatrixCell>> T_CONTAINER;
  size_t row_index_sum = 0;
  int value_sum = 0;
  bool done = false;
  api.AsyncCallFunction([&](const T_CONTAINER& container) {
    // Testing forward and transposed matrices.
    for (const auto rit : container.forward) {
      row_index_sum += rit.first;
    }
    for (const auto cit : container.transposed) {
      for (const auto rit : cit.second) {
        value_sum += rit.second.value;
      }
    }
    done = true;
  });
  while (!done) {
    ;  // Spin lock.
  }
  EXPECT_EQ(48u, row_index_sum);
  EXPECT_EQ(6, value_sum);
}
