#ifndef PHOTO_RENDERER_H
#define PHOTO_RENDERER_H

#include <cstdint>
#include <ftxui/dom/elements.hpp>
#include <vector>

ftxui::Element render_employee_photo(
    const std::vector<uint8_t>& photo_bytes,
    int char_w = 40,
    int char_h = 24);

#endif // PHOTO_RENDERER_H
