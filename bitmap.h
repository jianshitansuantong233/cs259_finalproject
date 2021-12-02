#ifndef BITMAP_H
#define BITMaP_H

#include <cstdint>

namespace Bitmap {

using std::size_t;

using bitmap_t = char; // bitmap element type.
constexpr size_t data_size = sizeof(bitmap_t) * 8;

// Computes the number of bitmap_t to satisfy a bitmap of number_of_elements.
constexpr size_t bitmap_size(size_t number_of_elements) {
  return (number_of_elements + data_size - 1) / data_size;
}

// Get bit at position n.
bool get_bit(const bitmap_t * const bitmap, size_t n);
// Set bit at position n to be 1.
void set_bit(bitmap_t * const bitmap, size_t n);

} // namespace Bitmap

#endif // BITMAP_H
