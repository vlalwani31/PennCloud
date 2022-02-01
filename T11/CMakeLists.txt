# Copyright 2018 gRPC authors.
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
#
# cmake build file for C++ route_guide example.
# Assumes protobuf and gRPC have been installed using cmake.
# See cmake_externalproject/CMakeLists.txt for all-in-one cmake build
# that automatically builds all the dependencies before building route_guide.

cmake_minimum_required(VERSION 3.5.1)

project(Backend C CXX)

include(../cmake/common.cmake)

file(GLOB RECOVERY_FILES  "${CMAKE_CURRENT_BINARY_DIR}/log/*" "${CMAKE_CURRENT_BINARY_DIR}/cp/*")

file(REMOVE ${RECOVERY_FILES})

# Proto file
get_filename_component(kv_store_proto "./kvstore.proto" ABSOLUTE)
get_filename_component(kv_store_proto_path "${kv_store_proto}" PATH)



# Generated sources
set(CMAKE_CXX_STANDARD 20)

set(kv_store_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/kvstore.pb.cc")
set(kv_store_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/kvstore.pb.h")
set(kv_store_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/kvstore.grpc.pb.cc")
set(kv_store_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/kvstore.grpc.pb.h")


add_custom_command(
      OUTPUT "${kv_store_proto_srcs}" "${kv_store_proto_hdrs}" "${kv_store_grpc_srcs}" "${kv_store_grpc_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
        -I "${kv_store_proto_path}"
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${kv_store_proto}" 
      DEPENDS "${kv_store_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# kv_store_grpc_proto
add_library(kv_store_grpc_proto
  ${kv_store_grpc_srcs}
  ${kv_store_grpc_hdrs}
  ${kv_store_proto_srcs}
  ${kv_store_proto_hdrs})
target_link_libraries(kv_store_grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})
  
  
# key_value_store_helper
add_library(key_value_store_helper
  "user_data.h"
  "user_data.cc"
  "backend_utils.h"
  "backend_utils.cc"
  "key_value_store_client.cc"
  "key_value_store_client.h"
  "user.cc"
  "user.h"
  "smtpclient.cc"
  "smtpclient.h"
  "process_request.cc"
  "process_request.h"
  "utils.cc"
  "utils.h"
  "smtpserver.cc"
  )
target_link_libraries(key_value_store_helper
  kv_store_grpc_proto
  -lresolv
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# Targets frontend backend and backend coordinator
foreach(_target
  user webserver key_value_store_server key_value_store_master frontend_master)
  add_executable(${_target}
    "${_target}.cc")
  target_link_libraries(${_target}
    kv_store_grpc_proto
    key_value_store_helper
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()




