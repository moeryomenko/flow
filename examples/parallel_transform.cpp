#include <flow/execution.hpp>
#include <iostream>
#include <numeric>
#include <span>
#include <vector>

using namespace flow::execution;

template <scheduler Sch>
auto parallel_transform_reduce(Sch sch, std::span<const int> input, std::span<int> output,
                               auto transform_op, auto reduce_op, int init) {
  const size_t chunk_size = input.size() / 4;  // 4 chunks

  return just(std::vector<int>(4)) | then([=](std::vector<int>&& partials) -> auto {
           auto bulk_sender = schedule(sch)
                              | bulk(seq, size_t(4),
                                     [=, partials_ptr = partials.data()](size_t i) mutable -> auto {
                                       auto start = i * chunk_size;
                                       auto end   = std::min(input.size(), (i + 1) * chunk_size);

                                       // Transform and reduce this chunk
                                       int partial = init;
                                       for (size_t j = start; j < end; ++j) {
                                         output[j] = transform_op(input[j]);
                                         partial   = reduce_op(partial, output[j]);
                                       }
                                       // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
                                       partials_ptr[i] = partial;
                                     });

           // Execute bulk work and then reduce
           flow::this_thread::sync_wait(bulk_sender);

           // Final reduction
           return std::accumulate(partials.begin(), partials.end(), init, reduce_op);
         });
}

auto main() -> int {
  thread_pool pool{4};

  // Create input data
  std::vector<int> input(1000);
  std::iota(input.begin(), input.end(), 1);  // 1, 2, 3, ..., 1000  // NOLINT(modernize-use-ranges)

  std::vector<int> output(input.size());

  std::cout << "Computing sum of squares of 1..1000" << '\n';

  auto sender = parallel_transform_reduce(
      pool.get_scheduler(), std::span{input}, std::span{output},
      [](int x) -> int { return x * x; },         // square
      [](int a, int b) -> int { return a + b; },  // sum
      0                                           // initial value
  );

  auto result = flow::this_thread::sync_wait(std::move(sender));

  if (result) {
    std::cout << "Sum of squares: " << std::get<0>(*result) << '\n';
    std::cout << "Expected: " << (1000 * 1001 * 2001) / 6 << '\n';
  }

  return 0;
}
