/**
 * @file TA_Time.cpp
 * @brief Implementation file for TA_Time test mode support
 * 
 * This file is only compiled when TA_TIME_TEST_MODE is defined.
 * Production builds use the header-only implementation.
 */

#ifdef TA_TIME_TEST_MODE
#include "TA_Time.h"

namespace trailair {
namespace time {
  /// @brief Test mode function pointer for mocking millis()
  uint32_t (*_testMillisFunction)() = nullptr;
} // namespace time
} // namespace trailair
#endif
