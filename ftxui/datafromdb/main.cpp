#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <string>
#include <vector>

// Подключаем ваш заголовок с DBManager и get_employees_list()
#include "db_manager.h"
#include "employee_service.h"
#include "photo_renderer.h"
#include "photo_viewer.h"

int main() {
    using namespace ftxui;

    // 1. Подключаемся к Firebird и получаем вектор сотрудников
    if (!DBManager::connect()) {
        return 1;
    }
    std::vector<Employee> employees = get_employees_list();

    int selected = 0;
    auto rows_container = Container::Vertical({}, &selected);

    // 2. Формируем строки таблицы из вектора
    for (size_t i = 0; i < employees.size(); ++i) {
        auto row = Renderer([&, i](bool focused) {
            const auto& emp = employees[i];

            // Собираем ФИО (если поля в std::string UTF-8)
            std::string full_name = emp.lastName + " " + emp.firstName + " " + emp.middleName;

            auto line = hbox({
                text(std::to_string(emp.id)) | size(WIDTH, EQUAL, 6),
                separator(),
                text(std::to_string(emp.sn)) | size(WIDTH, EQUAL, 14),
                separator(),
                text(full_name) | flex,
                separator(),
                text(emp.hasPhoto ? " [Да] " : " [Нет] ") | size(WIDTH, EQUAL, 8),
            });

            if (focused) {
                line = line | inverted | focus;
            }
            return line;
        });

        rows_container->Add(row);
    }

    // 3. Оборачиваем грид в общий макет с шапкой и подвалом
    auto layout = Renderer(rows_container, [&] {
        auto header = hbox({
            text("ID") | bold | size(WIDTH, EQUAL, 6),
            separator(),
            text("Таб. номер") | bold | size(WIDTH, EQUAL, 14),
            separator(),
            text("ФИО сотрудника") | bold | flex,
            separator(),
            text("Фото") | bold | size(WIDTH, EQUAL, 8),
        });

        Element body = employees.empty()
                           ? text("Записи не найдены") | center | flex
                           : rows_container->Render() | yframe | flex;

        return vbox({
                   text("Справочник сотрудников (Firebird)") | bold | center,
                   separator(),
                   header,
                   separator(),
                   body,
                   separator(),
                   text("Всего записей: " + std::to_string(employees.size()) + " | Карточка: Enter | Выход: 'q'") | dim,
               }) |
               border;
    });

    auto screen = ScreenInteractive::Fullscreen();

    bool show_card = false;
    Employee current_details{};
    Element current_photo_element;

    // 1. Компонент карточки (использует уже загруженные current_details и current_photo_element)
    auto card_component = Renderer([&] {
        std::string full_name = current_details.lastName + " " +
                                current_details.firstName + " " +
                                current_details.middleName;

        auto info_block = vbox({
                                hbox({text(" ID:           ") | dim, text(std::to_string(current_details.id)) | bold}),
                                hbox({text(" Таб. номер:   ") | dim, text(std::to_string(current_details.sn))}),
                                hbox({text(" Внутр. код:   ") | dim, text(std::to_string(current_details.si))}),
                                hbox({text(" ФИО:          ") | dim, text(full_name)}),
                                hbox({text(" Наличие фото: ") | dim, text(current_details.hasPhoto ? "Есть в базе" : "Отсутствует")}),
                                }) | flex;

        return vbox({
                   text(" Карточка сотрудника ") | bold | center,
                   separator(),
                   hbox({
                       current_photo_element,
                       separator(),
                       info_block,
                   }),
                   separator(),
                   text(current_details.hasPhoto
                            ? " Полное фото: 'f' | Закрыть: Esc или Enter "
                            : " Нажмите Esc или Enter, чтобы закрыть ") | dim | center,
               }) |
               borderDouble |
               size(WIDTH, GREATER_THAN, 98) |
               clear_under |
               center;
    });

    // 2. Перехват клавиш в открытой карточке
    auto card_modal = CatchEvent(card_component, [&](Event event) {
        // Открытие фото в полном разрешении по клавише 'f' (или русской 'а')
        if (event == Event::Character('f') || event == Event::Character('F') ||
            event == Event::Character("а") || event == Event::Character("А")) {
            if (current_details.hasPhoto && !current_details.photo.empty()) {
#ifdef _WIN32
                view_employee_photo_from_memory(current_details);
#else
                bool is_graphical_env = (std::getenv("DISPLAY") != nullptr || std::getenv("WAYLAND_DISPLAY") != nullptr);
                if (is_graphical_env) {
                    view_employee_photo_from_memory(current_details);
                } else {
                    // FTXUI временно приостанавливает отрисовку и возвращает обычный режим терминала
                    screen.WithRestoredIO([&] {
                        // view_employee_photo_fullscreen(current_details);
                        view_employee_photo_from_memory(current_details);
                    })(); // <-- Эти скобки запускают обёрнутую функцию!
                }
#endif
            }
            return true;
        }

        if (event == Event::Escape || event == Event::Return || event == Event::Character('q')) {
            show_card = false;
            current_details.photo.clear();
            return true;
        }
        return true;
    });

    // 3. Обработка нажатия Enter в таблице: запрашиваем БД и строим картинку один раз
    auto table_with_events = CatchEvent(layout, [&](Event event) {
        if (event == Event::Return && !employees.empty()) {
            // Загружаем полную запись с BLOB-фото из Firebird
            current_details = get_employee_details(employees[selected].id);
            // Превращаем байты JPEG в холст FTXUI
            current_photo_element = render_employee_photo(current_details.photo);
            show_card = true;
            return true;
        }
        if (event == Event::Character('q') || event == Event::Escape) {
            screen.Exit();
            return true;
        }
        return false;
    });

    // 8. Объединяем таблицу и карточку в единый интерфейс через dbox
    auto root_component = Renderer(table_with_events, [&] {
        Element document = table_with_events->Render();
        if (show_card) {
            // Накладываем карточку поверх таблицы
            document = dbox({
                document,
                card_modal->Render(),
            });
        }

#ifdef _WIN32
        // Принудительно скрываем курсор при отрисовке каждого кадра
        screen.SetCursor(Screen::Cursor{0, 0, Screen::Cursor::Hidden});
#endif

        return document;
    });

    // Маршрутизируем события в карточку, если она открыта, иначе — в таблицу
    auto app = CatchEvent(root_component, [&](Event event) {
        if (show_card) {
            return card_modal->OnEvent(event);
        }
        return table_with_events->OnEvent(event);
    });

    screen.Loop(app);

    DBManager::disconnect();
    return 0;
}
