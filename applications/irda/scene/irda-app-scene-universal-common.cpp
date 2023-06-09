#include "../irda-app.hpp"
#include "assets_icons.h"
#include "gui/modules/button_menu.h"
#include "gui/modules/button_panel.h"
#include "../view/irda-app-brut-view.h"
#include "gui/view.h"
#include "irda/irda-app-event.hpp"
#include "irda/irda-app-view-manager.hpp"
#include "irda/scene/irda-app-scene.hpp"

void IrdaAppSceneUniversalCommon::irda_app_item_callback(void* context, uint32_t index) {
    IrdaApp* app = static_cast<IrdaApp*>(context);
    IrdaAppEvent event;

    event.type = IrdaAppEvent::Type::ButtonPanelPressed;
    event.payload.menu_index = index;

    app->get_view_manager()->send_event(&event);
}

static bool irda_popup_brut_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    furi_assert(event);
    auto app = static_cast<IrdaApp*>(context);
    bool consumed = false;

    if((event->type == InputTypeShort) && (event->key == InputKeyBack)) {
        consumed = true;
        IrdaAppEvent irda_event;

        irda_event.type = IrdaAppEvent::Type::ButtonPanelPopupBackPressed;
        app->get_view_manager()->send_event(&irda_event);
    }

    return consumed;
}

void IrdaAppSceneUniversalCommon::remove_popup(IrdaApp* app) {
    auto button_panel = app->get_view_manager()->get_button_panel();
    button_panel_set_popup_draw_callback(button_panel, NULL, NULL);
    button_panel_set_popup_input_callback(button_panel, NULL, NULL);
}

void IrdaAppSceneUniversalCommon::show_popup(IrdaApp* app, int record_amount) {
    auto button_panel = app->get_view_manager()->get_button_panel();
    auto popup_brut = app->get_view_manager()->get_popup_brut();
    popup_brut_set_progress_max(popup_brut, record_amount);
    button_panel_set_popup_draw_callback(button_panel, popup_brut_draw_callback, popup_brut);
    button_panel_set_popup_input_callback(button_panel, irda_popup_brut_input_callback, app);
}

void IrdaAppSceneUniversalCommon::progress_popup(IrdaApp* app) {
    popup_brut_increase_progress(app->get_view_manager()->get_popup_brut());
    auto button_panel = app->get_view_manager()->get_button_panel();
    with_view_model_cpp(button_panel_get_view(button_panel), void*, model, { return true; });
}

bool IrdaAppSceneUniversalCommon::on_event(IrdaApp* app, IrdaAppEvent* event) {
    bool consumed = false;

    if(event->type == IrdaAppEvent::Type::Tick) {
        if(brute_force_started) {
            auto view_manager = app->get_view_manager();
            IrdaAppEvent tick_event = {.type = IrdaAppEvent::Type::Tick};
            view_manager->send_event(&tick_event);
            if(brute_force.send_next_bruteforce()) {
                progress_popup(app);
            } else {
                brute_force.stop_bruteforce();
                brute_force_started = false;
                remove_popup(app);
            }
        }
        consumed = true;
    }

    if(event->type == IrdaAppEvent::Type::ButtonPanelPopupBackPressed) {
        consumed = true;
        brute_force_started = false;
        brute_force.stop_bruteforce();
        remove_popup(app);
    } else if(event->type == IrdaAppEvent::Type::ButtonPanelPressed) {
        int record_amount = 0;
        if(brute_force.start_bruteforce(event->payload.menu_index, record_amount)) {
            if(record_amount > 0) {
                brute_force_started = true;
                show_popup(app, record_amount);
            }
        } else {
            app->switch_to_previous_scene();
        }
        consumed = true;
    }

    return consumed;
}

void IrdaAppSceneUniversalCommon::on_exit(IrdaApp* app) {
    IrdaAppViewManager* view_manager = app->get_view_manager();
    ButtonPanel* button_panel = view_manager->get_button_panel();
    button_panel_clean(button_panel);
}
