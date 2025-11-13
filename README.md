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

> This is an **experimental research project** exploring P2300 with member customization points (P2855), non-blocking support (P3669R2), async scopes (P3149), and structured concurrency (P3296). It is **not ready for production use** and is intended for study, experimentation, and academic research purposes only.

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

**Flow** is a header-only C++23 library that implements the sender/receiver model for structured asynchronous programming. It provides a type-safe, composable, and zero-overhead abstraction for managing asynchronous operations, based on the upcoming C++ standard proposal P2300, with additional support for async scopes (P3149), structured concurrency (P3296), and bulk algorithm improvements (P3481R5).

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
- Comprehensive cancellation support with stop tokens and active cancellation
- Structured cancellation and error propagation
- Rich set of composable algorithms including racing operations
- Member function customization points for cleaner syntax
- Async scopes for managing the lifetime of concurrent operations
- Structured concurrency patterns for safe fire-and-forget operations

---

## ✨ Features

- **🚀 Modern C++23** - Leverages concepts, requires expressions, and latest language features
- **🎯 Type-safe** - Compile-time checked sender/receiver connections with full type inference
- **⚡ Zero-overhead** - Header-only library with excellent optimization potential
- **🔧 Composable** - Rich set of algorithms for building complex async workflows
- **🧵 Thread Pool** - Built-in thread pool and run loop schedulers
- **📊 Algorithms** - `bulk`, `when_all`, `when_any`, `then`, `upon_error`, `upon_stopped`, and more
- **🎯 Execution Policies** - P3481R5 support with `seq`, `par`, `par_unseq`, `unseq` for bulk operations
- **🔀 Bulk Variants** - `bulk`, `bulk_chunked`, `bulk_unchunked` for different parallelism patterns
- **🔄 Retry Mechanisms** - `retry`, `retry_n`, `retry_if`, `retry_with_backoff` for resilient error handling
- **✨ Clean API** - Member function customization points (P2855) for clarity
- **🚫 Non-blocking Support** - P3669R2 `try_scheduler` for signal-safe, lock-free operations
- **📦 Async Scopes** - P3149 async scope support with `counting_scope` and `simple_counting_scope`
- **🎯 Structured Concurrency** - P3296 `let_async_scope` for managing concurrent operations
- **🛑 Stop Token Support** - Comprehensive cancellation infrastructure with `inplace_stop_token`
- **📦 C++23 Modules** - Experimental module support via CMake's FILE_SET CXX_MODULES
- **🧪 Comprehensive Tests** - Extensive test suite with 24+ test categories
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
| `FLOW_USE_MODULES` | `OFF` | Use C++23 modules (experimental, requires CMake 3.28+) |

#### Using C++ Modules (Experimental)

Flow supports C++23 modules through CMake's experimental `FILE_SET CXX_MODULES` feature:

```bash
# On macOS with Homebrew Clang (Apple Clang doesn't support module scanning)
brew install llvm ninja
export PATH="/opt/homebrew/opt/llvm/bin:$PATH"

mkdir build && cd build
cmake .. -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-isysroot $(xcrun --show-sdk-path) -stdlib=libc++" \
  -DCMAKE_EXE_LINKER_FLAGS="-L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++" \
  -DFLOW_USE_MODULES=ON
cmake --build .
```

```bash
# On Linux with Clang 16+
mkdir build && cd build
cmake .. -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++-16 \
  -DFLOW_USE_MODULES=ON
cmake --build .
```

When using modules, import Flow instead of including headers:

```cpp
// Traditional header-only mode
#include <flow/execution.hpp>

// C++ modules mode (when FLOW_USE_MODULES=ON)
import flow;

using namespace flow::execution;

int main() {
    thread_pool pool{4};
    auto work = schedule(pool.get_scheduler()) | then([] {
        return 42;
    });
    auto result = flow::this_thread::sync_wait(work);
    return 0;
}
```

