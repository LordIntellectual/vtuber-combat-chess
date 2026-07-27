#ifndef VCC_MSVC_COMPAT_HXX_
#define VCC_MSVC_COMPAT_HXX_

/*
 * Forced early include on MSVC (see CMakeLists.txt /FI).
 * - M_PI and friends from <cmath>
 * - alternative tokens and/or/not (iso646)
 */

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cmath>
#include <ciso646>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#endif
