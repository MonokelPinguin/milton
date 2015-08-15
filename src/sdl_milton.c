//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


#include "SDL.h"
#include "SDL_syswm.h"

#define MILTON_DESKTOP
#include "system_includes.h"

int milton_main();


#define func static
#include "define_types.h"


typedef struct TabletState_s TabletState;

#if defined(_WIN32)
#include "platform_windows.h"
#elif defined(__linux__) || defined(__MACH__)
#include "platform_unix.h"
#endif

#include "memory.h"

#include "milton.h"


typedef struct PlatformInput_s
{
    b32 is_ctrl_down;
    b32 is_space_down;
    b32 is_pointer_down;  // Left click or wacom input
    v2i pan_start;
    v2i pan_point;
} PlatformInput;

// Called periodically to force updates that don't depend on user input.
func u32 timer_callback(u32 interval, void *param)
{
    SDL_Event event;
    SDL_UserEvent userevent;

    userevent.type = SDL_USEREVENT;
    userevent.code = 0;
    userevent.data1 = NULL;
    userevent.data2 = NULL;

    event.type = SDL_USEREVENT;
    event.user = userevent;

    SDL_PushEvent(&event);
    return(interval);
}

int milton_main()
{
    // Note: Possible crash regarding SDL_main entry point.
    // Note: Event handling, File I/O and Threading are initialized by default
    SDL_Init(SDL_INIT_VIDEO);

    i32 width = 1280;
    i32 height = 800;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow("Milton",
                                          SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                          width, height,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    //SDL_MaximizeWindow(window);

    if (!window)
    {
        milton_log("[ERROR] -- Exiting. SDL could not create window\n");
        exit(EXIT_FAILURE);
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    if (!gl_context)
    {
        milton_fatal("Could not create OpenGL context\n");
    }

    platform_load_gl_func_pointers();

    TabletState tablet_state = { 0 };
    platform_wacom_init(&tablet_state, window);

    {
        milton_log("Created OpenGL context with version %s\n", glGetString(GL_VERSION));
        milton_log("    and GLSL %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));
    }

    // ==== Initialize milton
    //  Total memory requirement for Milton
    size_t total_memory_size = min((size_t)2 * 1024 * 1024 * 1024, get_system_RAM() / 2);
    //  Size of frame heap
    size_t frame_heap_in_MB  = 128 * 1024 * 1024;

    void* big_chunk_of_memory = platform_allocate(total_memory_size);

    assert (big_chunk_of_memory);

    Arena root_arena      = arena_init(big_chunk_of_memory, total_memory_size);
    Arena transient_arena = arena_spawn(&root_arena, frame_heap_in_MB);

    MiltonState* milton_state = arena_alloc_elem(&root_arena, MiltonState);
    {
        milton_state->root_arena = &root_arena;
        milton_state->transient_arena = &transient_arena;

        milton_init(milton_state);
    }

    PlatformInput platform_input = { 0 };

    b32 should_quit = false;
    MiltonInput milton_input = { 0 };
    milton_input.flags |= MiltonInputFlags_FULL_REFRESH;  // Full draw on first launch
    milton_resize(milton_state, (v2i){0}, (v2i){width, height});

    u32 window_id = SDL_GetWindowID(window);
    v2i input_point = { 0 };

    // Every 100ms, call this callback to send us an event so we don't wait for user input.
    SDL_AddTimer(100, timer_callback, NULL);

    while(!should_quit)
    {
        // ==== Handle events

        milton_input.pressure = 1.0f;  // Reset until we receive something else

        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            SDL_Keymod keymod = SDL_GetModState();
            platform_input.is_ctrl_down = (keymod & KMOD_LCTRL) | (keymod & KMOD_RCTRL);

            switch (event.type)
            {
            case SDL_QUIT:
                {
                    should_quit = true;
                    break;
                }
            case SDL_MOUSEBUTTONDOWN:
                {
                    if (event.button.windowID != window_id)
                    {
                        break;
                    }
                    if (event.button.button == SDL_BUTTON_LEFT)
                    {
                        platform_input.is_pointer_down = true;
                        if (platform_input.is_space_down)
                        {
                            platform_input.pan_start = (v2i) { event.button.x, event.button.y };
                        }
                        else
                        {
                            input_point = (v2i){ event.motion.x, event.motion.y };
                            milton_input.point = &input_point;
                        }
                    }
                    break;
                }
            case SDL_MOUSEBUTTONUP:
                {
                    if (event.button.windowID != window_id)
                    {
                        break;
                    }
                    if (event.button.button == SDL_BUTTON_LEFT)
                    {
                        platform_input.is_pointer_down = false;
                        milton_input.flags |= MiltonInputFlags_END_STROKE;
                    }
                    break;
                }
            case SDL_MOUSEMOTION:
                {
                    if (event.motion.windowID != window_id)
                    {
                        break;
                    }

                    input_point = (v2i){ event.motion.x, event.motion.y };
                    if (platform_input.is_pointer_down)
                    {
                        if (!platform_input.is_space_down)
                        {
                            milton_input.point = &input_point;
                        }
                        else if (platform_input.is_space_down)
                        {
                            platform_input.pan_point = (v2i){ event.motion.x, event.motion.y };

                            milton_input.flags |= MiltonInputFlags_FAST_DRAW;
                            milton_input.flags |= MiltonInputFlags_FULL_REFRESH;
                        }
                    }
                    else if (!milton_input.point)
                    {
                        milton_input.hover_point = &input_point;
                    }
                    break;
                }
            case SDL_MOUSEWHEEL:
                {
                    if (event.wheel.windowID != window_id)
                    {
                        break;
                    }

                    milton_input.scale += event.wheel.y;
                    milton_input.flags |= MiltonInputFlags_FAST_DRAW;

                    break;
                }
            case SDL_KEYDOWN:
                {
                    if (event.wheel.windowID != window_id)
                    {
                        break;
                    }

                    SDL_Keycode keycode = event.key.keysym.sym;

                    // Actions accepting key repeats.
                    {
                        if (keycode == SDLK_LEFTBRACKET)
                        {
                            milton_decrease_brush_size(milton_state);
                        }
                        else if (keycode == SDLK_RIGHTBRACKET)
                        {
                            milton_increase_brush_size(milton_state);
                        }
                        if (platform_input.is_ctrl_down)
                        {
                            if (keycode == SDLK_z)
                            {
                                milton_input.flags |= MiltonInputFlags_UNDO;
                            }
                            if (keycode == SDLK_y)
                            {
                                milton_input.flags |= MiltonInputFlags_REDO;
                            }
                        }
                    }

                    if (event.key.repeat)
                    {
                        break;
                    }

                    if (keycode == SDLK_ESCAPE)
                    {
                        should_quit = true;
                    }
                    if (keycode == SDLK_SPACE)
                    {
                        platform_input.is_space_down = true;
                    }
                    if (platform_input.is_ctrl_down)
                    {
                        if (keycode == SDLK_BACKSPACE)
                        {
                            milton_input.flags |= MiltonInputFlags_RESET;
                        }
                    }
                    else
                    {
                        if (keycode == SDLK_e)
                        {
                            milton_input.flags |= MiltonInputFlags_SET_MODE_ERASER;
                        }
                        else if (keycode == SDLK_b)
                        {
                            milton_input.flags |= MiltonInputFlags_SET_MODE_BRUSH;
                        }
                        else if (keycode == SDLK_1)
                        {
                            milton_set_pen_alpha(milton_state, 0.1f);
                        }
                        else if (keycode == SDLK_2)
                        {
                            milton_set_pen_alpha(milton_state, 0.2f);
                        }
                        else if (keycode == SDLK_3)
                        {
                            milton_set_pen_alpha(milton_state, 0.3f);
                        }
                        else if (keycode == SDLK_4)
                        {
                            milton_set_pen_alpha(milton_state, 0.4f);
                        }
                        else if (keycode == SDLK_5)
                        {
                            milton_set_pen_alpha(milton_state, 0.5f);
                        }
                        else if (keycode == SDLK_6)
                        {
                            milton_set_pen_alpha(milton_state, 0.6f);
                        }
                        else if (keycode == SDLK_7)
                        {
                            milton_set_pen_alpha(milton_state, 0.7f);
                        }
                        else if (keycode == SDLK_8)
                        {
                            milton_set_pen_alpha(milton_state, 0.8f);
                        }
                        else if (keycode == SDLK_9)
                        {
                            milton_set_pen_alpha(milton_state, 0.9f);
                        }
                        else if (keycode == SDLK_0)
                        {
                            milton_set_pen_alpha(milton_state, 1.0f);
                        }
                    }

                    break;
                }
            case SDL_KEYUP:
                {
                    if (event.wheel.windowID != window_id)
                    {
                        break;
                    }

                    SDL_Keycode keycode = event.key.keysym.sym;

                    if (keycode == SDLK_SPACE)
                    {
                        platform_input.is_space_down = false;
                    }
                    break;
                }
            case SDL_WINDOWEVENT:
                {
                    if (window_id != event.window.windowID)
                    {
                        break;
                    }
                    switch (event.window.event)
                    {
                        // Just handle every event that changes the window size.
                    case SDL_WINDOWEVENT_RESIZED:
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        {
                            width = event.window.data1;
                            height = event.window.data2;
                            milton_input.flags |= MiltonInputFlags_FULL_REFRESH;
                            glViewport(0, 0, width, height);
                            break;
                        }
                    case SDL_WINDOWEVENT_LEAVE:
                        {
                            if (event.window.windowID != window_id)
                            {
                                break;
                            }
                            if (platform_input.is_pointer_down)
                            {
                                platform_input.is_pointer_down = false;
                                milton_input.flags |= MiltonInputFlags_END_STROKE;
                            }
                            break;
                        }
                    default:
                        break;
                    }
                }
            case SDL_SYSWMEVENT:
                {
                    if (milton_input.point)
                    {
                        f32 pressure = platform_sdl_wmevent(&tablet_state, event.syswm);
                        if (pressure > 0)
                        {
                            milton_input.pressure = pressure;
                        }
                    }
                    break;
                }
            default:
                break;
            }
            if (should_quit)
            {
                break;
            }
        }

        milton_input.is_panning = platform_input.is_space_down;

        // The extra point would be ignored by the renderer, causing artifacts.
        if (milton_input.flags & MiltonInputFlags_END_STROKE)
        {
            milton_input.point = NULL;
        }

        v2i pan_delta = sub_v2i(platform_input.pan_point, platform_input.pan_start);
        if (pan_delta.x != 0 ||
            pan_delta.y != 0 ||
            width != milton_state->view->screen_size.x ||
            height != milton_state->view->screen_size.y)
        {
            milton_resize(milton_state, pan_delta, (v2i){width, height});
        }
        platform_input.pan_start = platform_input.pan_point;
        // ==== Update and render
        milton_update(milton_state, &milton_input);
        milton_gl_backend_draw(milton_state);
        SDL_GL_SwapWindow(window);
        SDL_WaitEvent(NULL);

        milton_input = (MiltonInput){0};
    }

    platform_wacom_deinit(&tablet_state);

    // Release pages. Not really necessary but we don't want to piss off leak
    // detectors, do we?
    platform_deallocate(big_chunk_of_memory);

    SDL_Quit();

    return 0;
}
