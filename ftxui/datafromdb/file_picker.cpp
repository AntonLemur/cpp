#include "file_picker.h"
#include "photo_renderer.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <ftxui/dom/elements.hpp>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct DirEntryItem {
    std::string display_name;
    fs::path full_path;
    bool is_directory = false;
};

bool is_supported_image(const fs::path& p) {
    if (!p.has_extension()) return false;
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".jpg" || ext == ".jpeg" || ext == ".png" || ext == ".bmp";
}

std::vector<uint8_t> read_file_bytes(const fs::path& p) {
    std::ifstream file(p, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return {};
    std::streamsize size = file.tellg();
    if (size <= 0 || size > 15 * 1024 * 1024) return {}; // Ограничение 15 МБ для защиты памяти
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return {};
    return buffer;
}

} // namespace

ftxui::Component CreateImageFilePicker(
    std::function<void(const std::string&)> on_select,
    std::function<void()> on_cancel) {
    using namespace ftxui;

    auto current_dir = std::make_shared<fs::path>(fs::current_path());
    auto items = std::make_shared<std::vector<DirEntryItem>>();
    auto selected_idx = std::make_shared<int>(0);
    auto last_preview_idx = std::make_shared<int>(-1);
    auto preview_element = std::make_shared<Element>(
        text(" [ Выберите файл ] ") | dim | center | size(WIDTH, EQUAL, 36) | size(HEIGHT, EQUAL, 20) | border);

    auto list_container = Container::Vertical({}, selected_idx.get());

    // Обновление правой панели с миниатюрой при смене выделенной строки
    auto update_preview = [=] {
        if (*selected_idx == *last_preview_idx) return;
        *last_preview_idx = *selected_idx;

        if (*selected_idx < 0 || static_cast<size_t>(*selected_idx) >= items->size() ||
            (*items)[*selected_idx].is_directory) {
            *preview_element = text(" [ Каталог ] ") | dim | center |
                               size(WIDTH, EQUAL, 36) | size(HEIGHT, EQUAL, 20) | border;
            return;
        }

        auto bytes = read_file_bytes((*items)[*selected_idx].full_path);
        *preview_element = render_employee_photo(bytes, 36, 20);
    };

    // Сканирование текущей директории
    // std::shared_ptr<std::function<...>> в C++ обычно используют для рекурсивных лямбд
    // но здесь можно объявить обычную локальную переменную
    // auto load_directory = std::make_shared<std::function<void()>>();
    auto load_directory = [=] {
        items->clear();
        list_container->DetachAllChildren();
        *selected_idx = 0;
        *last_preview_idx = -1;

        // Добавляем пункт выхода на уровень выше, если не в корне
        if (current_dir->has_parent_path() && *current_dir != current_dir->parent_path()) {
            items->push_back({"[ .. ]", current_dir->parent_path(), true});
        }

        std::vector<DirEntryItem> dirs;
        std::vector<DirEntryItem> files;

        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(*current_dir, ec)) {
            if (entry.is_directory(ec)) {
                dirs.push_back({"/" + entry.path().filename().string(), entry.path(), true});
            } else if (entry.is_regular_file(ec) && is_supported_image(entry.path())) {
                files.push_back({entry.path().filename().string(), entry.path(), false});
            }
        }

        auto sort_by_name = [](const DirEntryItem& a, const DirEntryItem& b) {
            return a.display_name < b.display_name;
        };
        std::sort(dirs.begin(), dirs.end(), sort_by_name);
        std::sort(files.begin(), files.end(), sort_by_name);

        items->insert(items->end(), dirs.begin(), dirs.end());
        items->insert(items->end(), files.begin(), files.end());

        for (size_t i = 0; i < items->size(); ++i) {
            auto row = Renderer([items, i](bool focused) {
                const auto& item = (*items)[i];
                auto line = text(item.display_name);
                if (item.is_directory) {
                    line = line | bold;
                }
                if (focused) {
                    line = line | inverted | focus;
                }
                return line;
            });
            list_container->Add(row);
        }

        update_preview();
    };

    // Первичная загрузка текущей папки
    load_directory();

    auto renderer = Renderer(list_container, [=] {
        update_preview();

        return vbox({
                   text(" Выбор фотографии сотрудника ") | bold | center,
                   separator(),
                   text(" Путь: " + current_dir->string()) | dim,
                   separator(),
                   hbox({
                       list_container->Render() | yframe | size(WIDTH, EQUAL, 38) | size(HEIGHT, EQUAL, 20),
                       separator(),
                       *preview_element,
                   }),
                   separator(),
                   text(" Вход/Выбор: Enter | Назад: Backspace | Отмена: Esc ") | dim | center,
               }) |
               borderDouble |
               clear_under |
               center;
    });

    return CatchEvent(renderer, [=](Event event) {
        if (event == Event::Escape) {
            on_cancel();
            return true;
        }
        if (event == Event::Backspace) {
            if (current_dir->has_parent_path() && *current_dir != current_dir->parent_path()) {
                *current_dir = current_dir->parent_path();
                load_directory();
            }
            return true;
        }
        if (event == Event::Return) {
            if (!items->empty() && *selected_idx >= 0 && static_cast<size_t>(*selected_idx) < items->size()) {
                const auto& chosen = (*items)[*selected_idx];
                if (chosen.is_directory) {
                    *current_dir = chosen.full_path;
                    load_directory();
                } else {
                    on_select(chosen.full_path.string());
                }
            }
            return true;
        }
        list_container->OnEvent(event);
        return true; // Блокируем утечку клавиш в нижние окна
    });
}