**Requirements for C++ modules:**
- CMake 3.28 or higher
- Ninja build system (recommended)
- Compiler support:
  - Clang 16+ with libc++ (not Apple Clang - use Homebrew llvm)
  - GCC 14+ (experimental)
  - MSVC 19.36+ (Visual Studio 2022 17.6+)

**Important Notes:**
- Apple Clang does not support CMake's module scanning - use `brew install llvm` and set `CMAKE_CXX_COMPILER` to Homebrew Clang
- C++ modules support is experimental and may have limited tooling support
- Module scanning requires additional compile time

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

### Retry with Backoff

```cpp
#include <flow/execution.hpp>
#include <iostream>
#include <stdexcept>

using namespace flow::execution;

auto unreliable_api_call(int& attempt) {
    return just()
        | then([&attempt] {
            attempt++;
            if (attempt < 3) {
                throw std::runtime_error("Temporary network error");
            }
            return "Success!";
        });
}

int main() {
    thread_pool pool{2};
    int attempt = 0;

    // Retry up to 5 times with exponential backoff
    auto result = flow::this_thread::sync_wait(
        unreliable_api_call(attempt)
        | retry_with_backoff(
            pool.get_scheduler(),
            std::chrono::milliseconds(100),  // initial delay
            std::chrono::milliseconds(2000), // max delay
            2.0,                             // multiplier
            5                                // max attempts
        )
    );

    if (result) {
        std::cout << "Result: " << std::get<0>(*result) << '\n';
        std::cout << "Took " << attempt << " attempts\n";
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

    // Parallel computation with execution policy
    auto sender = schedule(pool.get_scheduler())
        | bulk(par, input.size(), [&](size_t i) {
            input[i] = input[i] * input[i];  // square each element
        });

    flow::this_thread::sync_wait(std::move(sender));

    // Sum results
    int sum = std::accumulate(input.begin(), input.end(), 0);
    std::cout << "Sum of squares: " << sum << '\n';

    return 0;
}
```

### Structured Concurrency with Async Scopes

```cpp
#include <flow/execution.hpp>
#include <iostream>

using namespace flow::execution;

int main() {
    thread_pool pool{4};

    auto result = flow::this_thread::sync_wait(
        just() | let_async_scope([&](auto scope_token) {
            // Spawn multiple concurrent tasks
            for (int i = 0; i < 10; ++i) {
                spawn(
                    schedule(pool.get_scheduler()) | then([i] {
                        std::cout << "Task " << i << " executing\n";
                        return i * i;
                    }),
                    scope_token
                );
            }
            // All spawned work automatically completes before continuing
        })
        | then([] {
            std::cout << "All concurrent work completed!\n";
        })
    );

    return 0;
}
```

### Racing Operations with when_any

