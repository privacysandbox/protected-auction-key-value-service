# Package public/data_loading

This package provides libraries for manipulating data that KV server consumes.

The schema is defined in [data_loading.fbs](/public/data_loading/data_loading.fbs)

The C++ library generated from the schema is provided for referencing convenience at
[data_loading_generated.h](/public/data_loading/data_loading_generated.h). Since it is for
referencing only and not really used by the server build, there is a risk that it may go out of
date. To get the latest library, run from the repo root: builders/tools/bazel-debian build
//public/data_loading:data_loading_fbs and access the file at
`bazel-bin/public/data_loading/data_loading_generated.h`
