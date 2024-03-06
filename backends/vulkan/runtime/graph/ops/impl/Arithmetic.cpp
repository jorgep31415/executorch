/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include <executorch/backends/vulkan/runtime/graph/ops/impl/Arithmetic.h>

#include <executorch/backends/vulkan/runtime/graph/ops/OpUtils.h>

#include <executorch/backends/vulkan/runtime/graph/ops/impl/Staging.h>

namespace at {
namespace native {
namespace vulkan {

#define DEFINE_ARITHMETIC_FN(function, shader)                                \
  ValueRef function(ComputeGraph& graph, const std::vector<ValueRef>& args) { \
    return add_arithmetic_node(                                               \
        graph, args[0], args[1], args[2], VK_KERNEL(shader), args[3]);        \
  }

DEFINE_ARITHMETIC_FN(add, add);
DEFINE_ARITHMETIC_FN(sub, sub);
DEFINE_ARITHMETIC_FN(mul, mul);
DEFINE_ARITHMETIC_FN(div, div);
DEFINE_ARITHMETIC_FN(floor_div, floor_divide);
DEFINE_ARITHMETIC_FN(pow, pow);

// TODO(T180908843): Bypass this entrypoint function by creating `ValueRef out`
// ahead of time.
ValueRef add_arithmetic_node(
    ComputeGraph& graph,
    const ValueRef in1,
    const ValueRef in2,
    const float alpha,
    const api::ShaderInfo& shader,
    const int64_t shared_object_idx) {
  std::vector<int64_t> in1_sizes = graph.get_val_sizes(in1);
  api::ScalarType in1_dtype = graph.get_val_dtype(in1);

  ValueRef out = graph.add_tensor(in1_sizes, in1_dtype, shared_object_idx);
  add_arithmetic_node(graph, in1, in2, out, alpha, shader);
  return out;
}

void add_arithmetic_node(
    ComputeGraph& graph,
    const ValueRef in1,
    const ValueRef in2,
    const ValueRef out,
    const float alpha,
    const api::ShaderInfo& shader) {
  ValueRef arg1 = prepack_if_tensor_ref(graph, in1);
  ValueRef arg2 = prepack_if_tensor_ref(graph, in2);

  vTensor& t_in1 = graph.get_val(arg1).toTensor();
  vTensor& t_in2 = graph.get_val(arg2).toTensor();
  vTensor& t_out = graph.get_val(out).toTensor();

  api::utils::uvec3 global_size = t_out.extents();
  api::utils::uvec3 local_size = adaptive_work_group_size(global_size);

  ArithmeticParams block{
      get_size_as_ivec4(t_out),
      get_size_as_ivec4(t_in1),
      get_size_as_ivec4(t_in2),
      1.0,
  };
  api::UniformParamsBuffer params(graph.context(), block);

  graph.execute_nodes().emplace_back(new ExecuteNode(
      shader,
      global_size,
      local_size,
      {{out, api::MemoryAccessType::WRITE},
       {{arg1, arg2}, api::MemoryAccessType::READ}},
      std::move(params)));
}

} // namespace vulkan
} // namespace native
} // namespace at
