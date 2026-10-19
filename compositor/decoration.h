/* Resource-backed Luna decorations with recreated drawing as a fallback. */
#ifndef XPWM_DECORATION_H
#define XPWM_DECORATION_H
#define XP_CAPTION 26
#define XP_BUTTON 21
#define XP_GAP 2
#define XP_MIN_WIDTH 120
struct wlr_buffer *xp_caption_buffer(int width, const char *title, int active,
    int maximized, int hot, int pressed, int border, int can_maximize);
struct wlr_buffer *xp_frame_buffer(int width, int height, const char *role, int active);
struct wlr_buffer *xp_switcher_buffer(int width, int columns, int count, int selected, const char *title);
struct wlr_buffer *xp_system_menu_buffer(int selected, unsigned int enabled);
#endif
