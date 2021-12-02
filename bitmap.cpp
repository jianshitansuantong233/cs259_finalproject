#include "bitmap.h"

namespace Bitmap {

inline size_t data_offset(size_t n) { return n / data_size; }
inline size_t bit_offset(size_t n) { return n & (data_size - 1); }

inline bool get_bit(const bitmap_t * const bitmap, size_t n) {
  return bitmap[data_offset(n)] >> bit_offset(n) & static_cast<bitmap_t>(1);
}

inline void set_bit(bitmap_t * const bitmap, size_t n) {
  bitmap[data_offset(n)] |= static_cast<bitmap_t>(1) << bit_offset(n);
}

} // namespace Bitmap

