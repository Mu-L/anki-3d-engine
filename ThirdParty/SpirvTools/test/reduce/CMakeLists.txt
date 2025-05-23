# Copyright (c) 2018 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

add_spvtools_unittest(TARGET reduce
        SRCS
        conditional_branch_to_simple_conditional_branch_test.cpp
        merge_blocks_test.cpp
        operand_to_constant_test.cpp
        operand_to_undef_test.cpp
        operand_to_dominating_id_test.cpp
        reduce_test_util.cpp
        reduce_test_util.h
        reducer_test.cpp
        remove_block_test.cpp
        remove_function_test.cpp
        remove_selection_test.cpp
        remove_unused_instruction_test.cpp
        remove_unused_struct_member_test.cpp
        simple_conditional_branch_to_branch_test.cpp
        structured_construct_to_block_test.cpp
        structured_loop_to_selection_test.cpp
        validation_during_reduction_test.cpp
        ${spirv-tools_SOURCE_DIR}/tools/io.cpp
        LIBS SPIRV-Tools-reduce
        )

