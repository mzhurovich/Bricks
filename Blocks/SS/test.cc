/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>
          (c) 2016 Maxim Zhurovich <zhurovich@gmail.com>

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

#include "../../port.h"

#include "ss.h"

#include <string>

#include "../../Bricks/strings/printf.h"
#include "../../3rdparty/gtest/gtest-main.h"

using current::strings::Printf;

// A copy-able, move-able and clone-able entry.
struct DispatchDemoEntry {
  std::string text;
  DispatchDemoEntry() : text("Entry()") {}
  DispatchDemoEntry(const DispatchDemoEntry& rhs) : text("Entry(copy of {" + rhs.text + "})") {}
  DispatchDemoEntry(const std::string& text) : text("Entry('" + text + "')") {}
  DispatchDemoEntry(DispatchDemoEntry&& rhs) : text("Entry(move of {" + rhs.text + "})") {
    rhs.text = "moved away {" + rhs.text + "}";
  }

  struct Clone {};
  DispatchDemoEntry(Clone, const DispatchDemoEntry& rhs) : text("Entry(clone of {" + rhs.text + "})") {}

  DispatchDemoEntry& operator=(const DispatchDemoEntry&) = delete;
  DispatchDemoEntry& operator=(DispatchDemoEntry&&) = delete;
};

TEST(StreamSystem, TestHelperClassSmokeTest) {
  using DDE = DispatchDemoEntry;

  DDE e1;
  EXPECT_EQ("Entry()", e1.text);

  DDE e2("E2");
  EXPECT_EQ("Entry('E2')", e2.text);

  DDE e3(e2);
  EXPECT_EQ("Entry(copy of {Entry('E2')})", e3.text);
  EXPECT_EQ("Entry('E2')", e2.text);

  DDE e4(std::move(e2));
  EXPECT_EQ("Entry(move of {Entry('E2')})", e4.text);
  EXPECT_EQ("moved away {Entry('E2')}", e2.text);

  DDE e5(DDE::Clone(), e1);
  EXPECT_EQ("Entry()", e1.text);
  EXPECT_EQ("Entry(clone of {Entry()})", e5.text);
}

// Demonstrates that the entry is only copied/cloned if the dispatcher needs a copy.
// This test also confirms the dispatcher supports lambdas.
// The rest of the functionality -- treating `void` as `bool` and enabling omitting extra parameters --
// is tested in a giant write-only test below.
TEST(StreamSystem, SmokeTest) {
  using DDE = DispatchDemoEntry;
  using IDX_TS = current::ss::IndexAndTimestamp;
  using us = std::chrono::microseconds;

  struct CustomCloner {
    static DDE Clone(const DDE& e) { return DDE(DDE::Clone(), e); }
  };

  std::string text;

  const auto does_not_need_a_copy_lambda = [&text](const DDE& e, IDX_TS current, IDX_TS last) {
    text = Printf("%s, %llu, %llu", e.text.c_str(), current.index, last.index);
    return false;
  };

  const auto needs_a_copy_lambda = [&text](DDE&& e, IDX_TS current, IDX_TS last) {
    text = Printf("%s, %llu, %llu", e.text.c_str(), current.index, last.index);
    e.text = "invalidated <" + e.text + ">";
    return false;
  };

  // The two parameters after `entry` -- `current` and `last` -- do not carry any value for this test,
  // we just confirm they are passed through.
  DDE e;

  ASSERT_FALSE(current::ss::DispatchEntryByConstReference(
      does_not_need_a_copy_lambda, e, IDX_TS(1, us(100)), IDX_TS(4, us(400))));
  EXPECT_EQ("Entry(), 1, 4", text);

  ASSERT_FALSE(current::ss::DispatchEntryByConstReference<CustomCloner>(
      does_not_need_a_copy_lambda, e, IDX_TS(2, us(200)), IDX_TS(3, us(300))));
  EXPECT_EQ("Entry(), 2, 3", text);

  ASSERT_FALSE(current::ss::DispatchEntryByConstReference(
      needs_a_copy_lambda, e, IDX_TS(1, us(100)), IDX_TS(2, us(200))));
  EXPECT_EQ("Entry(copy of {Entry()}), 1, 2", text);

  ASSERT_FALSE(current::ss::DispatchEntryByConstReference<CustomCloner>(
      needs_a_copy_lambda, e, IDX_TS(3, us(300)), IDX_TS(4, us(400))));
  EXPECT_EQ("Entry(clone of {Entry()}), 3, 4", text);

  EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

  ASSERT_FALSE(current::ss::DispatchEntryByRValue(
      does_not_need_a_copy_lambda, e, IDX_TS(1, us(100)), IDX_TS(3, us(300))));
  EXPECT_EQ("Entry(), 1, 3", text);

  EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

  ASSERT_FALSE(current::ss::DispatchEntryByRValue(
      needs_a_copy_lambda, std::move(e), IDX_TS(2, us(200)), IDX_TS(4, us(400))));
  EXPECT_EQ("Entry(), 2, 4", text);

  EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
}