```cpp
#include <flow/execution.hpp>
#include <iostream>
#include <chrono>
#include <thread>

using namespace flow::execution;

int main() {
    thread_pool pool{4};

    // Create three competing tasks
    auto fast_task = schedule(pool.get_scheduler()) | then([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        return "fast";
    });

    auto medium_task = schedule(pool.get_scheduler()) | then([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        return "medium";
    });

    auto slow_task = schedule(pool.get_scheduler()) | then([] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return "slow";
    });

    // Race them - first to complete wins, others are cancelled
    auto race = when_any(std::move(fast_task), std::move(medium_task), std::move(slow_task));
    auto result = flow::this_thread::sync_wait(std::move(race));

    if (result) {
        auto [winner] = *result;
        std::cout << "Winner: " << winner << "\n";
    }

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
│           ├── try_scheduler.hpp   # Non-blocking scheduler support (P3669R2)
│           ├── operation_state.hpp # Operation state concepts
│           ├── queries.hpp         # Query customization points
│           ├── env.hpp             # Execution environments
│           ├── completion_signatures.hpp  # Completion signatures
│           ├── execution_policy.hpp       # Execution policies (P3481R5)
│           ├── factories.hpp       # Sender factories (just, just_error, etc.)
│           ├── adaptors.hpp        # Sender adaptors (then, upon_error, etc.)
│           ├── algorithms.hpp      # Advanced algorithms (bulk, when_all, when_any, etc.)
│           ├── retry.hpp           # Retry mechanisms for error recovery
│           ├── async_scope.hpp     # Async scope support (P3149, P3296)
│           ├── schedulers.hpp      # Standard scheduler implementations
│           ├── lock_free_queue.hpp # Lock-free queue for non-blocking operations
│           ├── stop_token.hpp      # Stop token and cancellation support
│           ├── sync_wait.hpp       # Synchronous execution utilities
│           ├── type_list.hpp       # Type manipulation utilities
│           └── utils.hpp           # General utilities
│
├── examples/
│   ├── CMakeLists.txt
│   ├── hello_world.cpp         # Basic usage example
│   ├── error_handling.cpp      # Error handling patterns
│   ├── parallel_transform.cpp  # Parallel computation example
│   ├── try_schedule_example.cpp # Non-blocking operations (P3669R2)
│   └── when_any_example.cpp    # Racing operations example
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
    ├── limitations_resolved_test.cpp   # Known issue verification
    ├── async_scope_basic_tests.cpp     # Basic async scope tests (P3149)
    ├── async_scope_comprehensive_tests.cpp  # Comprehensive scope tests
    ├── let_async_scope_tests.cpp       # let_async_scope tests (P3296)
    ├── when_any_tests.cpp              # when_any algorithm tests
    ├── retry_tests.cpp                 # retry algorithms tests
    ├── try_scheduler_tests.cpp         # P3669R2 non-blocking scheduler tests
    └── bulk_policy_tests.cpp           # P3481R5 bulk algorithms with execution policies
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

### Async Scopes

Async scopes manage the lifetime of asynchronous operations:

```cpp
// Simple counting scope - tracks operations
simple_counting_scope simple_scope;
auto simple_token = simple_scope.get_token();

// Counting scope - tracks operations with cancellation
counting_scope scope;
auto token = scope.get_token();

// Associate work with scope
auto work = associate(some_sender, token);

// Wait for all work to complete
flow::this_thread::sync_wait(scope.join());
```

### Scope Tokens

Scope tokens control association and wrapping of senders:

```cpp
counting_scope scope;
auto token = scope.get_token();

// Spawn fire-and-forget work
spawn(schedule(pool.get_scheduler()) | then([] {
    // Work that's tracked by scope
}), token);

// Request cancellation
scope.request_stop();
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
| `let_async_scope(fn)` | Create async scope for structured concurrency (P3296) |

### Algorithms

Advanced sender operations:

| Algorithm | Description |
|-----------|-------------|
| `bulk(policy, count, fn)` | Execute function for range [0, count) with execution policy |
| `bulk_chunked(policy, count, fn)` | Execute function with begin/end range (basis operation for chunking) |
| `bulk_unchunked(policy, count, fn)` | Execute function per iteration (one agent per iteration) |
| `when_all(senders...)` | Wait for all senders to complete, aggregating results |
| `when_any(senders...)` | Race senders, first to complete wins (with active cancellation) |
| `retry()` | Retry indefinitely on error until success |
| `retry_n(count)` | Retry up to N times on error |
| `retry_if(predicate)` | Retry only if predicate returns true for the error |
| `retry_with_backoff(...)` | Retry with exponential backoff delay |
| `transfer(scheduler)` | Move execution to different scheduler |

#### Execution Policies

Bulk algorithms support standard execution policies to control parallelism and vectorization:

| Policy | Description |
|--------|-------------|
| `seq` | Sequential execution (no parallelism) |
| `par` | Parallel execution allowed |
| `par_unseq` | Parallel and vectorized execution allowed |
| `unseq` | Vectorized execution allowed (no parallelism) |

### Async Scope Operations

Manage concurrent operation lifetimes:

