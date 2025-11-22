// Only include this file when in test mode
// Production builds don't need any .cpp file (header-only)

#ifdef TA_TIME_TEST_MODE
#include "TA_Time.h"

namespace ta {
namespace time {
  // Test mode function pointer for mocking millis()
  uint32_t (*_testMillis)() = nullptr;
} // namespace time
} // namespace ta
#endif
