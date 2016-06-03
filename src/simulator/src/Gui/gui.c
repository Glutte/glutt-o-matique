#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <math.h>
#include <time.h>
#include <limits.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#define NK_XLIB_GL3_IMPLEMENTATION
#define NK_XLIB_LOAD_OPENGL_EXTENSIONS
#include "nuklear.h"
#include "nuklear_xlib_gl3.h"

#define WINDOW_WIDTH 1200
#define WINDOW_HEIGHT 800

#define MAX_VERTEX_BUFFER 512 * 1024
#define MAX_ELEMENT_BUFFER 128 * 1024

#define UNUSED(a) (void)a
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) < (b) ? (b) : (a))
#define LEN(a) (sizeof(a)/sizeof(a)[0])


/**
 * USART
 **/
char uart_recv_txt[4096];
static char uart_send_txt[512];
static int uart_send_txt_len;
void gui_usart_send(char*);

/**
 * Leds
 **/
char led_blue = 0;
char led_green = 0;
char led_orange = 0;
char led_red = 0;

/**
 * GPS
 */
int gui_gps_send_frame = 1;
int gui_gps_frames_valid = 1;
char gui_gps_lat[64] = "4719.59788";
int gui_gps_lat_len = 10;
static const char *gps_NS[] = {"North", "South"};
int gui_gps_lat_hem = 0;
char gui_gps_lon[64] = "00830.92084";
int gui_gps_lon_len = 10;
static const char *gps_EW[] = {"East", "West"};
int gui_gps_lon_hem = 0;
int gui_gps_send_current_time = 1;

struct XWindow {
    Display *dpy;
    Window win;
    XVisualInfo *vis;
    Colormap cmap;
    XSetWindowAttributes swa;
    XWindowAttributes attr;
    GLXFBConfig fbc;
    int width, height;
};

static int gl_err = FALSE;
static int gl_error_handler(Display *dpy, XErrorEvent *ev) {
    UNUSED((dpy, ev));
    gl_err = TRUE;
    return 0;
}

static void die(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputs("\n", stderr);
    exit(EXIT_FAILURE);
}

static int has_extension(const char *string, const char *ext) {

    const char *start, *where, *term;
    where = strchr(ext, ' ');

    if (where || *ext == '\0')
        return FALSE;

    for (start = string;;) {
        where = strstr((const char*)start, ext);
        if (!where) break;
        term = where + strlen(ext);
        if (where == start || *(where - 1) == ' ') {
            if (*term == ' ' || *term == '\0')
                return TRUE;
        }
        start = term;
    }
    return FALSE;
}


