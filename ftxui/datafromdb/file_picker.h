#ifndef FILE_PICKER_H
#define FILE_PICKER_H
#pragma once

#include <ftxui/component/component.hpp>
#include <functional>
#include <string>
#include <filesystem>

// Создаёт компонент модального окна выбора изображения с живым предпросмотром.
// on_select вызывается при выборе файла (передаётся полный путь к файлу).
// on_cancel вызывается при отмене (нажатии Esc).
ftxui::Component CreateImageFilePicker(
    std::function<void(const std::filesystem::path&)> on_select,
    std::function<void()> on_cancel);

#endif // FILE_PICKER_H
