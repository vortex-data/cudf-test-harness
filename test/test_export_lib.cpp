// A simple test library that exports an Arrow device array
// This creates a simple int32 array on CUDA for testing the harness

#include <cuda_runtime.h>
#include <cstdlib>
#include <cstring>

// Include the Arrow C Device Data Interface definitions
extern "C" {

#include <stdint.h>

#ifndef ARROW_FLAG_NULLABLE
#define ARROW_FLAG_NULLABLE 2
#endif

#ifndef ARROW_DEVICE_CUDA
#define ARROW_DEVICE_CUDA 2
#endif

typedef int32_t ArrowDeviceType;

struct ArrowSchema {
    const char* format;
    const char* name;
    const char* metadata;
    int64_t flags;
    int64_t n_children;
    struct ArrowSchema** children;
    struct ArrowSchema* dictionary;
    void (*release)(struct ArrowSchema*);
    void* private_data;
};

struct ArrowArray {
    int64_t length;
    int64_t null_count;
    int64_t offset;
    int64_t n_buffers;
    int64_t n_children;
    const void** buffers;
    struct ArrowArray** children;
    struct ArrowArray* dictionary;
    void (*release)(struct ArrowArray*);
    void* private_data;
};

struct ArrowDeviceArray {
    struct ArrowArray array;
    int64_t device_id;
    ArrowDeviceType device_type;
    void* sync_event;
};

// Private data structure to hold allocated resources
struct PrivateData {
    void* data_buffer;
    const void** buffers;
};

// Release callback for schema
void release_schema(struct ArrowSchema* schema) {
    if (schema->release == nullptr) return;
    schema->format = nullptr;
    schema->name = nullptr;
    schema->release = nullptr;
}

// Release callback for array
void release_array(struct ArrowArray* array) {
    if (array->release == nullptr) return;

    auto* private_data = static_cast<PrivateData*>(array->private_data);
    if (private_data) {
        if (private_data->data_buffer) {
            cudaFree(private_data->data_buffer);
        }
        delete[] private_data->buffers;
        delete private_data;
    }

    array->release = nullptr;
    array->private_data = nullptr;
}

// Export function that creates a simple int32 array on GPU
int export_array(ArrowSchema* schema, ArrowDeviceArray* device_array) {
    // Set up schema for int32
    schema->format = "i";  // int32
    schema->name = "test_column";
    schema->metadata = nullptr;
    schema->flags = ARROW_FLAG_NULLABLE;
    schema->n_children = 0;
    schema->children = nullptr;
    schema->dictionary = nullptr;
    schema->release = release_schema;
    schema->private_data = nullptr;

    // Create test data: 10 int32 values
    const int64_t num_elements = 10;
    int32_t host_data[num_elements] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // Allocate GPU memory
    void* device_data = nullptr;
    cudaError_t err = cudaMalloc(&device_data, num_elements * sizeof(int32_t));
    if (err != cudaSuccess) {
        schema->release = nullptr;
        return 1;
    }

    // Copy data to GPU
    err = cudaMemcpy(device_data, host_data, num_elements * sizeof(int32_t), cudaMemcpyHostToDevice);
    if (err != cudaSuccess) {
        cudaFree(device_data);
        schema->release = nullptr;
        return 1;
    }

    // Allocate private data structure
    auto* private_data = new PrivateData();
    private_data->data_buffer = device_data;

    // Set up buffers array (null bitmap, data)
    private_data->buffers = new const void*[2];
    private_data->buffers[0] = nullptr;  // No null bitmap
    private_data->buffers[1] = device_data;

    // Set up array
    device_array->array.length = num_elements;
    device_array->array.null_count = 0;
    device_array->array.offset = 0;
    device_array->array.n_buffers = 2;
    device_array->array.n_children = 0;
    device_array->array.buffers = private_data->buffers;
    device_array->array.children = nullptr;
    device_array->array.dictionary = nullptr;
    device_array->array.release = release_array;
    device_array->array.private_data = private_data;

    // Set up device info
    device_array->device_type = ARROW_DEVICE_CUDA;
    device_array->device_id = 0;
    device_array->sync_event = nullptr;

    return 0;
}

}  // extern "C"
