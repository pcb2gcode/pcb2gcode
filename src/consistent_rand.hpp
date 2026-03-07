// Provide a random number generator that is consistent across operating systems.
// It has an interface similar to srand() and rand().

#pragma once

#include <cstdint>

namespace ConsistentRand {

void srand(unsigned int seed);
int rand(void);

} // namespace ConsistentRand