int main_gui() {
    /* Platform */
    int running = 1;
    struct XWindow win;
    GLXContext glContext;
    struct nk_context *ctx;
    struct nk_color background;

    memset(&win, 0, sizeof(win));
    win.dpy = XOpenDisplay(NULL);

    if (!win.dpy) {
        die("Failed to open X display\n");
    }

    {
        /* check glx version */
        int glx_major, glx_minor;
        if (!glXQueryVersion(win.dpy, &glx_major, &glx_minor))
            die("[X11]: Error: Failed to query OpenGL version\n");
        if ((glx_major == 1 && glx_minor < 3) || (glx_major < 1))
            die("[X11]: Error: Invalid GLX version!\n");
    }

    {
        /* find and pick matching framebuffer visual */
        int fb_count;
        static GLint attr[] = {
            GLX_X_RENDERABLE,   True,
            GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
            GLX_RENDER_TYPE,    GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
            GLX_RED_SIZE,       8,
            GLX_GREEN_SIZE,     8,
            GLX_BLUE_SIZE,      8,
            GLX_ALPHA_SIZE,     8,
            GLX_DEPTH_SIZE,     24,
            GLX_STENCIL_SIZE,   8,
            GLX_DOUBLEBUFFER,   True,
            None
        };
        GLXFBConfig *fbc;
        fbc = glXChooseFBConfig(win.dpy, DefaultScreen(win.dpy), attr, &fb_count);

        if (!fbc) {
            die("[X11]: Error: failed to retrieve framebuffer configuration\n");
        }

        {
            /* pick framebuffer with most samples per pixel */
            int i;
            int fb_best = -1;
            int best_num_samples = -1;

            for (i = 0; i < fb_count; ++i) {
                XVisualInfo *vi = glXGetVisualFromFBConfig(win.dpy, fbc[i]);
                if (vi) {
                    int sample_buffer, samples;
                    glXGetFBConfigAttrib(win.dpy, fbc[i], GLX_SAMPLE_BUFFERS, &sample_buffer);
                    glXGetFBConfigAttrib(win.dpy, fbc[i], GLX_SAMPLES, &samples);
                    if ((fb_best < 0) || (sample_buffer && samples > best_num_samples)) {
                        fb_best = i; best_num_samples = samples;
                    }
                }
            }
            win.fbc = fbc[fb_best];
            XFree(fbc);
            win.vis = glXGetVisualFromFBConfig(win.dpy, win.fbc);
        }
    }
    {
        /* create window */
        win.cmap = XCreateColormap(win.dpy, RootWindow(win.dpy, win.vis->screen), win.vis->visual, AllocNone);
        win.swa.colormap =  win.cmap;
        win.swa.background_pixmap = None;
        win.swa.border_pixel = 0;
        win.swa.event_mask =
            ExposureMask | KeyPressMask | KeyReleaseMask |
            ButtonPress | ButtonReleaseMask| ButtonMotionMask |
            Button1MotionMask | Button3MotionMask | Button4MotionMask | Button5MotionMask|
            PointerMotionMask| StructureNotifyMask;
        win.win = XCreateWindow(win.dpy, RootWindow(win.dpy, win.vis->screen), 0, 0,
                WINDOW_WIDTH, WINDOW_HEIGHT, 0, win.vis->depth, InputOutput,
                win.vis->visual, CWBorderPixel|CWColormap|CWEventMask, &win.swa);
        if (!win.win) die("[X11]: Failed to create window\n");
        XFree(win.vis);
        XStoreName(win.dpy, win.win, "glutt-o-matique simulator 3000");
        XMapWindow(win.dpy, win.win);
    }
    {
        /* create opengl context */
        typedef GLXContext(*glxCreateContext)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
        int(*old_handler)(Display*, XErrorEvent*) = XSetErrorHandler(gl_error_handler);
        const char *extensions_str = glXQueryExtensionsString(win.dpy, DefaultScreen(win.dpy));
        glxCreateContext create_context = (glxCreateContext)
            glXGetProcAddressARB((const GLubyte*)"glXCreateContextAttribsARB");

        gl_err = FALSE;
        if (!has_extension(extensions_str, "GLX_ARB_create_context") || !create_context) {
            fprintf(stdout, "[X11]: glXCreateContextAttribARB() not found...\n");
            fprintf(stdout, "[X11]: ... using old-style GLX context\n");
            glContext = glXCreateNewContext(win.dpy, win.fbc, GLX_RGBA_TYPE, 0, True);
        } else {
            GLint attr[] = {
                GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
                GLX_CONTEXT_MINOR_VERSION_ARB, 0,
                None
            };
            glContext = create_context(win.dpy, win.fbc, 0, True, attr);
            XSync(win.dpy, False);
            if (gl_err || !glContext) {
                /* Could not create GL 3.0 context. Fallback to old 2.x context.
                 * If a version below 3.0 is requested, implementations will
                 * return the newest context version compatible with OpenGL
                 * version less than version 3.0.*/
                attr[1] = 1; attr[3] = 0;
                gl_err = FALSE;
                fprintf(stdout, "[X11] Failed to create OpenGL 3.0 context\n");
                fprintf(stdout, "[X11] ... using old-style GLX context!\n");
                glContext = create_context(win.dpy, win.fbc, 0, True, attr);
            }
        }
        XSync(win.dpy, False);
        XSetErrorHandler(old_handler);
        if (gl_err || !glContext)
            die("[X11]: Failed to create an OpenGL context\n");
        glXMakeCurrent(win.dpy, win.win, glContext);
    }

    ctx = nk_x11_init(win.dpy, win.win);
    /* Load Fonts: if none of these are loaded a default font will be used  */
    {struct nk_font_atlas *atlas;
        nk_x11_font_stash_begin(&atlas);
        nk_x11_font_stash_end();
    }


    background = nk_rgb(28,48,62);

    while (running)
    {
        taskYIELD();

        vTaskSuspendAll();

        /* Input */
        XEvent evt;
        nk_input_begin(ctx);
        while (XCheckWindowEvent(win.dpy, win.win, win.swa.event_mask, &evt)){
            if (XFilterEvent(&evt, win.win)) continue;
            nk_x11_handle_event(&evt);
        }
        nk_input_end(ctx);

        /* GUI */
        {

            struct nk_panel layout;

            if (nk_begin(ctx, &layout, "UART", nk_rect(50, 50, 400, 300), NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_SCALABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {


                nk_menubar_begin(ctx);
                nk_layout_row_begin(ctx, NK_STATIC, 25, 2);
                nk_layout_row_push(ctx, 280);

                int active = nk_edit_string(ctx, NK_EDIT_FIELD|NK_EDIT_SIG_ENTER, uart_send_txt, &uart_send_txt_len, 512,  nk_filter_default);

                nk_layout_row_push(ctx, 70);
                if (nk_button_label(ctx, "Send", NK_BUTTON_DEFAULT) || (active & NK_EDIT_COMMITED)) {

                    uart_send_txt[uart_send_txt_len] = '\0';

                    gui_usart_send(uart_send_txt);

                    uart_send_txt[0] = '\0';
                    uart_send_txt_len = 0;
                }

                nk_menubar_end(ctx);

                nk_layout_row_dynamic(ctx, 25, 1);

                nk_label(ctx, "UART Output:", NK_TEXT_LEFT);

                nk_layout_row_dynamic(ctx, 16, 1);

                char * current_pointer = uart_recv_txt;
                int l = 0;

                while (*current_pointer != 0) {

                    if (*current_pointer == '\n') {
                        if (l > 1) {
                            nk_text(ctx, current_pointer - l, l, NK_TEXT_LEFT);
                        }
                        current_pointer++;
                        l = 0;
                    }
                    current_pointer++;
                    l++;
                }

                if (l > 1) {
                    nk_text(ctx, current_pointer - l, l, NK_TEXT_LEFT);
                }
            }
            nk_end(ctx);


            if (nk_begin(ctx, &layout, "LEDs", nk_rect(460, 50, 100, 155), NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {

                nk_layout_row_static(ctx, 20, 20, 3);

                struct nk_color color;

                nk_text(ctx, "", 0, NK_TEXT_LEFT);

                color.r = 255; color.g = 0; color.b = 0;

                if (led_red == 1) {
                    color.a = 255;
                } else {
                    color.a = 30;
                }
                nk_button_color(ctx, color, NK_BUTTON_DEFAULT);

                nk_text(ctx, "", 0, NK_TEXT_LEFT);

                color.r = 0; color.g = 255; color.b = 0;

                if (led_green == 1) {
                    color.a = 255;
                } else {
                    color.a = 30;
                }
                nk_button_color(ctx, color, NK_BUTTON_DEFAULT);

                nk_text(ctx, "", 0, NK_TEXT_LEFT);

                color.r = 255; color.g = 165; color.b = 0;

                if (led_orange == 1) {
                    color.a = 255;
                } else {
                    color.a = 30;
                }
                nk_button_color(ctx, color, NK_BUTTON_DEFAULT);

                nk_text(ctx, "", 0, NK_TEXT_LEFT);

                color.r = 0; color.g = 0; color.b = 255;

                if (led_blue == 1) {
                    color.a = 255;
                } else {
                    color.a = 30;
                }
                nk_button_color(ctx, color, NK_BUTTON_DEFAULT);

                nk_text(ctx, "", 0, NK_TEXT_LEFT);

            }
            nk_end(ctx);

            if (nk_begin(ctx, &layout, "GPS", nk_rect(460, 210, 200, 455), NK_WINDOW_BORDER|NK_WINDOW_MOVABLE|NK_WINDOW_MINIMIZABLE|NK_WINDOW_TITLE)) {

                nk_layout_row_dynamic(ctx, 30, 1);
                nk_checkbox_label(ctx, "Send frames", &gui_gps_send_frame);
                nk_checkbox_label(ctx, "Valid frames", &gui_gps_frames_valid);

                nk_layout_row_dynamic(ctx, 30, 2);

                nk_label(ctx, "Latitude:", NK_TEXT_LEFT);
                nk_edit_string(ctx, NK_EDIT_SIMPLE, gui_gps_lat, &gui_gps_lat_len, 64, nk_filter_float);
                nk_label(ctx, "", NK_TEXT_LEFT);
                gui_gps_lat_hem = nk_combo(ctx, gps_NS, LEN(gps_NS), gui_gps_lat_hem, 30);

                nk_label(ctx, "Longitude:", NK_TEXT_LEFT);
                nk_edit_string(ctx, NK_EDIT_SIMPLE, gui_gps_lon, &gui_gps_lon_len, 64, nk_filter_float);
                nk_label(ctx, "", NK_TEXT_LEFT);
                gui_gps_lon_hem = nk_combo(ctx, gps_EW, LEN(gps_EW), gui_gps_lon_hem, 30);

                nk_layout_row_dynamic(ctx, 30, 1);
                nk_checkbox_label(ctx, "Send current time", &gui_gps_send_current_time);

            }
            nk_end(ctx);


        }
        /* if (nk_window_is_closed(ctx, "Demo")) break; */

        {
            float bg[4];
            nk_color_fv(bg, background);
            XGetWindowAttributes(win.dpy, win.win, &win.attr);
            glViewport(0, 0, win.width, win.height);
            glClear(GL_COLOR_BUFFER_BIT);
            glClearColor(bg[0], bg[1], bg[2], bg[3]);
            nk_x11_render(NK_ANTI_ALIASING_ON, MAX_VERTEX_BUFFER, MAX_ELEMENT_BUFFER);
            glXSwapBuffers(win.dpy, win.win);
        }

        xTaskResumeAll();
    }

    nk_x11_shutdown();
    glXMakeCurrent(win.dpy, 0, 0);
    glXDestroyContext(win.dpy, glContext);
    XUnmapWindow(win.dpy, win.win);
    XFreeColormap(win.dpy, win.cmap);
    XDestroyWindow(win.dpy, win.win);
    XCloseDisplay(win.dpy);
    return 0;

}
