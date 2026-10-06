#pragma once

// Marks a function as callable from the audio thread: it cannot throw (noexcept). Clang 20+ then checks statically (-Wfunction-effects)
// that it calls nothing that can allocate or block, and RealtimeSanitizer builds trap such calls at runtime.
#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(clang::nonblocking)
#define AIRBAND_NONBLOCKING noexcept [[clang::nonblocking]]
#endif
#endif

#ifndef AIRBAND_NONBLOCKING
#define AIRBAND_NONBLOCKING noexcept
#endif

// Brackets calls into JUCE that the static analysis cannot see through. Each is covered at runtime by the
// RealtimeSanitizer test (scripts/check_realtime.sh), so a call that did allocate would still be caught.
#if defined(__clang__)
#define AIRBAND_UNCHECKED_BEGIN _Pragma ("clang diagnostic push") _Pragma ("clang diagnostic ignored \"-Wfunction-effects\"")
#define AIRBAND_UNCHECKED_END _Pragma ("clang diagnostic pop")
#else
#define AIRBAND_UNCHECKED_BEGIN
#define AIRBAND_UNCHECKED_END
#endif
