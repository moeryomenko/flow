<div align="center">

# 🌊 Flow

### Modern C++ Asynchronous Execution Library

![License](https://img.shields.io/badge/license-MIT%2FApache--2.0-blue.svg)
[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.23+-064F8C.svg)](https://cmake.org/)

*A comprehensive implementation of the C++ asynchronous execution framework based on P2300*

[Features](#-features) • [Quick Start](#-quick-start) • [Examples](#-examples) • [Documentation](#-documentation) • [Contributing](#-contributing)

</div>

---

## ⚠️ Research Project Notice

> This is an **experimental research project** exploring P2300 with member customization points (P2855) and non-blocking support (P3669). It is **not ready for production use** and is intended for study, experimentation, and academic research purposes only.

---

## 📖 Table of Contents

- [About](#-about)
- [Features](#-features)
- [Technology Stack](#-technology-stack)
- [Quick Start](#-quick-start)
  - [Requirements](#requirements)
  - [Installation](#installation)
  - [Building](#building)
- [Examples](#-examples)
- [Project Structure](#-project-structure)
- [Core Concepts](#-core-concepts)
- [Algorithms & API](#-algorithms--api)
- [Advanced Usage](#-advanced-usage)
- [Testing](#-testing)
- [Performance](#-performance)
- [FAQ](#-faq)
- [Roadmap](#-roadmap)
- [Contributing](#-contributing)
- [License](#-license)
- [References](#-references)
- [Author](#-author)

---

## 🎯 About

**Flow** is a header-only C++23 library that implements the sender/receiver model for structured asynchronous programming. It provides a type-safe, composable, and zero-overhead abstraction for managing asynchronous operations, based on the upcoming C++ standard proposal P2300.

### What is Flow?

Flow enables you to write asynchronous code that is:
- **Type-safe**: Compile-time verification of sender/receiver connections
- **Composable**: Chain operations using intuitive pipeline syntax
- **Efficient**: Zero-overhead abstractions with aggressive optimization
- **Flexible**: Support for various execution contexts (thread pools, event loops, inline)

### Who is it for?

- 🎓 **Researchers** exploring modern C++ asynchronous patterns
- 📚 **Students** learning about sender/receiver execution model
- 🔬 **Experimenters** investigating P2300 and related proposals
- 🤝 **Contributors** to C++ standardization discussions

### Why Flow?

Traditional callback-based or future-based async programming can be complex and error-prone. Flow provides:
- Clear separation of work description, scheduling, and result handling
- Structured cancellation and error propagation
- Rich set of composable algorithms
- Member function customization points for cleaner syntax

---

## ✨ Features

- **🚀 Modern C++23** - Leverages concepts, requires expressions, and latest language features
- **🎯 Type-safe** - Compile-time checked sender/receiver connections with full type inference
- **⚡ Zero-overhead** - Header-only library with excellent optimization potential
- **🔧 Composable** - Rich set of algorithms for building complex async workflows
- **🧵 Thread Pool** - Built-in thread pool and run loop schedulers
- **📊 Algorithms** - `bulk`, `when_all`, `then`, `upon_error`, `upon_stopped`, and more
- **✨ Clean API** - Member function customization points (P2855) for clarity
- **🚫 Non-blocking Support** - P3669 concurrent schedulers for lock-free integration
- **📦 C++ Modules Ready** - Future-proof module support (experimental)
- **🧪 Comprehensive Tests** - Extensive test suite with 19+ test categories
- **📝 Well Documented** - Clear examples and API documentation

---

## 🛠 Technology Stack

### Core Technologies

- **Language**: C++23
- **Build System**: CMake 3.23+
- **Threading**: Standard C++ threads (`<thread>`, `<mutex>`, `<condition_variable>`)
- **Testing Framework**: [Boost.UT v2.1.0](https://github.com/boost-ext/ut)
- **Code Quality**:
  - `clang-format` - Code formatting
  - `clang-tidy` - Static analysis

### Dependencies

**Runtime**: None (header-only)  
**Build-time**: CMake, C++23 compiler  
**Test-time**: Boost.UT (automatically fetched via CMake)

---

## 🚀 Quick Start

### Requirements

- **C++23 compatible compiler** (GCC 11+, Clang 14+, or MSVC 2022+)
- **CMake 3.23** or higher
- **Git** (for cloning and FetchContent)

### Installation

#### Option 1: Using CMake FetchContent (Recommended)

Add to your `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare(
    flow
    GIT_REPOSITORY https://github.com/moeryomenko/flow.git
    GIT_TAG        main
)

FetchContent_MakeAvailable(flow)

target_link_libraries(your_target PRIVATE flow::flow)
```

#### Option 2: Manual Installation

```bash
# Clone the repository
git clone https://github.com/moeryomenko/flow.git
cd flow

# Build and install
mkdir build && cd build
cmake ..
cmake --build .
sudo cmake --install .
```

#### Option 3: System-wide Installation

```bash
git clone https://github.com/moeryomenko/flow.git
cd flow
mkdir build && cd build
cmake .. -DFLOW_BUILD_EXAMPLES=OFF -DFLOW_BUILD_TESTS=OFF
sudo cmake --install .
```

Then in your project:

```cmake
find_package(flow REQUIRED)
target_link_libraries(your_target PRIVATE flow::flow)
```

### Building

#### Build with Examples

```bash
mkdir build && cd build
cmake .. -DFLOW_BUILD_EXAMPLES=ON
cmake --build .

# Run examples
./examples/hello_world
./examples/error_handling
./examples/parallel_transform
```

#### Build with Tests

```bash
mkdir build && cd build
cmake .. -DFLOW_BUILD_TESTS=ON
cmake --build .

# Run tests
ctest --output-on-failure
# or
ctest -V
```

#### CMake Build Options

| Option | Default | Description |
|--------|---------|-------------|
| `FLOW_BUILD_EXAMPLES` | `ON` | Build example applications |
| `FLOW_BUILD_TESTS` | `ON` | Build test suite (requires Boost.UT) |
| `FLOW_INSTALL` | `ON` | Generate install target |
| `FLOW_USE_MODULES` | `OFF` | Use C++20 modules (experimental) |

---

## 💡 Examples

### Hello World

```cpp
#include <flow/execution.hpp>
#include <iostream>

using namespace flow::execution;

int main() {
    // Create a thread pool with 4 threads
    thread_pool pool{4};

    // Build async work chain
    auto work = schedule(pool.get_scheduler())
        | then([] {
            std::cout << "Hello from thread pool!\n";
            return 42;
        })
        | then([](int x) {
            return x * 2;
        });

    // Execute and wait for result
    auto result = flow::this_thread::sync_wait(work);

    if (result) {
        std::cout << "Final result: " << std::get<0>(*result) << '\n';
    }

    return 0;
}
```

### Error Handling

```cpp
#include <flow/execution.hpp>
#include <iostream>
#include <stdexcept>

using namespace flow::execution;

auto risky_computation(int x) {
    return just(x)
        | then([](int val) -> int {
            if (val < 0) {
                throw std::runtime_error("Negative value not allowed!");
            }
            return val * 2;
        })
        | upon_error([](std::exception_ptr ep) -> int {
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                std::cout << "Error caught: " << e.what() << '\n';
                return -1;  // Fallback value
            }
        });
}

int main() {
    auto result = flow::this_thread::sync_wait(risky_computation(5));
    if (result) {
        std::cout << "Result: " << std::get<0>(*result) << '\n';
    }
    return 0;
}
```

### Parallel Transform

```cpp
#include <flow/execution.hpp>
#include <iostream>
#include <vector>
#include <numeric>

using namespace flow::execution;

int main() {
    thread_pool pool{4};

    // Create input data
    std::vector<int> input(1000);
    std::iota(input.begin(), input.end(), 1);  // 1, 2, 3, ..., 1000

    // Parallel computation
    auto sender = schedule(pool.get_scheduler())
        | bulk(input.size(), [&](size_t i) {
            input[i] = input[i] * input[i];  // square each element
        });

    flow::this_thread::sync_wait(std::move(sender));

    // Sum results
    int sum = std::accumulate(input.begin(), input.end(), 0);
    std::cout << "Sum of squares: " << sum << '\n';

    return 0;
}
```

---

## 📁 Project Structure

```
flow/
├── CMakeLists.txt              # Main build configuration
├── LICENSE-APACHE              # Apache 2.0 License
├── LICENSE-MIT                 # MIT License
├── README.md                   # This file
│
├── .clang-format               # Code formatting rules
├── .clang-tidy                 # Static analysis configuration
│
├── cmake/
│   └── flowConfig.cmake.in     # CMake package configuration
│
├── include/
│   └── flow/
│       ├── execution.hpp       # Main header (includes all)
│       └── execution/
│           ├── execution.hpp        # Core concepts and queries
│           ├── sender.hpp          # Sender concepts
│           ├── receiver.hpp        # Receiver concepts
│           ├── scheduler.hpp       # Scheduler concepts
│           ├── operation_state.hpp # Operation state concepts
│           ├── queries.hpp         # Query customization points
│           ├── env.hpp             # Execution environments
│           ├── completion_signatures.hpp  # Completion signatures
│           ├── factories.hpp       # Sender factories (just, just_error, etc.)
│           ├── adaptors.hpp        # Sender adaptors (then, upon_error, etc.)
│           ├── algorithms.hpp      # Advanced algorithms (bulk, when_all, etc.)
│           ├── schedulers.hpp      # Standard scheduler implementations
│           ├── sync_wait.hpp       # Synchronous execution utilities
│           ├── type_list.hpp       # Type manipulation utilities
│           └── utils.hpp           # General utilities
│
├── examples/
│   ├── CMakeLists.txt
│   ├── hello_world.cpp         # Basic usage example
│   ├── error_handling.cpp      # Error handling patterns
│   └── parallel_transform.cpp  # Parallel computation example
│
└── tests/
    ├── CMakeLists.txt
    ├── basic_test.cpp                  # Basic functionality tests
    ├── concept_tests.cpp               # Concept validation tests
    ├── customization_point_tests.cpp   # CPO tests
    ├── factory_tests.cpp               # Sender factory tests
    ├── adaptor_tests.cpp               # Sender adaptor tests
    ├── scheduler_tests.cpp             # Scheduler tests
    ├── consumer_tests.cpp              # Consumer (sync_wait) tests
    ├── error_handling_tests.cpp        # Error propagation tests
    ├── resource_management_tests.cpp   # Resource lifecycle tests
    ├── race_condition_tests.cpp        # Thread safety tests
    ├── performance_tests.cpp           # Performance benchmarks
    ├── integration_tests.cpp           # End-to-end tests
    ├── interoperability_tests.cpp      # Cross-component tests
    ├── edge_case_tests.cpp             # Corner case handling
    ├── module_tests.cpp                # C++ module tests
    ├── platform_tests.cpp              # Platform-specific tests
    ├── compilation_tests.cpp           # Compile-time tests
    ├── advanced_features_test.cpp      # Advanced feature tests
    └── limitations_resolved_test.cpp   # Known issue verification
```

---

## 🔍 Core Concepts

### Senders

Senders represent asynchronous work that can be started later. They describe what to compute, not when or where.

```cpp
// Factory senders
auto s1 = just(42);                          // Immediately produces value
auto s2 = just_error(std::runtime_error("")); // Immediately produces error
auto s3 = just_stopped();                     // Immediately signals cancellation

// Scheduled work
auto s4 = schedule(scheduler);                // Work on a scheduler
```

### Receivers

Receivers handle the completion of asynchronous operations through three channels:

```cpp
struct my_receiver {
    using receiver_concept = flow::execution::receiver_t;

    void set_value(int x) && noexcept {
        std::cout << "Success: " << x << '\n';
    }

    void set_error(std::exception_ptr ep) && noexcept {
        std::cout << "Error occurred\n";
    }

    void set_stopped() && noexcept {
        std::cout << "Operation cancelled\n";
    }
};
```

### Schedulers

Schedulers control where and when work executes:

```cpp
// Inline execution (no threading, immediate)
inline_scheduler inline_sch;

// Run loop (single-threaded event loop)
run_loop loop;
auto loop_sch = loop.get_scheduler();

// Thread pool (parallel execution across N threads)
thread_pool pool{8};
auto pool_sch = pool.get_scheduler();
```

### Operation States

Operation states represent running asynchronous operations:

```cpp
auto sender = just(42);
auto receiver = my_receiver{};
auto state = sender.connect(receiver);  // Create operation state
state.start();                          // Begin execution
```

---

## 🎨 Algorithms & API

### Sender Factories

Create senders from values or states:

| Function | Description |
|----------|-------------|
| `just(values...)` | Creates sender that immediately produces values |
| `just_error(error)` | Creates sender that immediately produces error |
| `just_stopped()` | Creates sender that immediately signals cancellation |
| `schedule(scheduler)` | Creates sender that schedules work on a scheduler |

### Sender Adaptors

Transform and compose senders:

| Adaptor | Description |
|---------|-------------|
| `then(fn)` | Transform successful values |
| `upon_error(fn)` | Handle errors and recover |
| `upon_stopped(fn)` | Handle cancellation |
| `let_value(fn)` | Chain dependent async operations |
| `let_error(fn)` | Chain error recovery operations |

### Algorithms

Advanced sender operations:

| Algorithm | Description |
|-----------|-------------|
| `bulk(count, fn)` | Execute function for range [0, count) in parallel |
| `when_all(senders...)` | Wait for all senders to complete |
| `transfer(scheduler)` | Move execution to different scheduler |

### Pipeline Syntax

Chain operations using `operator|`:

```cpp
auto result = schedule(pool.get_scheduler())
    | then([](auto... args) { /* transform */ })
    | upon_error([](auto ep) { /* handle error */ })
    | bulk(100, [](size_t i) { /* parallel work */ });
```

---

## 🔧 Advanced Usage

### Custom Scheduler

Implement your own execution context:

```cpp
class my_scheduler {
public:
    using scheduler_concept = flow::execution::scheduler_t;

    auto schedule() const {
        return my_sender{/* ... */};
    }

    auto query(flow::execution::get_forward_progress_guarantee_t) const {
        return flow::execution::forward_progress_guarantee::parallel;
    }

    bool operator==(const my_scheduler&) const = default;
};
```

### Custom Sender

Create specialized async operations:

```cpp
struct my_sender {
    using sender_concept = flow::execution::sender_t;

    template<class Env>
    auto get_completion_signatures(Env&&) const {
        return flow::execution::completion_signatures<
            flow::execution::set_value_t(int)
        >{};
    }

    template<flow::execution::receiver R>
    auto connect(this my_sender, R&& r) {
        return my_operation{std::forward<R>(r)};
    }
};
```

### Cancellation Support

Implement cancellation-aware operations:

```cpp
auto cancellable_work = schedule(scheduler)
    | then([](stop_token token) {
        while (!token.stop_requested()) {
            // Do work
        }
        return result;
    });
```

---

## 🧪 Testing

Flow includes a comprehensive test suite with 19+ test categories:

```bash
# Build with tests
cmake .. -DFLOW_BUILD_TESTS=ON
cmake --build .

# Run all tests
ctest

# Run specific test
./tests/scheduler_tests

# Run tests with verbose output
ctest -V
```

### Test Categories

- **Basic Tests**: Core functionality validation
- **Concept Tests**: C++23 concept compliance
- **Customization Point Tests**: CPO behavior verification
- **Factory Tests**: Sender factory correctness
- **Adaptor Tests**: Sender adaptor chains
- **Scheduler Tests**: Execution context behavior
- **Consumer Tests**: `sync_wait` and blocking operations
- **Error Handling Tests**: Exception propagation
- **Resource Management Tests**: RAII and lifecycle
- **Race Condition Tests**: Thread safety validation
- **Performance Tests**: Benchmarks and optimization checks
- **Integration Tests**: End-to-end scenarios
- **Edge Case Tests**: Corner cases and boundaries
- **Platform Tests**: OS-specific behavior

---

## ⚡ Performance

Flow is designed for **zero-overhead abstraction**:

### Design Principles

- ✅ **Header-only template library** - No runtime library overhead
- ✅ **Aggressive inlining** - All core operations marked `inline`
- ✅ **Move-only semantics** - Where appropriate to avoid copies
- ✅ **Minimal dynamic allocation** - Stack-based operation states when possible
- ✅ **Lock-free algorithms** - Where applicable (P3669 support)
- ✅ **Compile-time type checking** - No runtime type overhead

### Optimization Tips

1. **Use `-O3` and LTO** for production builds
2. **Profile-guided optimization (PGO)** can help with hot paths
3. **Avoid unnecessary `sync_wait`** - prefer async composition
4. **Reuse schedulers** - thread pool creation is expensive
5. **Batch operations** with `bulk` for better cache locality

---

## ❓ FAQ

### Is Flow production-ready?

**No.** Flow is an experimental research project. It should **not** be used in production systems or critical applications. Use it for:
- Learning and education
- Experimenting with P2300 concepts
- Academic research
- Contributing to standardization discussions

### Why not use existing async libraries?

Flow specifically explores:
- **P2855**: Member customization points (different from tag_invoke)
- **P3669**: Non-blocking scheduler support
- **Modern C++23**: Latest language features

If you need a production-ready solution, consider:
- [libunifex](https://github.com/facebookexperimental/libunifex) (Meta/Facebook)
- [stdexec](https://github.com/NVIDIA/stdexec) (NVIDIA)

### Can I use this with C++20?

**No.** Flow requires C++23 features:
- Deducing `this`
- Extended `constexpr`
- Concepts improvements

### How does Flow differ from futures/promises?

| Feature | Futures/Promises | Flow (Senders/Receivers) |
|---------|------------------|--------------------------|
| Composition | Limited chaining | Rich algorithm library |
| Type Safety | Runtime errors possible | Compile-time checked |
| Cancellation | Often missing | First-class support |
| Custom execution | Difficult | Scheduler abstraction |
| Error handling | Exception-based | Three-channel (value/error/stop) |
| Lazy execution | Eager by default | Lazy by design |

### What about coroutines?

Flow senders can be used **with** coroutines! The sender/receiver model complements coroutines:
- Senders for pipeline-style async chains
- Coroutines for sequential async control flow
- `co_await sender` integration possible

### How can I contribute?

See [Contributing](#-contributing) section below!

---

## 🗺 Roadmap

### Current Status (v1.0.0)

- ✅ Core sender/receiver model
- ✅ Standard schedulers (inline, run_loop, thread_pool)
- ✅ Essential algorithms (then, upon_error, bulk, when_all)
- ✅ Comprehensive test suite
- ✅ Example programs

### Future Explorations

- 🔄 **C++23 Modules** - Native module support
- 🔄 **More algorithms** - `when_any`, `repeat`, `retry`, etc.
- 🔄 **Coroutine integration** - `co_await` sender support
- 🔄 **I/O schedulers** - Async I/O primitives
- 🔄 **Timer support** - Scheduled/delayed execution
- 🔄 **Performance benchmarks** - Comparison with other implementations

**Note**: As a research project, priorities may shift based on standardization developments.

---

## 🤝 Contributing

Contributions are welcome! This is a research project, so experimental ideas are encouraged.

### How to Contribute

1. **Fork** the repository
2. **Create** a feature branch (`git checkout -b feature/amazing-feature`)
3. **Commit** your changes using [Conventional Commits](https://www.conventionalcommits.org/)
4. **Push** to your branch (`git push origin feature/amazing-feature`)
5. **Open** a Pull Request

### Contribution Ideas

- 🐛 **Bug reports** - Found an issue? Let us know!
- 💡 **Feature requests** - Ideas for new algorithms or improvements
- 📝 **Documentation** - Improve examples, comments, or README
- 🧪 **Tests** - Add test cases or improve coverage
- ⚡ **Performance** - Optimization suggestions or benchmarks
- 🔬 **Research** - Share findings from using Flow in your studies

### Code Style

- Follow the `.clang-format` configuration
- Run `clang-tidy` before submitting
- Add tests for new features
- Update documentation as needed

```bash
# Format code
cmake --build build --target format

# Run linter
cmake --build build --target lint

# Auto-fix lint issues
cmake --build build --target lint-fix
```

### Discussion

- **GitHub Issues**: [Report bugs or ask questions](https://github.com/moeryomenko/flow/issues)
- **GitHub Discussions**: [Share ideas and research findings](https://github.com/moeryomenko/flow/discussions)

**Note**: As an experimental project, breaking changes may occur. We'll try to communicate these clearly.

---

## 📄 License

This project is dual-licensed under your choice of:

- **MIT License** - See [LICENSE-MIT](./LICENSE-MIT)
- **Apache License 2.0** - See [LICENSE-APACHE](./LICENSE-APACHE)

You may use this project under the terms of either license.

---

## 📚 References

### C++ Standards Proposals

- [P2300R10: `std::execution`](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2300r10.html) - Core sender/receiver model
- [P2855: Member customization points](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2855r1.html) - Member function CPOs
- [P3669: Non-blocking support](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3669r0.html) - Concurrent schedulers

### Related Projects

- [libunifex](https://github.com/facebookexperimental/libunifex) - Meta's production implementation
- [stdexec](https://github.com/NVIDIA/stdexec) - NVIDIA's reference implementation
- [Boost.Asio](https://www.boost.org/doc/libs/1_82_0/doc/html/boost_asio.html) - Mature async I/O library

### Learning Resources

- [Structured Concurrency](https://vorpus.org/blog/notes-on-structured-concurrency-or-go-statement-considered-harmful/) - Foundational concepts
- [C++23 Features](https://en.cppreference.com/w/cpp/23) - Language features used

---

## 👨‍💻 Author

**Maxim Eryomenko**

- GitHub: [@moeryomenko](https://github.com/moeryomenko)
- Email: maxim_eryomenko@rambler.ru

---

## 🙏 Acknowledgments

- The C++ Standards Committee for P2300, P2855, and P3669
- [Boost.UT](https://github.com/boost-ext/ut) for the excellent testing framework
- The C++ community for feedback and discussions

---

<div align="center">

**⭐ Star this repo if you find it useful for your research or learning!**

Made with ❤️ for the C++ community

</div>