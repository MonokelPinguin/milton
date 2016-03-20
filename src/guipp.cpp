// This file is part of Milton.
//
// Milton is free software: you can redistribute it and/or modify it under the
// terms of the GNU Lesser General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option)
// any later version.
//
// Milton is distributed in the hope that it will be useful, but WITHOUT ANY
// WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
// more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Milton.  If not, see <http://www.gnu.org/licenses/>.

#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(push, 0)
#endif  // _WIN32 && _MSC_VER

#include <imgui.h>

#include "gui.h"
#include "localization.h"
#include "platform.h"
#include "rasterizer.h"
#include "persist.h"

static void exporter_init(Exporter* exporter)
{
    *exporter = {};
    exporter->scale = 1;
}

void milton_gui_tick(MiltonInput* input, MiltonState* milton_state)
{
    // ImGui Section
    auto default_imgui_window_flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize;

    const float pen_alpha = milton_get_pen_alpha(milton_state);
    assert(pen_alpha >= 0.0f && pen_alpha <= 1.0f);
    // Spawn below the picker
    Rect pbounds = get_bounds_for_picker_and_colors(&milton_state->gui->picker);

    int color_stack = 0;
    ImGui::GetStyle().WindowFillAlphaDefault = 0.9f;  // Redundant for all calls but the first one...
    ImGui::PushStyleColor(ImGuiCol_WindowBg,        ImVec4{.5f,.5f,.5f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_TitleBg,         ImVec4{.3f,.3f,.3f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive,   ImVec4{.4f,.4f,.4f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_Button,          ImVec4{.3f,.3f,.4f,1}); ++color_stack;
    ImGui::PushStyleColor(ImGuiCol_Text,            ImVec4{1, 1, 1, 1}); ++color_stack;

    ImGui::PushStyleColor(ImGuiCol_MenuBarBg,   ImVec4{.3f,.3f,.3f,1}); ++color_stack;

    //ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4{.1f,.1f,.1f,1}); ++color_stack;


    int menu_style_stack = 0;
    ImGui::PushStyleColor(ImGuiCol_WindowBg,        ImVec4{.3f,.3f,.3f,1}); ++menu_style_stack;
    ImGui::PushStyleColor(ImGuiCol_TextDisabled,   ImVec4{.9f,.3f,.3f,1}); ++menu_style_stack;
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   ImVec4{.3f,.3f,.6f,1}); ++menu_style_stack;
    if ( ImGui::BeginMainMenuBar() ) {
        if ( ImGui::BeginMenu(LOC(file)) ) {
            if ( ImGui::MenuItem(LOC(open_milton_canvas)) ) {

            }
            if ( ImGui::MenuItem(LOC(export_to_image_DOTS)) ) {
                milton_switch_mode(milton_state, MiltonMode_EXPORTING);
            }
            if ( ImGui::MenuItem(LOC(quit)) ) {
                milton_try_quit(milton_state);
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu(LOC(canvas), /*enabled=*/true) ) {
            if ( ImGui::MenuItem(LOC(set_background_color)) ) {
                i32 f = (i32)milton_state->gui->flags;
                set_flag(f, (int)MiltonGuiFlags_CHOOSING_BG_COLOR);
                milton_state->gui->flags = (MiltonGuiFlags)f;
            }
            ImGui::EndMenu();
        }
        if ( ImGui::BeginMenu(LOC(help)) ) {
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleColor(menu_style_stack);

    // TODO (IMPORTANT): Add a "reset UI" option? widgets might get outside the viewport without a way to get back.


    if ( milton_state->gui->visible ) {

        /* ImGuiSetCond_Always        = 1 << 0, // Set the variable */
        /* ImGuiSetCond_Once          = 1 << 1, // Only set the variable on the first call per runtime session */
        /* ImGuiSetCond_FirstUseEver */

        ImGui::SetNextWindowPos(ImVec2(10, 10 + (float)pbounds.bottom), ImGuiSetCond_Always);
        ImGui::SetNextWindowSize({271, 109}, ImGuiSetCond_Always);  // We don't want to set it *every* time, the user might have preferences

        b32 show_brush_window = (milton_state->current_mode == MiltonMode_PEN || milton_state->current_mode == MiltonMode_ERASER);

        if ( show_brush_window) {
            if ( ImGui::Begin(LOC(brushes), NULL, default_imgui_window_flags) ) {
                if ( milton_state->current_mode == MiltonMode_PEN ) {
                    float mut_alpha = pen_alpha;
                    ImGui::SliderFloat(LOC(opacity), &mut_alpha, 0.1f, 1.0f);
                    if ( mut_alpha != pen_alpha ) {
                        milton_set_pen_alpha(milton_state, mut_alpha);
                        i32 f = milton_state->gui->flags;
                        set_flag(f, (i32)MiltonGuiFlags_SHOWING_PREVIEW);
                        milton_state->gui->flags = (MiltonGuiFlags)f;
                    }
                }

                const auto size = milton_get_brush_size(milton_state);
                auto mut_size = size;

                ImGui::SliderInt(LOC(brush_size), &mut_size, 1, k_max_brush_size);

                if ( mut_size != size ) {
                    milton_set_brush_size(milton_state, mut_size);
                    i32 f = milton_state->gui->flags;
                    set_flag(f, (i32)MiltonGuiFlags_SHOWING_PREVIEW);
                    milton_state->gui->flags = (MiltonGuiFlags)f;
                }

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{1,1,1,1});
                {
                    if ( milton_state->current_mode != MiltonMode_PEN ) {
                        if (ImGui::Button(LOC(switch_to_pen))) {
                            i32 f = input->flags;
                            set_flag(f, MiltonInputFlags_CHANGE_MODE);
                            input->flags = (MiltonInputFlags)f;
                            input->mode_to_set = MiltonMode_PEN;
                        }
                    }

                    if ( milton_state->current_mode != MiltonMode_ERASER ) {
                        if (ImGui::Button(LOC(switch_to_eraser))) {
                            i32 f = input->flags;
                            set_flag(f, (i32)MiltonInputFlags_CHANGE_MODE);
                            input->flags = (MiltonInputFlags)f;
                            input->mode_to_set = MiltonMode_ERASER;
                        }
                    }
                }
                ImGui::PopStyleColor(1); // Pop white button text

                // ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
            }
            // Important to place this before ImGui::End()
            const v2i pos = {
                (i32)(ImGui::GetWindowPos().x + ImGui::GetWindowSize().x + milton_get_brush_size(milton_state)),
                (i32)(ImGui::GetWindowPos().y),
            };
            ImGui::End();  // Brushes
            if ( check_flag(milton_state->gui->flags, MiltonGuiFlags_SHOWING_PREVIEW) ) {
                milton_state->gui->preview_pos = pos;
            }
        }

        if ( check_flag(milton_state->gui->flags, MiltonGuiFlags_CHOOSING_BG_COLOR) ) {
            bool closed = false;
            if ( ImGui::Begin(LOC(choose_background_color), &closed, default_imgui_window_flags) ) {
                ImGui::SetWindowSize({271, 109}, ImGuiSetCond_Always);
                ImGui::Text("Sup");
                v3f bg = milton_state->view->background_color;
                if ( ImGui::ColorEdit3(LOC(background_color), bg.d) ) {
                    milton_state->view->background_color = clamp_01(bg);
                    i32 f = input->flags;
                    set_flag(f, (i32)MiltonInputFlags_FULL_REFRESH);
                    set_flag(f, (i32)MiltonInputFlags_FAST_DRAW);
                    input->flags = (MiltonInputFlags)f;
                }
                if ( closed ) {
                    i32 f = milton_state->gui->flags;
                    unset_flag(f, (i32)MiltonGuiFlags_CHOOSING_BG_COLOR);
                    milton_state->gui->flags = (MiltonGuiFlags)f;
                }
            }
            ImGui::End();
        }

    } // Visible

    // Note: The export window is drawn regardless of gui visibility.
    if ( milton_state->current_mode == MiltonMode_EXPORTING ) {
        bool opened = true;
        b32 reset = false;

        ImGui::SetNextWindowPos(ImVec2(100, 30), ImGuiSetCond_Once);
        ImGui::SetNextWindowSize({350, 235}, ImGuiSetCond_Always);  // We don't want to set it *every* time, the user might have preferences
        if ( ImGui::Begin(LOC(export_DOTS), &opened, ImGuiWindowFlags_NoCollapse) ) {
            ImGui::Text(LOC(MSG_click_and_drag_instruction));

            Exporter* exporter = &milton_state->gui->exporter;
            if ( exporter->state == ExporterState_SELECTED ) {
                i32 x = min( exporter->needle.x, exporter->pivot.x );
                i32 y = min( exporter->needle.y, exporter->pivot.y );
                int raster_w = abs(exporter->needle.x - exporter->pivot.x);
                int raster_h = abs(exporter->needle.y - exporter->pivot.y);

                ImGui::Text("%s: %dx%d\n",
                            LOC(current_selection),
                            raster_w, raster_h);
                if ( ImGui::InputInt(LOC(scale_up), &exporter->scale, 1, /*step_fast=*/2) ) {}
                if ( exporter->scale <= 0 ) exporter->scale = 1;
                i32 max_scale = milton_state->view->scale / 2;
                if ( exporter->scale > max_scale) {
                    exporter->scale = max_scale;
                }
                ImGui::Text("%s: %dx%d\n", LOC(final_image_size), raster_w*exporter->scale, raster_h*exporter->scale);

                if ( ImGui::Button(LOC(export_selection_to_image_DOTS)) ) {
                    // Render to buffer
                    int bpp = 4;  // bytes per pixel
                    i32 w = raster_w * exporter->scale;
                    i32 h = raster_h * exporter->scale;
                    size_t size = (size_t)w * h * bpp;
                    u8* buffer = (u8*)mlt_malloc(size);
                    if ( buffer ) {
                        opened = false;
                        milton_render_to_buffer(milton_state, buffer, x,y, raster_w, raster_h, exporter->scale);
                        char* fname = platform_save_dialog();
                        if ( fname ) {
                            milton_save_buffer_to_file(fname, buffer, w, h);
                        }
                        mlt_free (buffer);
                    } else {
                        platform_dialog(LOC(MSG_memerr_did_not_write), //""
                                        LOC(error));
                    }
                }
            }
        }

        if ( ImGui::Button(LOC(cancel)) ) {
            reset = true;
            milton_use_previous_mode(milton_state);
        }
        ImGui::End(); // Export...
        if ( !opened ) {
            reset = true;
            milton_use_previous_mode(milton_state);
        }
        if ( reset ) {
            exporter_init(&milton_state->gui->exporter);
        }
    }

    // Shortcut help. Also shown regardless of UI visibility.
    // TODO: Remove this.
    if ( milton_state->gui->show_help_widget ) {
        //bool opened;
        ImGui::SetNextWindowPos(ImVec2(365, 92), ImGuiSetCond_Always);
        ImGui::SetNextWindowSize({235, 235}, ImGuiSetCond_Always);  // We don't want to set it *every* time, the user might have preferences
        bool opened = true;
        if ( ImGui::Begin("Shortcuts", &opened, default_imgui_window_flags) ) {
            ImGui::TextWrapped(
                               "Increase brush size        ]\n"
                               "Decrease brush size        [\n"
                               "Pen                        b\n"
                               "Eraser                     e\n"
                               "10%%  opacity               1\n"
                               "20%%  opacity               2\n"
                               "30%%  opacity               3\n"
                               "             ...             \n"
                               "90%%  opacity               9\n"
                               "100%% opacity               0\n"
                               "\n"
                               "Show/Hide Help Window     F1\n"
                               "Toggle GUI Visibility     Tab\n"
                               "\n"
                               "\n"
                              );
            if ( !opened ) {
                milton_state->gui->show_help_widget = false;
            }
        }
        ImGui::End();  // Help
    }


    ImGui::PopStyleColor(color_stack);

}


#if defined(_WIN32) && defined(_MSC_VER)
#pragma warning(pop)
#endif  // _WIN32 && _MSC_VER