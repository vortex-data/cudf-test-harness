#include <cstdint>
#include <iomanip>
#include <dlfcn.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <cudf/interop.hpp>

#include "arrow_c_device.h"

using export_array_fn = int (*)(ArrowSchema *, ArrowDeviceArray *);
using export_device_stream_fn = int (*)(ArrowDeviceArrayStream *);
using validate_array_fn = int (*)(ArrowSchema *, ArrowArray *);

void print_usage(const char *program_name) {
    std::cerr << "Usage: " << program_name << " <command> <library.so>\n";
    std::cerr << "\n";
    std::cerr << "Commands:\n";
    std::cerr << "  check <library.so>         Test single ArrowDeviceArray export\n";
    std::cerr << "  check-stream <library.so>  Test ArrowDeviceArrayStream export\n";
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

void release_schema(ArrowSchema *schema) {
    if (schema != nullptr && schema->release != nullptr) {
        schema->release(schema);
    }
}

void release_device_array(ArrowDeviceArray *device_array) {
    if (device_array != nullptr && device_array->array.release != nullptr) {
        device_array->array.release(&device_array->array);
    }
}

void release_device_stream(ArrowDeviceArrayStream *stream) {
    if (stream != nullptr && stream->release != nullptr) {
        stream->release(stream);
    }
}

class SharedLibrary {
public:
    explicit SharedLibrary(const char *path) {
        std::cout << "Library: " << path << "\n";
        handle_ = dlopen(path, RTLD_NOW);
        if (handle_ == nullptr) {
            throw std::runtime_error(std::string("failed to load library: ") + dlerror());
        }
        dlerror();
    }

    ~SharedLibrary() {
        if (handle_ != nullptr) {
            dlclose(handle_);
        }
    }

    SharedLibrary(SharedLibrary const &) = delete;
    SharedLibrary &operator=(SharedLibrary const &) = delete;

    template <typename T>
    T symbol(const char *name) const {
        dlerror();
        auto symbol = reinterpret_cast<T>(dlsym(handle_, name));
        const char *error = dlerror();
        if (error != nullptr) {
            throw std::runtime_error(std::string("failed to load symbol ") + name + ": " + error);
        }
        return symbol;
    }

private:
    void *handle_ = nullptr;
};

struct OwnedSchema {
    ArrowSchema value{};

    ~OwnedSchema() { release_schema(&value); }

    OwnedSchema() = default;
    OwnedSchema(OwnedSchema const &) = delete;
    OwnedSchema &operator=(OwnedSchema const &) = delete;

    ArrowSchema *get() { return &value; }
};

struct OwnedDeviceArray {
    ArrowDeviceArray value{};

    ~OwnedDeviceArray() { release_device_array(&value); }

    OwnedDeviceArray() = default;
    OwnedDeviceArray(OwnedDeviceArray const &) = delete;
    OwnedDeviceArray &operator=(OwnedDeviceArray const &) = delete;

    ArrowDeviceArray *get() { return &value; }
    bool is_live() const { return value.array.release != nullptr; }
};

struct OwnedDeviceArrayStream {
    ArrowDeviceArrayStream value{};

    ~OwnedDeviceArrayStream() { release_device_stream(&value); }

    OwnedDeviceArrayStream() = default;
    OwnedDeviceArrayStream(OwnedDeviceArrayStream const &) = delete;
    OwnedDeviceArrayStream &operator=(OwnedDeviceArrayStream const &) = delete;

    ArrowDeviceArrayStream *get() { return &value; }
};

std::vector<cudf::column_metadata> metadata_from_schema(ArrowSchema const *schema,
                                                        cudf::size_type num_columns) {
    if (schema == nullptr || schema->children == nullptr || schema->n_children != num_columns) {
        throw std::runtime_error("exported schema does not match imported table");
    }

    auto metadata = std::vector<cudf::column_metadata>{};
    metadata.reserve(num_columns);
    for (auto i = 0; i < num_columns; ++i) {
        auto const child_schema = schema->children[i];
        if (child_schema == nullptr || child_schema->name == nullptr) {
            throw std::runtime_error("exported schema child is missing a name");
        }
        metadata.push_back(cudf::column_metadata{child_schema->name});
    }
    return metadata;
}

void validate_imported_table(cudf::table_view const &table,
                             ArrowSchema const *schema,
                             validate_array_fn validate_array) {
    auto host_array = cudf::to_arrow_host(table);
    auto host_metadata = metadata_from_schema(schema, table.num_columns());
    auto host_schema = cudf::to_arrow_schema(table, host_metadata);
    if (validate_array(host_schema.get(), &host_array->array) != 0) {
        throw std::runtime_error("validation failed");
    }
}

void print_device_array_summary(char const *label,
                                ArrowSchema const *schema,
                                ArrowDeviceArray const *device_array) {
    std::cout << label << "\n"
              << "  rows: " << device_array->array.length
              << ", nulls: " << device_array->array.null_count
              << ", device: " << device_type_to_string(device_array->device_type) << ":"
              << device_array->device_id
              << ", children: " << schema->n_children << "\n";

    for (int64_t i = 0; i < schema->n_children; ++i) {
        auto const child_schema = schema->children[i];
        auto const child_array = device_array->array.children[i];
        auto const child_name =
            child_schema != nullptr && child_schema->name != nullptr ? child_schema->name : "(unnamed)";
        auto const child_format =
            child_schema != nullptr && child_schema->format != nullptr ? child_schema->format : "(null)";
        auto const child_length = child_array != nullptr ? child_array->length : -1;
        std::cout << "    [" << std::setw(2) << i << "] " << std::left << std::setw(20)
                  << child_name << " format=" << std::setw(12) << child_format << std::right
                  << " rows=" << child_length << "\n";
    }
}

void check_cuda_device(ArrowDeviceArray const *device_array, int64_t *expected_device_id) {
    if (device_array->device_type != ARROW_DEVICE_CUDA) {
        throw std::runtime_error("ArrowDeviceArray device_type is not CUDA");
    }
    if (*expected_device_id == -1) {
        *expected_device_id = device_array->device_id;
    } else if (device_array->device_id != *expected_device_id) {
        throw std::runtime_error("stream batch CUDA device changed");
    }
}

void validate_device_array(char const *label,
                           ArrowSchema const *schema,
                           ArrowDeviceArray *device_array,
                           validate_array_fn validate_array,
                           int64_t *expected_device_id) {
    check_cuda_device(device_array, expected_device_id);

    if (device_array->array.n_children != schema->n_children) {
        throw std::runtime_error("schema and array child counts differ");
    }
    if (schema->n_children != 0 &&
        (schema->children == nullptr || device_array->array.children == nullptr)) {
        throw std::runtime_error("schema or array children pointer is null");
    }

    print_device_array_summary(label, schema, device_array);

    auto table = cudf::from_arrow_device(schema, device_array);
    std::cout << "  cuDF import: ok\n";
    validate_imported_table(*table, schema, validate_array);
    std::cout << "  host Arrow round-trip: ok\n";
}

void check_stream_result(ArrowDeviceArrayStream *stream, char const *operation, int code) {
    if (code == 0) {
        return;
    }

    auto message = std::string("ArrowDeviceArrayStream::") + operation + " returned " + std::to_string(code);
    if (stream != nullptr && stream->get_last_error != nullptr) {
        auto const *last_error = stream->get_last_error(stream);
        if (last_error != nullptr) {
            message += ": ";
            message += last_error;
        }
    }
    throw std::runtime_error(message);
}

int run_check(const char *library_path) {
    try {
        SharedLibrary library(library_path);
        OwnedSchema schema;
        OwnedDeviceArray device_array;

        auto export_array = library.symbol<export_array_fn>("export_array");
        auto validate_array = library.symbol<validate_array_fn>("validate_array");

        std::cout << "Export: ArrowDeviceArray\n";
        auto result = export_array(schema.get(), device_array.get());
        if (result != 0) {
            throw std::runtime_error("export_array returned error code " + std::to_string(result));
        }

        int64_t device_id = -1;
        validate_device_array("ArrowDeviceArray", schema.get(), device_array.get(), validate_array, &device_id);
        std::cout << "Result: ok\n";
        return 0;
    } catch (std::exception const &e) {
        std::cerr << "Error: Failed to validate Arrow device export with cuDF: " << e.what() << "\n";
        return 1;
    }
}

int run_check_stream(const char *library_path) {
    try {
        SharedLibrary library(library_path);
        OwnedDeviceArrayStream stream;
        OwnedSchema schema;

        auto export_device_stream = library.symbol<export_device_stream_fn>("export_device_stream");
        auto validate_array = library.symbol<validate_array_fn>("validate_array");

        std::cout << "Export: ArrowDeviceArrayStream\n";
        auto result = export_device_stream(stream.get());
        if (result != 0) {
            throw std::runtime_error("export_device_stream returned error code " + std::to_string(result));
        }
        if (stream.value.get_schema == nullptr || stream.value.get_next == nullptr || stream.value.release == nullptr) {
            throw std::runtime_error("stream is missing required callbacks");
        }
        if (stream.value.device_type != ARROW_DEVICE_CUDA) {
            throw std::runtime_error("stream device_type is not CUDA");
        }

        check_stream_result(stream.get(), "get_schema", stream.value.get_schema(stream.get(), schema.get()));
        std::cout << "Stream schema\n"
                  << "  format: " << (schema.value.format ? schema.value.format : "(null)")
                  << ", children: " << schema.value.n_children << "\n";

        int64_t batch_count = 0;
        int64_t device_id = -1;
        while (true) {
            OwnedDeviceArray batch;
            check_stream_result(stream.get(), "get_next", stream.value.get_next(stream.get(), batch.get()));
            if (!batch.is_live()) {
                std::cout << "End of stream: " << batch_count << " batch(es)\n";
                break;
            }

            auto label = std::string("Stream batch ") + std::to_string(batch_count);
            validate_device_array(label.c_str(), schema.get(), batch.get(), validate_array, &device_id);
            ++batch_count;
        }

        if (batch_count == 0) {
            throw std::runtime_error("stream produced no batches");
        }

        OwnedDeviceArray second_eos;
        check_stream_result(stream.get(), "get_next after EOS", stream.value.get_next(stream.get(), second_eos.get()));
        if (second_eos.is_live()) {
            throw std::runtime_error("get_next after EOS returned a live batch");
        }

        std::cout << "Result: ok\n";
        return 0;
    } catch (std::exception const &e) {
        std::cerr << "Error: Failed to validate Arrow device stream export with cuDF: " << e.what() << "\n";
        return 1;
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    auto command = std::string{argv[1]};
    if (command == "--help" || command == "-h") {
        print_usage(argv[0]);
        return 0;
    }
    if (argc < 3) {
        std::cerr << "Error: " << command << " command requires a library path\n";
        print_usage(argv[0]);
        return 1;
    }
    if (command == "check") {
        return run_check(argv[2]);
    }
    if (command == "check-stream") {
        return run_check_stream(argv[2]);
    }

    std::cerr << "Error: Unknown command " << command << "\n";
    print_usage(argv[0]);
    return 1;
}
