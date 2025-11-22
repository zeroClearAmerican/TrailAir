#include "TA_Time.h"

#ifdef TA_TIME_TEST_MODE
namespace ta {
namespace time {
  // Test mode function pointer for mocking millis()
  uint32_t (*_testMillis)() = nullptr;
} // namespace time
} // namespace ta
#endif
