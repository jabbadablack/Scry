#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #ifdef SCRY_EXPORT
        #define SCRY_API __declspec(dllexport)
    #else
        #define SCRY_API __declspec(dllimport)
    #endif
#else
    #if __GNUC__ >= 4
        #define SCRY_API __attribute__ ((visibility ("default")))
    #else
        #define SCRY_API
    #endif
#endif

extern "C" SCRY_API const char* ScryGetVersion();

