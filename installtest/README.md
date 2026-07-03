# Test Project against installed `beman.optional`

To test from the root of the source tree
```sh
cmake --workflow --preset gcc-release
cmake --install build/gcc-release --prefix .install --component beman.optional
cmake -S installtest -B installtest/build
cmake --build  installtest/build --target test
```