| Operation | Description |
|-----------|-------------|
| `associate(sndr, token)` | Associate sender with scope token |
| `spawn(sndr, token)` | Fire-and-forget work tracked by scope |
| `spawn_future(sndr, token)` | Spawn work and get future sender |
| `scope.join()` | Wait for all associated work to complete |
| `scope.close()` | Prevent new associations |
| `scope.request_stop()` | Request cancellation (counting_scope only) |

### Pipeline Syntax

Chain operations using `operator|`:

```cpp
auto result = schedule(pool.get_scheduler())
    | then([](auto... args) { /* transform */ })
    | upon_error([](auto ep) { /* handle error */ })
    | bulk(100, [](size_t i) { /* parallel work */ });
```

---

## � Non-Blocking Operations with P3669R2

Flow implements P3669R2 non-blocking scheduler support, enabling signal-safe and truly lock-free asynchronous operations.

### Why Non-Blocking Operations?

Some execution environments require operations that **never block**:
- **Signal handlers** for asynchronous signals (POSIX signals)
- **Interrupt handlers** in embedded systems
- **Real-time contexts** with strict timing requirements
- **Lock-free algorithms** that must avoid blocking

Regular `schedule()` may block when enqueueing work (e.g., acquiring a mutex on the work queue). P3669R2 introduces `try_schedule()` which either succeeds immediately or signals `would_block_t` error without blocking.

### try_scheduler Concept

Schedulers that support non-blocking operations model the `try_scheduler` concept:

```cpp
using namespace flow::execution;

// Check if scheduler supports try_schedule
static_assert(try_scheduler<decltype(pool.get_scheduler())>);
static_assert(try_scheduler<decltype(loop.get_scheduler())>);
```

Both `run_loop` and `thread_pool` in Flow are `try_scheduler`s.

### try_schedule() Usage

Use `try_schedule()` when blocking is unacceptable:

```cpp
thread_pool pool{4};
auto sch = pool.get_scheduler();

// Non-blocking schedule
auto work = sch.try_schedule()
    | then([] {
        std::cout << "Executing without blocking!\n";
        return 42;
    })
    | upon_error([](would_block_t) {
        std::cout << "Would have blocked - queue was full\n";
        return -1;  // Fallback value
    });

auto result = flow::this_thread::sync_wait(std::move(work));
```

### Signal-Safe Example

Perfect for signal handlers where blocking operations are forbidden:

```cpp
#include <csignal>
#include <flow/execution.hpp>

thread_pool pool{4};

void signal_handler(int signum) {
    // SAFE: try_schedule() is signal-safe
    auto work = pool.get_scheduler().try_schedule()
        | then([signum] {
            // Handle signal asynchronously
            log_signal(signum);
        });

    // Fire and forget (or use async scope)
    spawn(std::move(work), scope_token);
}

int main() {
    std::signal(SIGUSR1, signal_handler);
    // ...
}
```

### Key Guarantees

P3669R2 `try_schedule()` guarantees:

- ✅ **Never blocks** - Returns immediately
- ✅ **Signal-safe** - Can be called from signal handlers
- ✅ **Lock-free** - No mutex acquisition in critical path
- ✅ **No allocation** - Uses fixed-size lock-free ring buffer
- ✅ **Error signaling** - `set_error(would_block_t{})` if queue is full

### Implementation Details

Flow's implementation uses:
- **Lock-free bounded queue** (`lock_free_bounded_queue`) with fixed capacity (1024 items)
- **MPMC ring buffer** with atomic version numbers for ABA problem prevention
- **CAS operations** for wait-free push/pop
- **Exception handling** for `std::function` move construction failures

### When to Use try_schedule

| Scenario | Use try_schedule | Use schedule |
|----------|------------------|--------------|
| Signal handlers | ✅ Yes | ❌ No |
| Interrupt handlers | ✅ Yes | ❌ No |
| Real-time contexts | ✅ Yes | ❌ No |
| Lock-free algorithms | ✅ Yes | ❌ No |
| Normal async work | Either | ✅ Preferred |

