#include <atomic>
#include <boost/ut.hpp>
#include <vector>

#include "flow/execution.hpp"

int main() {
  using namespace boost::ut;
  using namespace flow::execution;
  "bulk_with_seq_policy"_test = [] {
    std::vector<int> results(10);
    auto s = just() | bulk(seq, 10, [&](std::size_t i) { results[i] = static_cast<int>(i * 2); });

    flow::this_thread::sync_wait(s);

    for (std::size_t i = 0; i < 10; ++i) {
      expect(results[i] == static_cast<int>(i * 2));
    }
  };

  "bulk_with_par_policy"_test = [] {
    std::vector<int> results(10);
    auto s = just() | bulk(par, 10, [&](std::size_t i) { results[i] = static_cast<int>(i * 3); });

    flow::this_thread::sync_wait(s);

    for (std::size_t i = 0; i < 10; ++i) {
      expect(results[i] == static_cast<int>(i * 3));
    }
  };

  "bulk_with_par_unseq_policy"_test = [] {
    std::atomic<int> counter{0};
    auto             s = just() | bulk(par_unseq, 100, [&](std::size_t /*i*/) { counter++; });

    flow::this_thread::sync_wait(s);

    expect(counter.load() == 100_i);
  };

  "bulk_chunked_with_seq_policy"_test = [] {
    std::vector<int> results(20);
    auto             s = just() | bulk_chunked(seq, 20, [&](std::size_t begin, std::size_t end) {
               for (std::size_t i = begin; i < end; ++i) {
                 results[i] = static_cast<int>(i * 5);
               }
             });

    flow::this_thread::sync_wait(s);

    for (std::size_t i = 0; i < 20; ++i) {
      expect(results[i] == static_cast<int>(i * 5));
    }
  };

  "bulk_chunked_with_par_policy"_test = [] {
    std::atomic<int> chunk_count{0};
    std::atomic<int> total_iterations{0};

    auto s = just() | bulk_chunked(par, 50, [&](std::size_t begin, std::size_t end) {
               chunk_count++;
               for (std::size_t i = begin; i < end; ++i) {
                 total_iterations++;
               }
             });

    flow::this_thread::sync_wait(s);

    expect(chunk_count.load() >= 1_i);  // At least one chunk
    expect(total_iterations.load() == 50_i);
  };

  "bulk_chunked_atomic_reduction"_test = [] {
    // Example from the proposal: chunked version allows better performance
    std::atomic<std::uint32_t> sum{0};
    std::vector<std::uint32_t> data(100);
    for (std::size_t i = 0; i < 100; ++i) {
      data[i] = static_cast<std::uint32_t>(i + 1);
    }

    auto s = just() | bulk_chunked(par, 100, [&](std::size_t begin, std::size_t end) {
               std::uint32_t partial_sum = 0;
               for (std::size_t i = begin; i < end; ++i) {
                 partial_sum += data[i];
               }
               sum.fetch_add(partial_sum, std::memory_order_relaxed);
             });

    flow::this_thread::sync_wait(s);

    // Sum of 1..100 = 100 * 101 / 2 = 5050
    expect(sum.load() == 5050_u);
  };

  "bulk_unchunked_with_seq_policy"_test = [] {
    std::vector<int> results(15);
    auto             s =
        just() | bulk_unchunked(seq, 15, [&](std::size_t i) { results[i] = static_cast<int>(i); });

    flow::this_thread::sync_wait(s);

    for (std::size_t i = 0; i < 15; ++i) {
      expect(results[i] == static_cast<int>(i));
    }
  };

  "bulk_unchunked_with_par_policy"_test = [] {
    std::atomic<int> counter{0};
    auto             s = just() | bulk_unchunked(par, 30, [&](std::size_t /*i*/) { counter++; });

    flow::this_thread::sync_wait(s);

    expect(counter.load() == 30_i);
  };

  "bulk_with_predecessor_values"_test = [] {
    std::vector<int> results(5);
    auto             s = just(10) | bulk(seq, 5, [&](std::size_t i, int multiplier) {
               results[i] = static_cast<int>(i * multiplier);
             });

    flow::this_thread::sync_wait(std::move(s));

    for (std::size_t i = 0; i < 5; ++i) {
      expect(results[i] == static_cast<int>(i * 10));
    }
  };

  "bulk_chunked_with_predecessor_values"_test = [] {
    std::vector<int> results(8);
    auto s = just(5) | bulk_chunked(par, 8, [&](std::size_t begin, std::size_t end, int base) {
               for (std::size_t i = begin; i < end; ++i) {
                 results[i] = static_cast<int>(i + base);
               }
             });

    flow::this_thread::sync_wait(std::move(s));

    for (std::size_t i = 0; i < 8; ++i) {
      expect(results[i] == static_cast<int>(i + 5));
    }
  };

  "bulk_error_propagation"_test = [] {
    try {
      auto s = just() | bulk(seq, 10, [](std::size_t i) {
                 if (i == 5) {
                   throw std::runtime_error("Test error");
                 }
               });

      flow::this_thread::sync_wait(s);
      expect(false) << "Should have thrown";
    } catch (const std::runtime_error& e) {
      expect(std::string(e.what()) == "Test error");
    }
  };

  "bulk_chunked_error_propagation"_test = [] {
    try {
      auto s = just() | bulk_chunked(seq, 20, [](std::size_t begin, std::size_t end) {
                 for (std::size_t i = begin; i < end; ++i) {
                   if (i == 10) {
                     throw std::runtime_error("Chunked error");
                   }
                 }
               });

      flow::this_thread::sync_wait(s);
      expect(false) << "Should have thrown";
    } catch (const std::runtime_error& e) {
      expect(std::string(e.what()) == "Chunked error");
    }
  };

  "bulk_zero_shape"_test = [] {
    int  call_count = 0;
    auto s          = just() | bulk(seq, 0, [&](std::size_t /*i*/) { call_count++; });

    flow::this_thread::sync_wait(s);

    expect(call_count == 0_i);
  };

  "bulk_chunked_zero_shape"_test = [] {
    int  call_count = 0;
    auto s = just() | bulk_chunked(seq, 0, [&](std::size_t /*begin*/, std::size_t /*end*/) {
               call_count++;
             });

    flow::this_thread::sync_wait(s);

    expect(call_count == 0_i);
  };

  "bulk_with_multiple_values"_test = [] {
    std::vector<int> results(5);
    auto             s = just(10, 20) | bulk(seq, 5, [&](std::size_t i, int a, int b) {
               results[i] = static_cast<int>(i + a + b);
             });

    flow::this_thread::sync_wait(std::move(s));

    for (std::size_t i = 0; i < 5; ++i) {
      expect(results[i] == static_cast<int>(i + 30));
    }
  };

  "bulk_composition"_test = [] {
    std::atomic<int> counter{0};
    auto             s = just(5) | bulk(par, 10, [&](std::size_t /*i*/, int /*val*/) { counter++; })
             | bulk(seq, 20, [&](std::size_t /*i*/, int /*val*/) { counter++; });

    flow::this_thread::sync_wait(std::move(s));

    expect(counter.load() == 30_i);
  };

  "bulk_different_policies_composition"_test = [] {
    std::vector<int> results(15);
    auto             s = just()
             | bulk_chunked(par, 15,
                            [&](std::size_t begin, std::size_t end) {
                              for (std::size_t i = begin; i < end; ++i) {
                                results[i] = static_cast<int>(i * 2);
                              }
                            })
             | bulk_unchunked(seq, 15, [&](std::size_t i) { results[i] += 10; });

    flow::this_thread::sync_wait(s);

    for (std::size_t i = 0; i < 15; ++i) {
      expect(results[i] == static_cast<int>((i * 2) + 10));
    }
  };
}
