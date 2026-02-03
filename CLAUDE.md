# cudf-test-harness

This test harness is meant to help test the ability for native code to
send batches of data to cuDF over the Arrow C Device Data Interface.

## Usage

This is a single binary that has the ability to launch several interface types.

```
cudf-test-harness check some-library.so
```

This will `dlopen` the provided `some-library.so` file, and load the following symbols from it:

* `int export_array(ArrowSchema* schema, ArrowDeviceArray* array)`: A function that exports an array owned by the shared library into the given pointer,
  returning a non-zero error code on failure


The test harness will convert the data from the array using the `from_arrow_device_column` function available on cuDF,
then it will print several summary stats about the column for display.

At the end, it will call release on the ArrowDeviceArray using its embedded release pointer.

If all of this succeeds, the binary returns a zero exit code. If any step fails, an error should be printed to stderr
and a non-zero exit code should be returned.


## Building

Expect that this is being build in a CMake project, inside of a conda environment where the following has been run:

```
conda install -c rapidsai -c conda-forge -c nvidia rapidsai::libcudf
```

The binary should build in debug mode with full debug symbols by default.

## Links

* [Arrow cuDF interop docs](https://docs.rapids.ai/api/cudf/latest/libcudf_docs/api_docs/interop_arrow/)
* [Arrow C Device Data Interface docs](https://arrow.apache.org/docs/format/CDeviceDataInterface.html)
