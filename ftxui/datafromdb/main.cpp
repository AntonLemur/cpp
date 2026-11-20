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

    // Буферные переменные для полей ввода
    std::string edit_last_name;
    std::string edit_first_name;
    std::string edit_middle_name;
    std::string status_message;

    // 1. Создаём поля ввода FTXUI
    InputOption input_opt;
    input_opt.multiline = false; // Запрещаем перенос строки по Enter
    // input_opt.transform = [](InputState state) {
    //     // Текст внутри поля делаем контрастным
    //     state.element |= color(Color::Black);

    //     if (state.focused) {
    //         // Активное поле (в фокусе): светло-голубой/серый фон и чёткая рамка
    //         state.element |= bgcolor(Color::RGB(220, 235, 255));
    //     } else {
    //         // Неактивное поле: светло-серый фон, чтобы границы поля всегда были видны на белом фоне
    //         state.element |= bgcolor(Color::RGB(240, 240, 240));
    //     }
    //     return state.element;
    // };

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

            employees[selected].lastName = edit_last_name;
            employees[selected].firstName = edit_first_name;
            employees[selected].middleName = edit_middle_name;

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

    // Объединяем все интерактивные элементы карточки для навигации стрелками/Tab
    auto card_form_container = Container::Vertical({
        input_last_name,
        input_first_name,
        input_middle_name,
        btn_save,
        btn_cancel,
    });

    // 3. Отрисовка самой карточки
    auto card_component = Renderer(card_form_container, [&] {
        auto info_block = vbox({
                                hbox({text(" ID:           ") | dim, text(std::to_string(current_details.id)) | bold}),
                                hbox({text(" Таб. номер:   ") | dim, text(std::to_string(current_details.sn))}),
                                hbox({text(" Внутр. код:   ") | dim, text(std::to_string(current_details.si))}),
                                separator(),
                                hbox({
                                  text(" Фамилия:      ") | dim,
                                  input_last_name->Render() | (input_last_name->Focused() ? bgcolor(Color::Blue) | color(Color::White) :
                                                                   bgcolor(Color::GrayLight) | color(Color::Black)) | flex,
                                }),
                                hbox({
                                  text(" Имя:          ") | dim,
                                  input_first_name->Render() | (input_first_name->Focused() ? bgcolor(Color::Blue) | color(Color::White) :
                                                                   bgcolor(Color::GrayLight) | color(Color::Black)) | flex,
                                }),
                                hbox({
                                  text(" Отчество:     ") | dim,
                                  input_middle_name->Render() | (input_middle_name->Focused() ? bgcolor(Color::Blue) | color(Color::White) :
                                                                   bgcolor(Color::GrayLight) | color(Color::Black)) | flex,
                                }),
                                separator(),
                                hbox({text(" Наличие фото: ") | dim, text(current_details.hasPhoto ? "Есть в базе" : "Отсутствует")}),
                                filler(),
                                status_message.empty() ? text("") : text(status_message) | color(Color::Red),
                                hbox({btn_save->Render(), text("  "), btn_cancel->Render()}) | center,
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

    // 3. Обработка нажатия Enter в таблице: запрашиваем БД и строим картинку один раз
    auto table_with_events = CatchEvent(layout, [&](Event event) {
        if (event == Event::Return && !employees.empty()) {
            // Загружаем полную запись с BLOB-фото из Firebird
            current_details = get_employee_details(employees[selected].id);
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
