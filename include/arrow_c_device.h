// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#ifndef ARROW_C_DEVICE_DATA_H
#define ARROW_C_DEVICE_DATA_H

#include "arrow_c_data.h"

#ifdef __cplusplus
extern "C" {
#endif

// Arrow C Device Data Interface
// https://arrow.apache.org/docs/format/CDeviceDataInterface.html

// Device type for ArrowDeviceArray
#ifndef ARROW_DEVICE_CPU
#define ARROW_DEVICE_CPU 1
#endif
#ifndef ARROW_DEVICE_CUDA
#define ARROW_DEVICE_CUDA 2
#endif
#ifndef ARROW_DEVICE_CUDA_HOST
#define ARROW_DEVICE_CUDA_HOST 3
#endif
#ifndef ARROW_DEVICE_OPENCL
#define ARROW_DEVICE_OPENCL 4
#endif
#ifndef ARROW_DEVICE_VULKAN
#define ARROW_DEVICE_VULKAN 7
#endif
#ifndef ARROW_DEVICE_METAL
#define ARROW_DEVICE_METAL 8
#endif
#ifndef ARROW_DEVICE_VPI
#define ARROW_DEVICE_VPI 9
#endif
#ifndef ARROW_DEVICE_ROCM
#define ARROW_DEVICE_ROCM 10
#endif
#ifndef ARROW_DEVICE_ROCM_HOST
#define ARROW_DEVICE_ROCM_HOST 11
#endif
#ifndef ARROW_DEVICE_EXT_DEV
#define ARROW_DEVICE_EXT_DEV 12
#endif
#ifndef ARROW_DEVICE_CUDA_MANAGED
#define ARROW_DEVICE_CUDA_MANAGED 13
#endif
#ifndef ARROW_DEVICE_ONEAPI
#define ARROW_DEVICE_ONEAPI 14
#endif
#ifndef ARROW_DEVICE_WEBGPU
#define ARROW_DEVICE_WEBGPU 15
#endif
#ifndef ARROW_DEVICE_HEXAGON
#define ARROW_DEVICE_HEXAGON 16
#endif

typedef int32_t ArrowDeviceType;

struct ArrowDeviceArray {
  struct ArrowArray array;
  int64_t device_id;
  ArrowDeviceType device_type;
  void* sync_event;
  int64_t _reserved[3];
};

struct ArrowDeviceArrayStream {
  ArrowDeviceType device_type;
  int (*get_schema)(struct ArrowDeviceArrayStream*, struct ArrowSchema* out);
  int (*get_next)(struct ArrowDeviceArrayStream*, struct ArrowDeviceArray* out);
  const char* (*get_last_error)(struct ArrowDeviceArrayStream*);
  void (*release)(struct ArrowDeviceArrayStream*);
  void* private_data;
};

#ifdef __cplusplus
}
#endif

#endif  // ARROW_C_DEVICE_DATA_H
