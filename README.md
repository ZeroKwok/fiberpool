# fiberpool

fiberpool is a C++ library based on Boost.Fibers, providing a convenient wrapper for managing asynchronous tasks within applications, optimizing the execution of time-consuming and fragmented tasks across multiple threads.

## Prerequisites

- C++17 compiler
- Boost 1.60 or later

## Installation

To use fiberpool in your project, follow these steps:

1. Clone the Repository:

```shell
git clone https://github.com/ZeroKwok/fiberpool.git
```

2. Build and Install:

```shell
cd fiberpool
mkdir build && cd build
cmake ..
cmake --build .
cmake --install .
```

3. Link to Your Project

Once installed, you can link fiberpool to your CMake project by adding the following lines to your CMakeLists.txt:

```cmake
find_package(fiberpool REQUIRED)
target_link_libraries(your_project_name PRIVATE fiberpool)
```

4. Include Headers

In your C++ code, include fiberpool headers as follows:

```cpp
#include <fiber_pool.hpp>
```

5. Enjoy Using fiberpool!

## Detail

For more detailed usage and examples, refer to the example directory.