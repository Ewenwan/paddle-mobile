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

#pragma once

#include <string>
#include <vector>

#include "common/log.h"
#include "framework/op_registry.h"
#include "io/io.h"
#include "operators/conv_op.h"
#include "operators/elementwise_add_op.h"
#include "operators/pool_op.h"
#include "operators/relu_op.h"
#include "operators/reshape_op.h"
#include "operators/sigmoid_op.h"
#include "operators/softmax_op.h"
#include "operators/transpose_op.h"

using paddle_mobile::Executor;
using paddle_mobile::framework::BlockDesc;
using paddle_mobile::framework::DDim;
using paddle_mobile::framework::LoDTensor;
using paddle_mobile::framework::OpDesc;
using paddle_mobile::framework::Program;
using paddle_mobile::framework::Tensor;
using paddle_mobile::framework::Variable;
using std::string;
using std::vector;
template <typename DeviceType, typename OpType>
class Executor4Test : public Executor<DeviceType> {
 public:
  Executor4Test(Program<DeviceType> p, string op_type,
                bool use_optimize = false)
      : Executor<DeviceType>() {
    this->use_optimize_ = use_optimize;
    this->program_ = p;
    if (this->use_optimize_) {
      this->to_predict_program_ = this->program_.optimizeProgram;
    } else {
      this->to_predict_program_ = this->program_.originProgram;
    }

    if (this->program_.originProgram == nullptr) {
      LOG(paddle_mobile::LogLevel::kLOG_ERROR)
          << "to_predict_program_ == nullptr";
    }
    const std::vector<std::shared_ptr<BlockDesc>> blocks =
        this->to_predict_program_->Blocks();
    for (std::shared_ptr<BlockDesc> block_desc : blocks) {
      std::vector<std::shared_ptr<OpDesc>> ops = block_desc->Ops();
      for (std::shared_ptr<OpDesc> op : ops) {
        if (op->Type() == op_type) {
          DLOG << "匹配到: " << op->Type();

          /// test first meeting op in program
          std::shared_ptr<paddle_mobile::framework::OperatorBase<DeviceType>>
              op_ptr =
                  paddle_mobile::framework::OpRegistry<DeviceType>::CreateOp(
                      op->Type(), op->GetInputs(), op->GetOutputs(),
                      op->GetAttrMap(), this->program_.scope);
          this->ops_of_block_[*block_desc.get()].push_back(op_ptr);
          break;
        }
      }
    }
    this->InitMemory();
  }

  template <typename T = LoDTensor>
  vector<std::shared_ptr<Tensor>> Predict(const vector<Tensor> &ts,
                                          const vector<string> &input_names,
                                          const vector<string> &output_names,
                                          const vector<DDim> &ddims) {
    auto scope = this->program_.scope;
    size_t input_size = input_names.size();
    size_t out_size = output_names.size();

    vector<Variable *> input_vars(input_size);
    vector<LoDTensor *> input_tensors(input_size);
    for (int i = 0; i < input_size; i++) {
      input_vars[i] = scope->Var(input_names[i]);
      input_tensors[i] = input_vars[i]->GetMutable<T>();
      input_tensors[i]->ShareDataWith(ts[i]);
    }

    vector<Variable *> output_vars(out_size);
    vector<LoDTensor *> output_tensors(out_size);
    vector<std::shared_ptr<Tensor>> output_tensor_sptrs(out_size);

    for (int i = 0; i < out_size; i++) {
      output_vars[i] = scope->Var(output_names[i]);
      output_tensors[i] = output_vars[i]->GetMutable<T>();
      output_tensors[i]->mutable_data<float>(ddims[i]);
      output_tensor_sptrs[i] = std::make_shared<LoDTensor>();
      output_tensor_sptrs[i].reset(output_tensors[i]);
    }

    std::shared_ptr<paddle_mobile::framework::BlockDesc> to_predict_block =
        this->to_predict_program_->Block(0);
    for (int j = 0; j < this->ops_of_block_[*to_predict_block.get()].size();
         ++j) {
      auto op = this->ops_of_block_[*to_predict_block.get()][j];
      op->Run();
    }

    return output_tensor_sptrs;
  }

  std::shared_ptr<Tensor> Predict(const Tensor &t, string input, string output,
                                  const DDim &dDim) {
    auto scope = this->program_.scope;
    Variable *g_feed_value = scope->Var(input);
    auto tensor = g_feed_value->GetMutable<LoDTensor>();
    tensor->ShareDataWith(t);

    Variable *con_output = scope->Var(output);
    auto *output_tensor = con_output->GetMutable<LoDTensor>();
    output_tensor->mutable_data<float>(dDim);

    std::shared_ptr<Tensor> out_tensor = std::make_shared<LoDTensor>();
    out_tensor.reset(output_tensor);

    std::shared_ptr<paddle_mobile::framework::BlockDesc> to_predict_block =
        this->to_predict_program_->Block(0);
    for (int j = 0; j < this->ops_of_block_[*to_predict_block.get()].size();
         ++j) {
      auto op = this->ops_of_block_[*to_predict_block.get()][j];
      op->Run();
    }

    return out_tensor;
  }
};
