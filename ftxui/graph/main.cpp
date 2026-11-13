#include <atomic>
#include <chrono>
#include <cmath>
#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/canvas.hpp>
#include <ftxui/dom/elements.hpp>
#include <thread>

int main() {
    using namespace ftxui;

    auto screen = ScreenInteractive::Fullscreen();

    // Параметры анимации (атомарные, так как используются в двух потоках)
    std::atomic<double> phase = 0.0;
    std::atomic<bool> running = true;

    double amplitude = 15.0; // Высота волны
    double frequency = 0.1;  // Частота волны
    std::atomic<bool> paused = false;

    // 1. Описываем рендеринг кадра
    auto renderer = Renderer([&] {
        const int width = 140;
        const int height = 60;
        auto c = Canvas(width, height);

        int center_y = height / 2;

        // Рисуем статическую ось X
        c.DrawPointLine(0, center_y, width - 1, center_y, Color::GrayDark);

        // Считываем текущую фазу для этого кадра
        double current_phase = phase.load();

        int prev_x = 0;
        int prev_y = center_y;

        // Рисуем волну с учетом сдвига фазы
        for (int x = 0; x < width; ++x) {
            // int y = height / 2 + std::sin(x * frequency + current_phase) * amplitude;
            double angle = x * frequency * 0.12 - current_phase;
            int y = static_cast<int>(center_y - std::sin(angle) * (height * amplitude * 0.4));

            if (x > 0) {
                c.DrawPointLine(prev_x, prev_y, x, y, Color::Cyan);
            }
            prev_x = x;
            prev_y = y;
        }

        return vbox({
                   text(paused ? "Анимированная волна y = sin(x - t) [ ПАУЗА ]"
                               : "Анимированная волна y = sin(x - t)") | bold | center,
                   canvas(std::move(c)) | border | center,
                   text("Амплитуда - \u2191 \u2193. Частота - \u2190 \u2192. Пауза - Пробел") | dim | center,
                   text("Нажмите 'q' или 'Esc' для выхода") | dim | center,
               }) | center;
    });

    // 2. Добавляем обработку клавиш для выхода из программы
    auto component = CatchEvent(renderer, [&](Event event) {
        if (event == Event::Character('q') || event == Event::Escape) {
            running = false;
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == Event::ArrowUp) {
            amplitude = std::min(19.0, amplitude + 1.0); // Ограничиваем высотой холста
            return true;
        }
        if (event == Event::ArrowDown) {
            amplitude = std::max(1.0, amplitude - 1.0);
            return true;
        }
        if (event == Event::ArrowRight) {
            frequency += 0.02;
            return true;
        }
        if (event == Event::ArrowLeft) {
            frequency = std::max(0.02, frequency - 0.02);
            return true;
        }
        if (event == Event::Character(' ')) {
            paused = !paused; // Пауза по пробелу (если сделать paused атомарным для animator)
            return true;
        }
        return false;
    });

    // 3. Запускаем отдельный поток для обновления кадров (около 30 FPS)
    std::thread animator([&] {
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(33));
            // если только один писатель
            // phase = phase.load() + 0.15; // Сдвигаем волну
            if (!paused) {
                double current = phase.load();
                // Пытаемся записать (current + 0.15), только если phase всё ещё равна current.
                // Если другой поток успел её поменять, цикл сам обновит current и повторит попытку.
                while (!phase.compare_exchange_weak(current, current + 0.15));
                // начиная с c++ 20 можно так
                // phase += 0.15; // Атомарно! Вызывает phase.fetch_add(0.15)
                screen.PostEvent(Event::Custom); // Просим FTXUI перерисовать экран
            }
        }
    });

    // 4. Запускаем главный цикл интерфейса
    screen.Loop(component);

    // Корректно завершаем поток анимации при выходе
    running = false;
    if (animator.joinable()) {
        animator.join();
    }

    return 0;
}