// This following test is write-only code. It's `SmokeTest` copied over multiple times with minor changes.
// Just make sure it passes. You really don't have to go through it in detail.
// Okay, if you have to -- sorry! -- it introduces changes to `SmokeTest` across two dimensions:
// 1) Return type: `bool` -> `void`.
// 2) Number of arguments: 3 -> 2 -> 1.
// You've been warned. Thanks!
TEST(StreamSystem, WriteOnlyTestTheRemainingCasesOutOfThoseTwelve) {
  using DDE = DispatchDemoEntry;
  using IDX_TS = current::ss::IndexAndTimestamp;
  using us = std::chrono::microseconds;

  struct CustomCloner {
    static DDE Clone(const DDE& e) { return DDE(DDE::Clone(), e); }
  };

  // The copy-paste of `SmokeTest`, as the boilerplate.
  {
    struct DoesNotNeedACopy {
      std::string text;
      bool operator()(const DDE& e, IDX_TS current, IDX_TS last) {
        text += Printf("%s, %llu, %llu", e.text.c_str(), current.index, last.index);
        return false;
      }
    };

    struct NeedsACopy {
      std::string text;
      bool operator()(DDE&& e, IDX_TS current, IDX_TS last) {
        text += Printf("%s, %llu, %llu", e.text.c_str(), current.index, last.index);
        e.text = "invalidated <" + e.text + ">";
        return false;
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_FALSE(current::ss::DispatchEntryByConstReference(
        does_not_need_a_copy_1, e, IDX_TS(1, us(100)), IDX_TS(4, us(400))));
    EXPECT_EQ("Entry(), 1, 4", does_not_need_a_copy_1.text);

    ASSERT_FALSE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        does_not_need_a_copy_2, e, IDX_TS(2, us(200)), IDX_TS(3, us(300))));
    EXPECT_EQ("Entry(), 2, 3", does_not_need_a_copy_2.text);

    ASSERT_FALSE(
        current::ss::DispatchEntryByConstReference(needs_a_copy_1, e, IDX_TS(1, us(100)), IDX_TS(2, us(200))));
    EXPECT_EQ("Entry(copy of {Entry()}), 1, 2", needs_a_copy_1.text);

    ASSERT_FALSE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        needs_a_copy_2, e, IDX_TS(3, us(300)), IDX_TS(4, us(400))));
    EXPECT_EQ("Entry(clone of {Entry()}), 3, 4", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(
        current::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, IDX_TS(1, us(100)), IDX_TS(3, us(300))));
    EXPECT_EQ("Entry(), 1, 3", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(current::ss::DispatchEntryByRValue(
        needs_a_copy_3, std::move(e), IDX_TS(2, us(200)), IDX_TS(4, us(400))));
    EXPECT_EQ("Entry(), 2, 4", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }

  // Confirm that `void` return type is treated as `bool true`.
  {
    struct DoesNotNeedACopy {
      std::string text;
      void operator()(const DDE& e, IDX_TS current, IDX_TS last) {
        text += Printf("%s, %llu, %llu", e.text.c_str(), current.index, last.index);
      }
    };

    struct NeedsACopy {
      std::string text;
      void operator()(DDE&& e, IDX_TS current, IDX_TS last) {
        text += Printf("%s, %llu, %llu", e.text.c_str(), current.index, last.index);
        e.text = "invalidated <" + e.text + ">";
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_TRUE(current::ss::DispatchEntryByConstReference(
        does_not_need_a_copy_1, e, IDX_TS(1, us(100)), IDX_TS(4, us(400))));
    EXPECT_EQ("Entry(), 1, 4", does_not_need_a_copy_1.text);

    ASSERT_TRUE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        does_not_need_a_copy_2, e, IDX_TS(2, us(200)), IDX_TS(3, us(300))));
    EXPECT_EQ("Entry(), 2, 3", does_not_need_a_copy_2.text);

    ASSERT_TRUE(
        current::ss::DispatchEntryByConstReference(needs_a_copy_1, e, IDX_TS(1, us(100)), IDX_TS(2, us(200))));
    EXPECT_EQ("Entry(copy of {Entry()}), 1, 2", needs_a_copy_1.text);

    ASSERT_TRUE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        needs_a_copy_2, e, IDX_TS(3, us(300)), IDX_TS(4, us(400))));
    EXPECT_EQ("Entry(clone of {Entry()}), 3, 4", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(
        current::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, IDX_TS(1, us(100)), IDX_TS(3, us(300))));
    EXPECT_EQ("Entry(), 1, 3", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(current::ss::DispatchEntryByRValue(
        needs_a_copy_3, std::move(e), IDX_TS(2, us(200)), IDX_TS(4, us(400))));
    EXPECT_EQ("Entry(), 2, 4", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }

  // The copy-paste of `SmokeTest`, with two, not three, parameters for `operator()`.
  {
    struct DoesNotNeedACopy {
      std::string text;
      bool operator()(const DDE& e, IDX_TS current) {
        text += Printf("%s, %llu", e.text.c_str(), current.index);
        return false;
      }
    };

    struct NeedsACopy {
      std::string text;
      bool operator()(DDE&& e, IDX_TS current) {
        text += Printf("%s, %llu", e.text.c_str(), current.index);
        e.text = "invalidated <" + e.text + ">";
        return false;
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_FALSE(current::ss::DispatchEntryByConstReference(
        does_not_need_a_copy_1, e, IDX_TS(100, us(100)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(), 100", does_not_need_a_copy_1.text);

    ASSERT_FALSE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        does_not_need_a_copy_2, e, IDX_TS(101, us(101)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(), 101", does_not_need_a_copy_2.text);

    ASSERT_FALSE(
        current::ss::DispatchEntryByConstReference(needs_a_copy_1, e, IDX_TS(102, us(102)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(copy of {Entry()}), 102", needs_a_copy_1.text);

    ASSERT_FALSE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        needs_a_copy_2, e, IDX_TS(103, us(103)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(clone of {Entry()}), 103", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(
        current::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, IDX_TS(104, us(104)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(), 104", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(current::ss::DispatchEntryByRValue(
        needs_a_copy_3, std::move(e), IDX_TS(105, us(105)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(), 105", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }

  // Confirm that `void` return type is treated as `bool true`, with two parameters for `operator()`
  {
    struct DoesNotNeedACopy {
      std::string text;
      void operator()(const DDE& e, IDX_TS current) {
        text += Printf("%s, %llu", e.text.c_str(), current.index);
      }
    };

    struct NeedsACopy {
      std::string text;
      void operator()(DDE&& e, IDX_TS current) {
        text += Printf("%s, %llu", e.text.c_str(), current.index);
        e.text = "invalidated <" + e.text + ">";
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_TRUE(current::ss::DispatchEntryByConstReference(
        does_not_need_a_copy_1, e, IDX_TS(200, us(200)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(), 200", does_not_need_a_copy_1.text);

    ASSERT_TRUE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        does_not_need_a_copy_2, e, IDX_TS(201, us(201)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(), 201", does_not_need_a_copy_2.text);

    ASSERT_TRUE(
        current::ss::DispatchEntryByConstReference(needs_a_copy_1, e, IDX_TS(202, us(202)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(copy of {Entry()}), 202", needs_a_copy_1.text);

    ASSERT_TRUE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        needs_a_copy_2, e, IDX_TS(203, us(203)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(clone of {Entry()}), 203", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(
        current::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, IDX_TS(204, us(204)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(), 204", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(current::ss::DispatchEntryByRValue(
        needs_a_copy_3, std::move(e), IDX_TS(205, us(205)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(), 205", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }

  // The copy-paste of `SmokeTest`, with one, not three, parameters for `operator()`.
  {
    struct DoesNotNeedACopy {
      std::string text;
      bool operator()(const DDE& e) {
        text += e.text;
        return false;
      }
    };

    struct NeedsACopy {
      std::string text;
      bool operator()(DDE&& e) {
        text += e.text;
        e.text = "invalidated <" + e.text + ">";
        return false;
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_FALSE(current::ss::DispatchEntryByConstReference(
        does_not_need_a_copy_1, e, IDX_TS(300, us(300)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry()", does_not_need_a_copy_1.text);

    ASSERT_FALSE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        does_not_need_a_copy_2, e, IDX_TS(301, us(301)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry()", does_not_need_a_copy_2.text);

    ASSERT_FALSE(
        current::ss::DispatchEntryByConstReference(needs_a_copy_1, e, IDX_TS(302, us(302)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(copy of {Entry()})", needs_a_copy_1.text);

    ASSERT_FALSE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        needs_a_copy_2, e, IDX_TS(303, us(303)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(clone of {Entry()})", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(
        current::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, IDX_TS(304, us(304)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry()", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_FALSE(current::ss::DispatchEntryByRValue(
        needs_a_copy_3, std::move(e), IDX_TS(305, us(305)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry()", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }

  // Confirm that `void` return type is treated as `bool true`, with one parameter for `operator()`
  {
    struct DoesNotNeedACopy {
      std::string text;
      void operator()(const DDE& e) { text += e.text; }
    };

    struct NeedsACopy {
      std::string text;
      void operator()(DDE&& e) {
        text += e.text;
        e.text = "invalidated <" + e.text + ">";
      }
    };

    DoesNotNeedACopy does_not_need_a_copy_1;
    DoesNotNeedACopy does_not_need_a_copy_2;
    DoesNotNeedACopy does_not_need_a_copy_3;

    NeedsACopy needs_a_copy_1;
    NeedsACopy needs_a_copy_2;
    NeedsACopy needs_a_copy_3;

    DDE e;

    ASSERT_TRUE(current::ss::DispatchEntryByConstReference(
        does_not_need_a_copy_1, e, IDX_TS(400, us(400)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry()", does_not_need_a_copy_1.text);

    ASSERT_TRUE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        does_not_need_a_copy_2, e, IDX_TS(401, us(401)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry()", does_not_need_a_copy_2.text);

    ASSERT_TRUE(
        current::ss::DispatchEntryByConstReference(needs_a_copy_1, e, IDX_TS(402, us(402)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(copy of {Entry()})", needs_a_copy_1.text);

    ASSERT_TRUE(current::ss::DispatchEntryByConstReference<CustomCloner>(
        needs_a_copy_2, e, IDX_TS(403, us(403)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry(clone of {Entry()})", needs_a_copy_2.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(
        current::ss::DispatchEntryByRValue(does_not_need_a_copy_3, e, IDX_TS(404, us(404)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry()", does_not_need_a_copy_3.text);

    EXPECT_EQ("Entry()", e.text) << "The original `DDE()` should stay immutable.";

    ASSERT_TRUE(current::ss::DispatchEntryByRValue(
        needs_a_copy_3, std::move(e), IDX_TS(405, us(405)), IDX_TS(0, us(0))));
    EXPECT_EQ("Entry()", needs_a_copy_3.text);

    EXPECT_EQ("invalidated <Entry()>", e.text) << "The original `DDE()` should be invalidated.";
  }
}
