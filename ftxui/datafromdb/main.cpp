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
#include "file_picker.h"

#include <algorithm>
#include <cctype>
#include <fstream>

// Приводит строку UTF-8 (латиницу и кириллицу) к нижнему регистру для поиска
std::string to_lower_utf8(const std::string& s) {
    std::string res;
    res.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        // 1. Обычная латиница A-Z
        if (c >= 'A' && c <= 'Z') {
            res.push_back(static_cast<char>(c + 32));
        }
        // 2. Кириллица в UTF-8 (двухбайтовые последовательности)
        else if (c == 0xD0 && i + 1 < s.size()) {
            unsigned char c2 = static_cast<unsigned char>(s[i + 1]);
            if (c2 >= 0x90 && c2 <= 0x9F) { // А-П -> а-п
                res.push_back(static_cast<char>(0xD0));
                res.push_back(static_cast<char>(c2 + 0x20));
                ++i;
            } else if (c2 >= 0xA0 && c2 <= 0xAF) { // Р-Я -> р-я
                res.push_back(static_cast<char>(0xD1));
                res.push_back(static_cast<char>(c2 - 0x20));
                ++i;
            } else if (c2 == 0x81) { // Ё -> ё
                res.push_back(static_cast<char>(0xD1));
                res.push_back(static_cast<char>(0x91));
                ++i;
            } else {
                res.push_back(s[i]);
            }
        } else {
            res.push_back(s[i]);
        }
    }
    return res;
}

