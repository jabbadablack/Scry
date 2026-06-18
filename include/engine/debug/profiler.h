#pragma once

#ifdef SCRY_DEBUG
#  include <tracy/TracyC.h>
#  define SCRY_PROFILE_FRAME()              TracyCFrameMark
#  define SCRY_PROFILE_ZONE(name)           TracyCZoneN(ctx_##name, #name, true)
#  define SCRY_PROFILE_ZONE_END(name)       TracyCZoneEnd(ctx_##name)
#  define SCRY_PROFILE_THREAD_NAME(name)    TracyCSetThreadName(name)
#else
#  define SCRY_PROFILE_FRAME()              do {} while(0)
#  define SCRY_PROFILE_ZONE(name)           do {} while(0)
#  define SCRY_PROFILE_ZONE_END(name)       do {} while(0)
#  define SCRY_PROFILE_THREAD_NAME(name)    do {} while(0)
#endif
