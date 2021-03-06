/* Copyright (c) 2018 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include <fstream>
#include "../test_helper.h"
#include "../test_include.h"

int main() {
  paddle_mobile::Loader<paddle_mobile::CPU> loader;
  auto time1 = time();
  auto program = loader.Load(g_resnet, false);
  auto time2 = time();
  DLOG << "load cost :" << time_diff(time1, time1) << "ms";
  paddle_mobile::Executor<paddle_mobile::CPU> executor(program, 1, false);

  std::vector<int64_t> dims{1, 3, 32, 32};
  Tensor input_tensor;
  SetupTensor<float>(&input_tensor, {1, 3, 32, 32}, static_cast<float>(0),
                     static_cast<float>(1));

  std::vector<float> input(input_tensor.data<float>(),
                           input_tensor.data<float>() + input_tensor.numel());
  auto time3 = time();
  executor.Predict(input, dims);
  auto time4 = time();
  DLOG << "predict cost :" << time_diff(time3, time4) << "ms";
  return 0;
}
