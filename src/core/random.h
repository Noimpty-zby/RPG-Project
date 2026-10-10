#pragma once
#include <cstdint>

// Game-owned pseudo-random numbers (PCG32, pcg-random.org). Unlike std::rand,
// a seed produces the same sequence on every platform and standard library,
// so expedition seeds and simulation results can be shared and compared.
namespace Random {
void seed(std::uint64_t value);
std::uint32_t next();
// Uniform integer in [0, bound) without modulo bias; 0 when bound <= 1.
int below(int bound);
} // namespace Random
