#if !defined(HANDMADE_INTRINSICS_H)

// TODO(gab): Convert all of these to intrinsics and remove math.h

#include <math.h>

inline int32 roundReal32ToInt32(real32 value) {
  return (int32) roundf(value);
}

inline uint32 roundReal32ToUint32(real32 value) {
  return (uint32) roundf(value);
}

inline int32 floorReal32ToInt32(real32 value) {
  return (int32) floorf(value);
}

inline int32 truncateReal32ToInt32(real32 value) {
  return (int32) value;
}

inline real32 sin(real32 angle) {
  return sinf(angle);
}

inline real32 cos(real32 angle) {
  return cosf(angle);
}

inline real32 atan2(real32 y, real32 x) {
  return atan2(y, x);
}

#define HANDMADE_INTRINSICS_H
#endif
