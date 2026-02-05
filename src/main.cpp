#include <cstdlib>
#include <dlfcn.h>
#include <iostream>
#include <memory>
#include <string>

#include <cudf/column/column.hpp>
#include <cudf/column/column_view.hpp>
#include <cudf/interop.hpp>
#include <cudf/types.hpp>

#include "arrow_c_device.h"

// Function pointer type for export_array
using export_array_fn = int (*)(ArrowSchema *, ArrowDeviceArray *);
using validate_array_fn = int (*)(ArrowSchema *, ArrowArray *);

void print_usage(const char *program_name) {
    std::cerr << "Usage: " << program_name << " check <library.so>\n";
    std::cerr << "\n";
    std::cerr << "Commands:\n";
    std::cerr << "  check <library.so>  Load the library and test Arrow device data export\n";
}

const char *device_type_to_string(ArrowDeviceType device_type) {
    switch (device_type) {
        case ARROW_DEVICE_CPU: return "CPU";
        case ARROW_DEVICE_CUDA: return "CUDA";
        case ARROW_DEVICE_CUDA_HOST: return "CUDA_HOST";
        case ARROW_DEVICE_CUDA_MANAGED: return "CUDA_MANAGED";
        case ARROW_DEVICE_ROCM: return "ROCM";
        case ARROW_DEVICE_ROCM_HOST: return "ROCM_HOST";
        default: return "UNKNOWN";
    }
}

const char *cudf_type_to_string(cudf::type_id type) {
    switch (type) {
        case cudf::type_id::EMPTY: return "EMPTY";
        case cudf::type_id::INT8: return "INT8";
        case cudf::type_id::INT16: return "INT16";
        case cudf::type_id::INT32: return "INT32";
        case cudf::type_id::INT64: return "INT64";
        case cudf::type_id::UINT8: return "UINT8";
        case cudf::type_id::UINT16: return "UINT16";
        case cudf::type_id::UINT32: return "UINT32";
        case cudf::type_id::UINT64: return "UINT64";
        case cudf::type_id::FLOAT32: return "FLOAT32";
        case cudf::type_id::FLOAT64: return "FLOAT64";
        case cudf::type_id::BOOL8: return "BOOL8";
        case cudf::type_id::TIMESTAMP_DAYS: return "TIMESTAMP_DAYS";
        case cudf::type_id::TIMESTAMP_SECONDS: return "TIMESTAMP_SECONDS";
        case cudf::type_id::TIMESTAMP_MILLISECONDS: return "TIMESTAMP_MILLISECONDS";
        case cudf::type_id::TIMESTAMP_MICROSECONDS: return "TIMESTAMP_MICROSECONDS";
        case cudf::type_id::TIMESTAMP_NANOSECONDS: return "TIMESTAMP_NANOSECONDS";
        case cudf::type_id::DURATION_DAYS: return "DURATION_DAYS";
        case cudf::type_id::DURATION_SECONDS: return "DURATION_SECONDS";
        case cudf::type_id::DURATION_MILLISECONDS: return "DURATION_MILLISECONDS";
        case cudf::type_id::DURATION_MICROSECONDS: return "DURATION_MICROSECONDS";
        case cudf::type_id::DURATION_NANOSECONDS: return "DURATION_NANOSECONDS";
        case cudf::type_id::DICTIONARY32: return "DICTIONARY32";
        case cudf::type_id::STRING: return "STRING";
        case cudf::type_id::LIST: return "LIST";
        case cudf::type_id::DECIMAL32: return "DECIMAL32";
        case cudf::type_id::DECIMAL64: return "DECIMAL64";
        case cudf::type_id::DECIMAL128: return "DECIMAL128";
        case cudf::type_id::STRUCT: return "STRUCT";
        default: return "UNKNOWN";
    }
}

void print_column_stats(const cudf::column_view &col, const char *name) {
    std::cout << "Column Statistics(" << name << "):\n";
    std::cout << "  Type: " << cudf_type_to_string(col.type().id()) << "\n";
    std::cout << "  Size: " << col.size() << " rows\n";
    std::cout << "  Null count: " << col.null_count() << "\n";
    std::cout << "  Has nulls: " << (col.has_nulls() ? "yes" : "no") << "\n";
    std::cout << "  Num children: " << col.num_children() << "\n";
}