### would_block_t Error Type

When `try_schedule()` would block, it signals an error of type `would_block_t`:

```cpp
auto work = sch.try_schedule()
    | then([] { return 42; })
    | upon_error([](would_block_t) {
        // Queue was full, handle gracefully
        return fallback_value;
    })
    | upon_error([](std::exception_ptr ep) {
        // Handle other errors
        return error_value;
    });
```

### Complete Example

See [`examples/try_schedule_example.cpp`](examples/try_schedule_example.cpp) for a comprehensive demonstration of:
- Basic `try_schedule()` usage
- Composing with `then()`
- Multiple concurrent non-blocking operations
- Checking `try_scheduler` support at compile time

---

## 🔄 Retry Mechanisms for Resilient Operations

Flow provides comprehensive retry mechanisms to handle transient failures and build resilient asynchronous applications. All retry algorithms automatically attempt to re-execute failed operations, making your code more robust against temporary errors.

### Basic Retry - Unlimited Attempts

The `retry()` algorithm retries indefinitely until the operation succeeds:

```cpp
#include <flow/execution.hpp>
using namespace flow::execution;

auto flaky_operation = just(42)
    | then([](int x) {
        // Might throw occasionally
        if (random_failure()) {
            throw std::runtime_error("Transient error");
        }
        return x * 2;
    })
    | retry();  // Keep trying until success

auto result = flow::this_thread::sync_wait(std::move(flaky_operation));
```

