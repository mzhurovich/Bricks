/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2015 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "api/key_entry/test.cc"
#include "api/matrix/test.cc"

#include "yoda.h"
#include "test_types.h"

#include "../../Bricks/dflags/dflags.h"
#include "../../Bricks/3party/gtest/gtest-main-with-dflags.h"
#include "../../Bricks/strings/printf.h"

using bricks::strings::Printf;

TEST(Yoda, CoverTest) {
  typedef yoda::API<yoda::KeyEntry<KeyValueEntry>, yoda::MatrixEntry<MatrixCell>, yoda::KeyEntry<StringKVEntry>>
      TestAPI;
  TestAPI api("YodaCoverTest");

  api.UnsafeStream().Emplace(new StringKVEntry());

  while (!api.CaughtUp()) {
    ;
  }

  api.Add(KeyValueEntry(1, 42.0));
  EXPECT_EQ(42.0, api.Get(1).value);
  api.Add(MatrixCell(42, "answer", 100));
  EXPECT_EQ(100, api.Get(42, "answer").value);
  api.Add(StringKVEntry("foo", "bar"));
  EXPECT_EQ("bar", api.Get("foo").foo);

  // Adding some more values.
  api.Add(KeyValueEntry(2, 31.5));
  api.Add(KeyValueEntry(3, 11.2));
  api.Add(MatrixCell(1, "test", 2));
  api.Add(MatrixCell(2, "test", 1));
  api.Add(MatrixCell(3, "test", 4));

  // Asynchronous call of user function.
  bool done = false;
  EXPECT_EQ("bar", api.Get("foo").foo);
  api.Call([&](TestAPI::T_CONTAINER_WRAPPER& cw) {
    const bool exists = cw.Get(1);
    EXPECT_TRUE(exists);
    yoda::EntryWrapper<KeyValueEntry> entry = cw.Get(1);
    EXPECT_EQ(42.0, entry().value);
    EXPECT_TRUE(cw.Get(42, "answer"));
    EXPECT_EQ(100, cw.Get(42, "answer")().value);
    EXPECT_TRUE(cw.Get("foo"));
    EXPECT_EQ("bar", cw.Get("foo")().foo);

    EXPECT_FALSE(cw.Get(-1));
    EXPECT_FALSE(cw.Get(41, "not an answer"));
    EXPECT_FALSE(cw.Get("bazinga"));

    // Accessing nonexistent entry throws an exception.
    ASSERT_THROW(cw.Get(1000)(), yoda::NonexistentEntryAccessed);

    double result = 0.0;
    for (int i = 1; i <= 3; ++i) {
      result += cw.Get(i)().value * static_cast<double>(cw.Get(i, "test")().value);
    }
    cw.Add(StringKVEntry("result", Printf("%.2f", result)));
    cw.Add(MatrixCell(123, "test", 11));
    cw.Add(KeyValueEntry(42, 1.23));

    done = true;
  });

  while (!done) {
    ;  // Spin lock;
  }

  EXPECT_EQ("160.30", api.Get("result").foo);
  EXPECT_EQ(11, api.Get(123, "test").value);
  EXPECT_EQ(1.23, api.Get(42).value);
}