int main() {
    using namespace ftxui;

    // 1. Подключаемся к Firebird и получаем вектор сотрудников
    if (!DBManager::connect()) {
        return 1;
    }
    std::vector<Employee> employees = get_employees_list();

    int selected = 0;
    std::string search_query;
    std::string last_search_query; // Запоминаем последнее состояние строки поиска
    std::vector<size_t> filtered_indices;
    bool show_file_picker = false;
    bool photo_changed = false;

    auto rows_container = Container::Vertical({}, &selected);

    // Функция пересборки видимых строк при изменении строки поиска
    auto rebuild_table = [&] {
        rows_container->DetachAllChildren();
        filtered_indices.clear();
        selected = 0;

        std::string q_lower = to_lower_utf8(search_query);

        for (size_t i = 0; i < employees.size(); ++i) {
            const auto& emp = employees[i];
            std::string full_name = emp.lastName + " " + emp.firstName + " " + emp.middleName;
            std::string sn_str = std::to_string(emp.sn);

            // Проверяем совпадение по ФИО (без учёта регистра) или по табельному номеру
            bool match = q_lower.empty() ||
                         (to_lower_utf8(full_name).find(q_lower) != std::string::npos) ||
                         (sn_str.find(q_lower) != std::string::npos);

            if (match) {
                filtered_indices.push_back(i);

                auto row = Renderer([&, i](bool focused) {
                    const auto& item = employees[i];
                    std::string item_name = item.lastName + " " + item.firstName + " " + item.middleName;

                    auto line = hbox({
                        text(std::to_string(item.id)) | size(WIDTH, EQUAL, 6),
                        separator(),
                        text(std::to_string(item.sn)) | size(WIDTH, EQUAL, 14),
                        separator(),
                        text(item_name) | flex,
                        separator(),
                        text(item.hasPhoto ? " [Да] " : " [Нет] ") | size(WIDTH, EQUAL, 8),
                    });

                    if (focused) {
                        line = line | inverted | focus;
                    }
                    return line;
                });

                rows_container->Add(row);
            }
        }
    };

    // Первоначальное заполнение таблицы всеми записями
    rebuild_table();

    // Поле живого поиска
    InputOption search_opt;
    search_opt.multiline = false;
    // search_opt.on_change = rebuild_table; // Вызывается автоматически при каждой введённой букве!
    search_opt.transform = [](InputState state) {
        if (state.focused) {
            state.element |= bgcolor(Color::Blue) | color(Color::White);
        } else {
            state.element |= bgcolor(Color::GrayLight) | color(Color::Black);
        }
        return state.element;
    };

    auto search_input = Input(&search_query, "Введите фамилию или таб. номер...", search_opt);

    // Объединяем строку поиска и таблицу в единый вертикальный контейнер фокуса
    auto main_area_container = Container::Vertical({
        search_input,
        rows_container,
    });

    // 3. Оборачиваем грид в общий макет с шапкой и подвалом
    auto layout = Renderer(main_area_container, [&] {
        // Если строка поиска изменилась (любым способом: ввод, Backspace, Delete) — пересобираем таблицу
        if (search_query != last_search_query) {
            last_search_query = search_query;
            rebuild_table();
        }

        auto header = hbox({
            text("ID") | bold | size(WIDTH, EQUAL, 6),
            separator(),
            text("Таб. номер") | bold | size(WIDTH, EQUAL, 14),
            separator(),
            text("ФИО сотрудника") | bold | flex,
            separator(),
            text("Фото") | bold | size(WIDTH, EQUAL, 8),
        });

        Element body = filtered_indices.empty()
                           ? text("Совпадений не найдено") | dim | center | flex
                           : rows_container->Render() | yframe | flex;

        return vbox({
                   text("Справочник сотрудников (Firebird)") | bold | center,
                   separator(),
                   hbox({
                       text(" Поиск: ") | bold,
                       search_input->Render() | flex,
                       search_query.empty() ? text("") : text(" [Esc — очистить] ") | dim,
                   }),
                   separator(),
                   header,
                   separator(),
                   body,
                   separator(),
                   text("Найдено: " + std::to_string(filtered_indices.size()) +
                        " из " + std::to_string(employees.size()) +
                        " | Поиск: '/' | Карточка: Enter | Выход: 'q'") | dim,
               }) |
               border;
    });

    auto screen = ScreenInteractive::Fullscreen();

    bool show_card = false;
    Employee current_details{};
    Element current_photo_element;

    // Буферные переменные для полей ввода
    std::string edit_last_name;
    std::string edit_first_name;
    std::string edit_middle_name;
    std::string status_message;

    // 1. Создаём поля ввода FTXUI
    InputOption input_opt;
    input_opt.multiline = false; // Запрещаем перенос строки по Enter
    input_opt.transform = [](InputState state) {
        if (state.focused) {
            state.element |= bgcolor(Color::Blue) | color(Color::White);
        } else {
            state.element |= bgcolor(Color::GrayLight) | color(Color::Black);
        }
        return state.element;
    };

    auto input_last_name  = Input(&edit_last_name, "Введите фамилию", input_opt);
    auto input_first_name = Input(&edit_first_name, "Введите имя", input_opt);
    auto input_middle_name = Input(&edit_middle_name, "Введите отчество", input_opt);

    // 2. Кнопки действий
    ButtonOption btn_opt;
    btn_opt.transform = [](const EntryState& s) {
        auto element = text(s.label);
        if (s.focused) {
            element = element | inverted | bold;
        }
        return element;
    };

    auto btn_save = Button(" [ Сохранить ] ", [&] {
        // Распаковка полей (фича C++17)
        auto [ok, error] = update_employee_name(current_details.id, edit_last_name, edit_first_name, edit_middle_name);
        if (ok) {
            // Обновляем данные в открытой карточке и в списке таблицы на экране
            current_details.lastName = edit_last_name;
            current_details.firstName = edit_first_name;
            current_details.middleName = edit_middle_name;

            size_t real_idx = filtered_indices[selected];
            employees[real_idx].lastName = edit_last_name;
            employees[real_idx].firstName = edit_first_name;
            employees[real_idx].middleName = edit_middle_name;
            if (photo_changed)
            {
                update_employee_photo(current_details.id, current_details.photo);
                employees[real_idx].hasPhoto = current_details.hasPhoto;
            }
            show_card = false;
            current_details.photo.clear();
        } else {
            status_message = error;
        }
    },btn_opt);

    auto btn_cancel = Button(" [ Отмена ] ", [&] {
        show_card = false;
        current_details.photo.clear();
    },btn_opt);

    auto btn_change_photo = Button(" [ Изменить фото ] ", [&] { show_file_picker = true; }, btn_opt);

    // Объединяем все интерактивные элементы карточки для навигации стрелками/Tab
    auto card_form_container = Container::Vertical({
        input_last_name,
        input_first_name,
        input_middle_name,
        btn_save,
        btn_cancel,
        btn_change_photo,
    });

    // 3. Отрисовка самой карточки
    auto card_component = Renderer(card_form_container, [&] {
        auto info_block = vbox({
                                hbox({text(" ID:           ") | dim, text(std::to_string(current_details.id)) | bold}),
                                hbox({text(" Таб. номер:   ") | dim, text(std::to_string(current_details.sn))}),
                                hbox({text(" Внутр. код:   ") | dim, text(std::to_string(current_details.si))}),
                                separator(),
                                hbox({text(" Фамилия:      ") | dim, input_last_name->Render() | flex}),
                                hbox({text(" Имя:          ") | dim, input_first_name->Render() | flex}),
                                hbox({text(" Отчество:     ") | dim, input_middle_name->Render() | flex}),
                                separator(),
                                hbox({text(" Наличие фото: ") | dim, text(current_details.hasPhoto ? "Есть в базе" : "Отсутствует")}),
                                filler(),
                                status_message.empty() ? text("") : text(status_message) | color(Color::Red),
                                hbox({btn_save->Render(), text("  "), btn_cancel->Render(), text("  "), btn_change_photo->Render()}) | center,
                                }) | flex;

        return vbox({
                   text(" Карточка сотрудника (редактирование) ") | bold | center,
                   separator(),
                   hbox({
                       current_photo_element,
                       separator(),
                       info_block,
                   }),
                   separator(),
                   text(current_details.hasPhoto
                            ? " Переход: Tab/Стрелки | Полное фото: F2 | Закрыть: Esc "
                            : " Переход: Tab/Стрелки | Закрыть: Esc ") | dim | center,
               }) |
               borderDouble |
               size(WIDTH, GREATER_THAN, 98) |
               clear_under |
               center;
    });

    // 2. Перехват клавиш в открытой карточке
    auto card_modal = CatchEvent(card_component, [&](Event event) {
        // Открытие фото в полном разрешении по клавише 'f' (или русской 'а')
        if (event == Event::F2) {
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

        if (event == Event::Escape) {
            show_card = false;
            current_details.photo.clear();
            return true;
        }

        // Стрелки Вверх/Вниз и обратная табуляция (включая код \033[Z в Linux-терминалах)
        if (event == Event::ArrowDown || event == Event::Tab) {
            card_form_container->OnEvent(Event::Tab);
            return true;
        }
        if (event == Event::ArrowUp || event == Event::TabReverse || event == Event::Special("\033[Z")) {
            card_form_container->OnEvent(Event::TabReverse);
            return true;
        }

        if (event == Event::ArrowRight && btn_save->Focused()) {
            btn_cancel->TakeFocus();
            return true;
        }
        if (event == Event::ArrowLeft && btn_cancel->Focused()) {
            btn_save->TakeFocus();
            return true;
        }

        // Все остальные клавиши передаём напрямую в контейнер формы
        card_form_container->OnEvent(event);
        return true; // Всегда поглощаем любые события, пока открыта карточка!
    });

    // Окно выбора фото
    auto file_picker_modal = CreateImageFilePicker(
        [&](const std::string& chosen_path) {
            std::ifstream f(chosen_path, std::ios::binary | std::ios::ate);
            if (f.is_open()) {
                auto sz = f.tellg();
                f.seekg(0, std::ios::beg);
                current_details.photo.resize(static_cast<size_t>(sz));
                f.read(reinterpret_cast<char*>(current_details.photo.data()), sz);
                current_details.hasPhoto = !current_details.photo.empty();
                current_photo_element = render_employee_photo(current_details.photo);
                photo_changed = true;
            }
            show_file_picker = false;
        },
        [&] { show_file_picker = false; }
    );

    // 3. Обработка нажатия Enter в таблице: запрашиваем БД и строим картинку один раз
    auto table_with_events = CatchEvent(layout, [&](Event event) {
        // Если фокус сейчас в строке поиска:
        if (search_input->Focused()) {
            if (event == Event::Escape) {
                if (!search_query.empty()) {
                    search_query.clear();
                    rebuild_table();
                } else {
                    rows_container->TakeFocus(); // Если строка пуста, Esc переводит фокус в таблицу
                }
                return true;
            }
            // Нажатие Enter или стрелки Вниз переводит фокус из поиска на первую найденную строку
            if ((event == Event::Return || event == Event::ArrowDown) && !filtered_indices.empty()) {
                rows_container->TakeFocus();
                return true;
            }
            // Все остальные клавиши (включая 'q' и 'й') печатаются в строку поиска!
            return false;
        }

        // Если фокус в таблице:
        if (event == Event::Character('/') || event == Event::F3) {
            search_input->TakeFocus(); // Быстрый переход в строку поиска по '/' или F3
            return true;
        }

        if (event == Event::Return &&  !filtered_indices.empty()) {
            size_t real_idx = filtered_indices[selected];
            // Загружаем полную запись с BLOB-фото из Firebird
            current_details = get_employee_details(employees[real_idx].id);
            // Превращаем байты JPEG в холст FTXUI
            current_photo_element = render_employee_photo(current_details.photo);

            edit_last_name = current_details.lastName;
            edit_first_name = current_details.firstName;
            edit_middle_name = current_details.middleName;
            status_message.clear();
            input_last_name->TakeFocus();

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
        if (show_file_picker) {
            document = dbox({document, file_picker_modal->Render()});
        }
#ifdef _WIN32
        // Принудительно скрываем курсор при отрисовке каждого кадра
        screen.SetCursor(Screen::Cursor{0, 0, Screen::Cursor::Hidden});
#endif

        return document;
    });

    // Маршрутизируем события в карточку, если она открыта, иначе — в таблицу
    auto app = CatchEvent(root_component, [&](Event event) {
        if (show_file_picker) return file_picker_modal->OnEvent(event);
        if (show_card) {
            return card_modal->OnEvent(event);
        }
        return table_with_events->OnEvent(event);
    });

    screen.Loop(app);

    DBManager::disconnect();
    return 0;
}
