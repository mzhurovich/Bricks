/*******************************************************************************
The MIT License (MIT)

Copyright (c) 2015 Dmitry "Dima" Korolev <dmitry.korolev@gmail.com>

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

#ifndef BRICKS_GRAPH_DOCU_02
#define BRICKS_GRAPH_DOCU_02

#include "../plotutils.h"

#include "../../3party/gtest/gtest-main.h"

TEST(Graph, PlotutilsLove) {
  // Where visualization meets love.
  const size_t N = 1000;
  std::vector<std::pair<double, double>> line(N);
    
  for (size_t i = 0; i < N; ++i) {
    const double t = M_PI * 2 * i / (N - 1);
    line[i] = std::make_pair(
      16 * pow(sin(t), 3),
      -(13 * cos(t) + 5 * cos(t * 2) - 2 * cos(t * 3) - cos(t * 4)));
  }
  
  // Pull Plotutils, LineColor, GridStyle and more plotutils-related symbols.
  using namespace bricks::plotutils;
  
  const std::string result = Plotutils(line)
    .LineMode(CustomLineMode(LineColor::Red, LineStyle::LongDashed))
    .GridStyle(GridStyle::Full)
    .Label("Imagine all the people ...")
    .X("... living life in peace")
    .Y("John Lennon, \"Imagine\"")
    .LineWidth(0.015)
    .OutputFormat("png");

ASSERT_EQ(result, bricks::FileSystem::ReadFileAsString("golden/love.png"));
}

#endif  // BRICKS_GRAPH_DOCU_02