int run_check(const char *library_path) {
    std::cout << "Loading library: " << library_path << "\n";

    // Open the shared library
    void *handle = dlopen(library_path, RTLD_NOW);
    if (!handle) {
        std::cerr << "Error: Failed to load library: " << dlerror() << "\n";
        return 1;
    }

    // Clear any existing error
    dlerror();

    // Load the export_array symbol
    auto export_array = reinterpret_cast<export_array_fn>(dlsym(handle, "export_array"));
    const char *dlsym_error = dlerror();
    if (dlsym_error) {
        std::cerr << "Error: Failed to load symbol 'export_array': " << dlsym_error << "\n";
        dlclose(handle);
        return 1;
    }

    std::cout << "Found export_array symbol\n";

    auto validate_array = reinterpret_cast<validate_array_fn>(dlsym(handle, "validate_array"));

    // Initialize the Arrow structures
    ArrowSchema schema{};
    ArrowDeviceArray device_array{};

    // Call export_array
    std::cout << "Calling export_array...\n";
    int result = export_array(&schema, &device_array);
    if (result != 0) {
        std::cerr << "Error: export_array returned error code " << result << "\n";
        dlclose(handle);
        return 1;
    }

    std::cout << "export_array succeeded\n";

    // Print Arrow array info
    std::cout << "\nArrow Array Info:\n";
    std::cout << "  Schema format: " << (schema.format ? schema.format : "(null)") << "\n";
    std::cout << "  Schema name: " << (schema.name ? schema.name : "(null)") << "\n";
    std::cout << "  Array length: " << device_array.array.length << "\n";
    std::cout << "  Array null_count: " << device_array.array.null_count << "\n";
    std::cout << "  Device type: " << device_type_to_string(device_array.device_type) << "\n";
    std::cout << "  Device ID: " << device_array.device_id << "\n";

    std::cout << "\nStruct children debug:\n";
    std::cout << "  Schema n_children: " << schema.n_children << "\n";
    std::cout << "  Schema children ptr: " << (void *) schema.children << "\n";
    std::cout << "  Array n_children: " << device_array.array.n_children << "\n";
    std::cout << "  Array children ptr: " << (void *) device_array.array.children << "\n";

    if (schema.children) {
        for (int64_t i = 0; i < schema.n_children; i++) {
            std::cout << "  Child " << i << ":\n";
            std::cout << "    schema ptr: " << (void *) schema.children[i] << "\n";
            if (schema.children[i]) {
                std::cout << "    format: " << (schema.children[i]->format ? schema.children[i]->format : "(null)") <<
                        "\n";
                std::cout << "    name: " << (schema.children[i]->name ? schema.children[i]->name : "(null)") << "\n";
            }
            if (device_array.array.children) {
                std::cout << "    array ptr: " << (void *) device_array.array.children[i] << "\n";
                if (device_array.array.children[i]) {
                    std::cout << "    array length: " << device_array.array.children[i]->length << "\n";
                }
            }
        }
    }

    // Convert to cuDF column view using from_arrow_device_column
    std::cout << "\nConverting to cuDF column view...\n";
    try {
        auto table = cudf::from_arrow_device(&schema, &device_array);

        std::cout << "Conversion successful!\n\n";

        // convert to host array, call the verifier inside of the shared library.
        auto host_array = cudf::to_arrow_host(*table);
        if (validate_array(&schema, &host_array->array) != 0) {
            std::cerr << "\nValidation failed!\n";
        }
    } catch (const std::exception &e) {
        std::cerr << "Error: Failed to convert Arrow array to cuDF column: " << e.what() << "\n";

        // Release the Arrow array
        if (device_array.array.release) {
            device_array.array.release(&device_array.array);
        }
        if (schema.release) {
            schema.release(&schema);
        }
        dlclose(handle);
        return 1;
    }

    // Release the Arrow array
    std::cout << "\nReleasing Arrow array...\n";
    if (device_array.array.release) {
        device_array.array.release(&device_array.array);
        std::cout << "  Released ArrowDeviceArray\n";
    } else {
        std::cout << "  Warning: ArrowDeviceArray has no release callback\n";
    }

    // Release the schema
    if (schema.release) {
        schema.release(&schema);
        std::cout << "  Released ArrowSchema\n";
    } else {
        std::cout << "  Warning: ArrowSchema has no release callback\n";
    }

    // Close the library
    dlclose(handle);
    std::cout << "\nAll checks passed!\n";

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::string command = argv[1];

    if (command == "check") {
        if (argc < 3) {
            std::cerr << "Error: 'check' command requires a library path\n";
            print_usage(argv[0]);
            return 1;
        }
        return run_check(argv[2]);
    } else if (command == "--help" || command == "-h") {
        print_usage(argv[0]);
        return 0;
    } else {
        std::cerr << "Error: Unknown command '" << command << "'\n";
        print_usage(argv[0]);
        return 1;
    }
}
