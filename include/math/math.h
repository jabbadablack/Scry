#ifndef ENGINE_MATH_H
#define ENGINE_MATH_H

#include "../OS/types.h"

// Disable Eigen's automatic MSVC alignment warnings if compiled under MSVC
#if defined(_MSC_VER)
    #pragma warning(push)
    #pragma warning(disable : 4324) // structure was padded due to alignment specifier
#endif

#ifndef ENGINE_MATH_SUB_INCLUSION
    #include "algebra.hpp"
    #include "trigonometry.hpp"
    #include "geometry.hpp"
    #include "calculus.hpp"
    #include "interpolation.hpp"
    #include "random.hpp"
    #include "pathfinding.hpp"
#endif

#if defined(_MSC_VER)
    #pragma warning(pop)
#endif

#endif // ENGINE_MATH_H
