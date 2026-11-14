#include "photo_renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <algorithm>
#include <cmath>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/screen/color.hpp>
#include <vector>

ftxui::Element render_employee_photo(
    const std::vector<uint8_t>& photo_bytes,
    int char_w,
    int char_h) {
    using namespace ftxui;

    bool has_truecolor = (Terminal::ColorSupport() == Terminal::Color::TrueColor);

    if (photo_bytes.empty()) {
        return text(" [ Нет фото ] ") | dim | center |
               size(WIDTH, EQUAL, char_w) | size(HEIGHT, EQUAL, char_h) | border;
    }

    int img_w = 0, img_h = 0, channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        photo_bytes.data(), static_cast<int>(photo_bytes.size()), &img_w, &img_h, &channels, 3);

    if (!pixels || img_w <= 0 || img_h <= 0) {
        if (pixels) stbi_image_free(pixels);
        return text(" [ Ошибка фото ] ") | color(Color::Red) | center |
               size(WIDTH, EQUAL, char_w) | size(HEIGHT, EQUAL, char_h) | border;
    }

    // Лёгкое кадрирование по краям (обрезаем 15% пустого фона слева/справа и 8% сверху),
    // чтобы лицо занимало больше полезной площади холста
    int crop_x = static_cast<int>(img_w * 0.15f);
    int crop_y = static_cast<int>(img_h * 0.08f);
    int crop_w = std::max(1, img_w - crop_x * 2);
    int crop_h = std::max(1, img_h - static_cast<int>(img_h * 0.12f));

    const int dot_w = char_w * 2;
    const int dot_h = char_h * 4;

    std::vector<float> gray(dot_w * dot_h);
    float min_lum = 255.0f;
    float max_lum = 0.0f;

    // 1. Вычисляем яркость с усреднением области внутри кадрированной рамки
    for (int y = 0; y < dot_h; ++y) {
        int y0 = crop_y + (y * crop_h) / dot_h;
        int y1 = std::max(y0 + 1, crop_y + ((y + 1) * crop_h) / dot_h);
        y1 = std::min(y1, img_h);

        for (int x = 0; x < dot_w; ++x) {
            int x0 = crop_x + (x * crop_w) / dot_w;
            int x1 = std::max(x0 + 1, crop_x + ((x + 1) * crop_w) / dot_w);
            x1 = std::min(x1, img_w);

            float sum = 0.0f;
            int count = 0;
            for (int sy = y0; sy < y1; ++sy) {
                for (int sx = x0; sx < x1; ++sx) {
                    int idx = (sy * img_w + sx) * 3;
                    sum += 0.299f * pixels[idx] + 0.587f * pixels[idx + 1] + 0.114f * pixels[idx + 2];
                    ++count;
                }
            }

            float lum = (count > 0) ? (sum / count) : 0.0f;
            gray[y * dot_w + x] = lum;
            min_lum = std::min(min_lum, lum);
            max_lum = std::max(max_lum, lum);
        }
    }
    stbi_image_free(pixels);

    // 2. Растягиваем контраст и усиливаем тени лица (гамма 1.6)
    float range = std::max(1.0f, max_lum - min_lum);
    std::vector<float> contrasted(dot_w * dot_h);
    for (int i = 0; i < dot_w * dot_h; ++i) {
        float norm = std::clamp((gray[i] - min_lum) / range, 0.0f, 1.0f);
        contrasted[i] = std::pow(norm, 1.6f) * 255.0f;
    }

    // Копия для дизеринга
    std::vector<float> dither = contrasted;

    // 3. Рисуем точки Брайля с градациями серого для каждой клетки 2x4
    auto c = Canvas(dot_w, dot_h);
    for (int y = 0; y < dot_h; ++y) {
        for (int x = 0; x < dot_w; ++x) {
            int pos = y * dot_w + x;
            float old_pixel = dither[pos];

            // Порог 210 вместо 128 позволяет прорисовывать даже светлые тени на коже
            bool draw_dot = (old_pixel < 210.0f);
            float new_pixel = draw_dot ? 0.0f : 255.0f;

            if (draw_dot) {
                if (has_truecolor) {
                    // Задаём оттенок серого в зависимости от реальной яркости пикселя (от 0 до 200)
                    uint8_t shade = static_cast<uint8_t>(std::clamp(contrasted[pos] * 0.85f, 0.0f, 210.0f));
                    c.DrawPoint(x, y, true, Color::RGB(shade, shade, shade));
                } else {
                    // В чистой консоли TTY не используем TrueColor, чтобы не ломать атрибуты строк
                    c.DrawPoint(x, y, true);
                }
            }

            float err = old_pixel - new_pixel;
            if (x + 1 < dot_w) dither[pos + 1] += err * (7.0f / 16.0f);
            if (y + 1 < dot_h) {
                if (x > 0) dither[pos + dot_w - 1] += err * (3.0f / 16.0f);
                dither[pos + dot_w] += err * (5.0f / 16.0f);
                if (x + 1 < dot_w) dither[pos + dot_w + 1] += err * (1.0f / 16.0f);
            }
        }
    }

    return canvas(std::move(c)) | border;
}
