#ifndef BITMAP_H
#define BITMAP_H

#include <cstdint>

namespace Bitmap {

using std::size_t;

using bitmap_t = char; // bitmap element type.
constexpr size_t data_size = sizeof(bitmap_t) * 8;

// Computes the number of bitmap_t to satisfy a bitmap of number_of_elements.
constexpr size_t bitmap_size(size_t number_of_elements) {
  return (number_of_elements + data_size - 1) / data_size;
}

inline size_t data_offset(size_t n) { return n / data_size; }
inline size_t bit_offset(size_t n) { return n & (data_size - 1); }

// Get bit at position n.
inline
bool get_bit(const bitmap_t * const bitmap, size_t n) {
  return bitmap[data_offset(n)] >> bit_offset(n) & static_cast<bitmap_t>(1);
}
// Set bit at position n to be 1.
inline
void set_bit(bitmap_t * const bitmap, size_t n) {
  bitmap[data_offset(n)] |= static_cast<bitmap_t>(1) << bit_offset(n);
}

} // namespace Bitmap

#endif // BITMAP_H