**Use case**: Operations that should eventually succeed (e.g., connecting to a service that's temporarily down).

### Bounded Retry - Limited Attempts

The `retry_n(count)` algorithm retries up to N times before giving up:

```cpp
auto operation = risky_sender()
    | retry_n(3)  // Try up to 3 times total
    | upon_error([](std::exception_ptr ep) {
        // Handle final failure after all retries exhausted
        return fallback_value;
    });
```

**Use case**: Operations that might fail permanently (e.g., invalid user input, non-existent resources).

### Conditional Retry - Selective Error Handling

The `retry_if(predicate)` algorithm retries only when the predicate returns true:

```cpp
auto selective_retry = fetch_data()
    | retry_if([](std::exception_ptr ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const network_timeout& e) {
            return true;   // Retry on timeout
        } catch (const not_found_error& e) {
            return false;  // Don't retry on 404
        } catch (...) {
            return false;  // Don't retry on other errors
        }
    });
```

**Use case**: Distinguishing between transient failures (network timeout) and permanent failures (resource not found).

### Exponential Backoff - Smart Retry Delays

The `retry_with_backoff(...)` algorithm implements exponential backoff to avoid overwhelming failing services:

```cpp
thread_pool pool{4};

auto resilient_api_call = api_request()
    | retry_with_backoff(
        pool.get_scheduler(),            // Scheduler for delays
        std::chrono::milliseconds(100),  // Initial delay: 100ms
        std::chrono::milliseconds(5000), // Max delay: 5s
        2.0,                             // Multiplier: double each time
        10                               // Max attempts: 10
    );

// Retry delays: 100ms, 200ms, 400ms, 800ms, 1600ms, 3200ms, 5000ms (capped), ...
```

**Use case**: API calls, database connections, or any external service that might be temporarily overloaded.

### Retry Composition

Retry algorithms compose naturally with other sender operations:

```cpp
auto robust_pipeline = schedule(pool.get_scheduler())
    | then([] { return fetch_user_data(); })
    | retry_n(3)                           // Retry fetch up to 3 times
    | then([](auto data) {
        return process_data(data);
    })
    | upon_error([](std::exception_ptr ep) {
        log_error(ep);
        return default_result;
    });
```

### Key Features

- **🔁 Automatic Retry**: Transparent retry logic without manual loops
- **🎯 Selective Recovery**: Choose which errors to retry
- **⏱️ Smart Backoff**: Exponential delays prevent service overload
- **🔗 Composable**: Works seamlessly with other sender operations
- **🛑 Cancellation-Aware**: Respects stop tokens and cancellation requests
- **📊 Type-Safe**: Preserves completion signatures through retry chain

### Best Practices

1. **Use `retry_n()` by default** to prevent infinite loops
2. **Implement backoff for external services** to be a good citizen
3. **Use `retry_if()` for selective retry** based on error types
4. **Log retry attempts** for debugging and monitoring
5. **Set reasonable limits** on attempts and delays
6. **Combine with timeouts** to prevent hanging operations

---

## 🛑 Cancellation & Stop Tokens

Flow provides comprehensive support for cooperative cancellation through stop tokens, enabling graceful termination of asynchronous operations.

### Stop Token Types

Flow supports multiple stop token types:

```cpp
// Standard C++ stop token
std::stop_source source;
std::stop_token token = source.get_token();

// Inplace stop token (no allocation, lightweight)
flow::execution::inplace_stop_source source;
flow::execution::inplace_stop_token token = source.get_token();
```

### Query Stop Tokens from Environment

Senders can query stop tokens from their execution environment:

```cpp
auto work = schedule(pool.get_scheduler()) | then([](auto) {
    auto env = flow::execution::get_env(/* receiver */);
    auto token = flow::execution::get_stop_token(env);

    while (!token.stop_requested()) {
        // Do work that respects cancellation
    }
});
```

### Stop Callbacks

Register callbacks to be invoked when cancellation is requested:

```cpp
flow::execution::inplace_stop_source source;
auto token = source.get_token();

// Register callback
flow::execution::inplace_stop_callback callback(token, [] {
    std::cout << "Cancellation requested!\n";
});

source.request_stop();  // Callback is invoked
```

### when_any Active Cancellation

The `when_any` algorithm demonstrates active cancellation - when the first operation completes, remaining operations are automatically cancelled:

```cpp
auto fast = schedule(pool.get_scheduler()) | then([] { return 1; });
auto slow = schedule(pool.get_scheduler()) | then([] {
    // This will be cancelled when fast completes
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return 2;
});

auto race = when_any(std::move(fast), std::move(slow));
auto result = flow::this_thread::sync_wait(std::move(race));
// slow is automatically cancelled, doesn't wait 10 seconds
```

### Environment with Stop Token

Create execution environments with custom stop tokens:

```cpp
flow::execution::inplace_stop_source source;
auto env = flow::execution::make_env_with_stop_token(
    source.get_token(),
    flow::execution::empty_env{}
);
```

---

## 🎭 Async Scopes & Structured Concurrency

Flow provides comprehensive support for managing the lifetime of asynchronous operations through async scopes (P3149) and structured concurrency patterns (P3296).

### Simple Counting Scope

Basic scope that tracks active operations:

```cpp
#include <flow/execution.hpp>
using namespace flow::execution;

simple_counting_scope scope;
auto token = scope.get_token();

// Associate work with scope
auto work = associate(schedule(pool.get_scheduler()) | then([] {
    std::cout << "Work in scope\n";
    return 42;
}), token);

flow::this_thread::sync_wait(work);

// Wait for all work to complete
flow::this_thread::sync_wait(scope.join());
```

### Counting Scope

Advanced scope with stop token support for cancellation:

```cpp
counting_scope scope;
auto token = scope.get_token();

// Spawn fire-and-forget work
spawn(schedule(pool.get_scheduler()) | then([] {
    std::cout << "Background work\n";
}), token);

// Request cancellation of all work
scope.request_stop();

// Wait for cleanup
flow::this_thread::sync_wait(scope.join());
```

### let_async_scope - Structured Concurrency

The `let_async_scope` adaptor provides a safe way to spawn concurrent work that automatically waits for completion:

```cpp
auto result = flow::this_thread::sync_wait(
    just(42) | let_async_scope([&](auto scope_token) {
        // Spawn multiple concurrent operations
        spawn(schedule(pool.get_scheduler()) | then([](int x) {
            std::cout << "Processing " << x << "\n";
        }), scope_token);

        spawn(schedule(pool.get_scheduler()) | then([](int x) {
            std::cout << "Also processing " << x << "\n";
        }), scope_token);

        // Scope automatically waits for all spawned work
    })
);
```

### Spawn Future

Create futures from spawned work:

```cpp
counting_scope scope;
auto token = scope.get_token();

auto future = spawn_future(
    schedule(pool.get_scheduler()) | then([] {
        return 42;
    }),
    token
);

// Use future as a sender
auto result = flow::this_thread::sync_wait(future);

flow::this_thread::sync_wait(scope.join());
```

### Key Features

- **Lifetime Management**: Automatically track async operations
- **Structured Cancellation**: Stop token propagation
- **Error Handling**: First error stops all work in `let_async_scope`
- **Type Safety**: Compile-time checked scope associations
- **Fire-and-Forget**: Safe `spawn` with automatic cleanup

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

Flow includes a comprehensive test suite with 22+ test categories:

```bash
# Build with tests
cmake .. -DFLOW_BUILD_TESTS=ON
cmake --build .

# Run all tests
ctest

# Run specific test
./tests/scheduler_tests
./tests/async_scope_basic_tests
./tests/let_async_scope_tests

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
- **Async Scope Tests**: P3149 scope functionality
- **let_async_scope Tests**: P3296 structured concurrency
- **when_any Tests**: Racing operations and active cancellation
- **Retry Tests**: Retry mechanisms and error recovery
- **Bulk Policy Tests**: P3481R5 execution policies and bulk variants

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
- **P3669R2**: Non-blocking scheduler support with `try_scheduler` and lock-free queues
- **P3149**: Async scope support for lifetime management
- **P3296**: Structured concurrency with `let_async_scope`
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
- ✅ Essential algorithms (then, upon_error, bulk, when_all, when_any)
- ✅ Bulk algorithms with execution policies (P3481R5) - `bulk`, `bulk_chunked`, `bulk_unchunked`
- ✅ Execution policy support - `seq`, `par`, `par_unseq`, `unseq`
- ✅ Stop token support (inplace_stop_token, inplace_stop_source)
- ✅ Active cancellation in when_any
- ✅ Non-blocking operations (P3669R2) - `try_scheduler`, `try_schedule`
- ✅ Lock-free bounded queue for signal-safe operations
- ✅ Async scopes (P3149) - `simple_counting_scope`, `counting_scope`
- ✅ Structured concurrency (P3296) - `let_async_scope`
- ✅ Spawn operations (`spawn`, `spawn_future`)
- ✅ Retry mechanisms - `retry`, `retry_n`, `retry_if`, `retry_with_backoff`
- ✅ C++23 Modules - Experimental support via CMake FILE_SET CXX_MODULES
- ✅ Comprehensive test suite (26+ tests, including bulk policy tests)
- ✅ Example programs

### Future Explorations

- 🔄 **More algorithms** - `repeat`, `split`, `timeout`, etc.
- 🔄 **Coroutine integration** - `co_await` sender support
- 🔄 **I/O schedulers** - Async I/O primitives
- 🔄 **Timer support** - Scheduled/delayed execution
- 🔄 **Performance benchmarks** - Comparison with other implementations
- 🔄 **Enhanced error handling** - Better error aggregation in scopes

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
- [P3149: Async scopes](https://open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3149r11.html) - Lifetime management for async operations
- [P3296: `let_async_scope`](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3296r4.html) - Structured concurrency patterns
- [P3481R5: `bulk` improvements](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3481r5.html) - Execution policies and bulk algorithm variants
- [P3669R2: Non-blocking support](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3669r2.html) - Signal-safe `try_scheduler` and `try_schedule`

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
