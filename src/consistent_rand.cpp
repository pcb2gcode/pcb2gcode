// Provide a random number generator that is consistent across operating systems.
// It has an interface similar to srand() and rand().

#include "consistent_rand.hpp"

namespace ConsistentRand {

// A global variable representing the current state of the random number generator.
thread_local static uint32_t state = 1;

void srand(unsigned int seed) {
  state = seed ? seed : 1;
}

int rand(void) {
  state ^= state << 13;
  state ^= state >> 17;
  state ^= state << 5;
  return static_cast<int>(state & 0x7FFFFFFF);  // Return positive int
}

} // namespace ConsistentRand
