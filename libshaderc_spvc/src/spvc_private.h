// Copyright 2019 The Shaderc Authors. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <spirv_msl.hpp>
#include <spirv_reflect.hpp>
#include <string>
#include <vector>

#include "spirv-tools/libspirv.hpp"
#include "spvc/spvc.h"

// GLSL version produced when none specified nor detected from source.
#define DEFAULT_GLSL_VERSION 450

struct shaderc_spvc_context {
  std::unique_ptr<spirv_cross::Compiler> cross_compiler;
  std::string messages;
};

struct shaderc_spvc_compile_options {
  bool validate = true;
  bool optimize = true;
  bool remove_unused_variables = false;
  bool robust_buffer_access_pass = false;
  bool flatten_ubo = false;
  bool force_es = false;
  bool forced_es_setting = false;
  std::string entry_point;
  spv_target_env source_env = SPV_ENV_VULKAN_1_0;
  spv_target_env target_env = SPV_ENV_VULKAN_1_0;
  std::vector<uint32_t> msl_discrete_descriptor_sets;
  spirv_cross::CompilerGLSL::Options glsl;
  spirv_cross::CompilerHLSL::Options hlsl;
  spirv_cross::CompilerMSL::Options msl;
};

// Described in spvc.h.
struct shaderc_spvc_compilation_result {
  std::string string_output;
  std::vector<uint32_t> binary_output;
};

namespace spvc_private {

// Convert from shaderc values to spirv-tools values.
spv_target_env get_spv_target_env(shaderc_target_env env,
                                  shaderc_env_version version);

// Handler for spirv-tools error/warning messages that records them in the
// results structure.
void consume_spirv_tools_message(shaderc_spvc_context* context,
                                 spv_message_level_t level, const char* src,
                                 const spv_position_t& pos,
                                 const char* message);

// Test whether or not the given SPIR-V binary is valid for the specific
// environment. Invoke spirv-val to perform this operation.
shaderc_compilation_status validate_spirv(shaderc_spvc_context* context,
                                          spv_target_env env,
                                          const uint32_t* source,
                                          size_t source_len);

// Convert SPIR-V from one environment to another, if there is a known
// conversion. If the origin and destination environments are the same, then
// the binary is just copied to the output buffer. Invokes spirv-opt to perform
// the actual translation.
shaderc_compilation_status translate_spirv(
    shaderc_spvc_context* context, spv_target_env source_env,
    spv_target_env target_env, const uint32_t* source, size_t source_len,
    shaderc_spvc_compile_options_t options, std::vector<uint32_t>* target);

// Execute the validation and translate steps. Specifically validates the input,
// transforms it, then validates the transformed input. All of these steps are
// only done if needed.
shaderc_compilation_status validate_and_translate_spirv(
    shaderc_spvc_context* context, const uint32_t* source, size_t source_len,
    shaderc_spvc_compile_options_t options, std::vector<uint32_t>* target);

// Given a configured compiler run it to generate a shader. Does all of the
// required trapping to handle if the compile fails.
shaderc_compilation_status generate_shader(
    spirv_cross::Compiler* compiler, shaderc_spvc_compilation_result_t result);

// Given a Vulkan SPIR-V shader and set of options generate a GLSL shader.
// Handles correctly setting up the SPIRV-Cross compiler based on the options
// and then envoking it.
shaderc_compilation_status generate_glsl_shader(
    const shaderc_spvc_context_t context, const uint32_t* source,
    size_t source_len, shaderc_spvc_compile_options_t options,
    shaderc_spvc_compilation_result_t result);

// Given a Vulkan SPIR-V shader and set of options generate a HLSL shader.
// Handles correctly setting up the SPIRV-Cross compiler based on the options
// and then envoking it.
shaderc_compilation_status generate_hlsl_shader(
    const shaderc_spvc_context_t context, const uint32_t* source,
    size_t source_len, shaderc_spvc_compile_options_t options,
    shaderc_spvc_compilation_result_t result);

// Given a Vulkan SPIR-V shader and set of options generate a MSL shader.
// Handles correctly setting up the SPIRV-Cross compiler based on the options
// and then envoking it.
shaderc_compilation_status generate_msl_shader(
    const shaderc_spvc_context_t context, const uint32_t* source,
    size_t source_len, shaderc_spvc_compile_options_t options,
    shaderc_spvc_compilation_result_t result);

// Given a Vulkan SPIR-V shader and set of options, generate a Vulkan shader.
// Is a No-op from the perspective of converting the shader, but setup a
// SPIRV-Cross compiler to be used for reflection later.
shaderc_compilation_status generate_vulkan_shader(
    const shaderc_spvc_context_t context, const uint32_t* source,
    size_t source_len, shaderc_spvc_compile_options_t options,
    shaderc_spvc_compilation_result_t result);

// Given a pointer to an SPIRV-Cross IR (with initialized spirv field), Invokes
// spirv-opt to generate a SPIRV-Cross IR (ie. populate empty fields of the
// given spirv_cross::ParsedIR* ir).
shaderc_compilation_status generate_spvcir(
    const shaderc_spvc_context_t context, spirv_cross::ParsedIR* ir,
    const uint32_t* source, size_t source_len,
    shaderc_spvc_compile_options_t options);

}  // namespace spvc_private