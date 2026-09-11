/* Temporary recreated Luna drawing. All metrics await XP resource validation. */
#ifndef XPWM_DECORATION_H
#define XPWM_DECORATION_H
#define XP_CAPTION 26
#define XP_BUTTON 21
#define XP_GAP 2
#define XP_MIN_WIDTH 120
struct wlr_buffer *xp_caption_buffer(int width, const char *title, int active,
    int maximized, int hot, int pressed);
#endif
