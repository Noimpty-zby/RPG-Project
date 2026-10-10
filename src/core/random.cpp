#include "core/random.h"

namespace {
// PCG32 state; the defaults are the reference PCG32_INITIALIZER.
std::uint64_t state = 0x853c49e6748fea9bULL;
std::uint64_t increment = 0xda3e39cb94b95bdbULL;
constexpr std::uint64_t Stream = 0xda3e39cb94b95bdbULL;
} // namespace

namespace Random {
void seed(std::uint64_t value) {
  // pcg32_srandom_r with a fixed stream.
  state = 0;
  increment = (Stream << 1u) | 1u;
  next();
  state += value;
  next();
}

std::uint32_t next() {
  const std::uint64_t old = state;
  state = old * 6364136223846793005ULL + increment;
  const auto xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
  const auto rotation = static_cast<std::uint32_t>(old >> 59u);
  return (xorshifted >> rotation) | (xorshifted << ((32u - rotation) & 31u));
}

int below(int bound) {
  if (bound <= 1) return 0;
  const auto range = static_cast<std::uint32_t>(bound);
  // Reject the low values that would make some results more likely.
  const std::uint32_t threshold = (0u - range) % range;
  for (;;) {
    const std::uint32_t value = next();
    if (value >= threshold) return static_cast<int>(value % range);
  }
}
} // namespace Random
