/* Taken from https://github.com/djpohly/dwl/issues/466 */
#define COLOR(hex)    { ((hex >> 24) & 0xFF) / 255.0f, \
                        ((hex >> 16) & 0xFF) / 255.0f, \
                        ((hex >> 8) & 0xFF) / 255.0f, \
                        (hex & 0xFF) / 255.0f }
/* appearance */
static const int sloppyfocus               = 0;  /* XP baseline: click to focus */
static const int bypass_surface_visibility = 0;  /* 1 means idle inhibitors will disable idle tracking even if it's surface isn't visible  */
static const unsigned int borderpx         = 3;  /* border pixel of windows */
static const float rootcolor[]             = COLOR(0x222222ff);
static const float bordercolor[]           = COLOR(0x7a96dfff);
static const float focuscolor[]            = COLOR(0x0054e3ff);
static const float urgentcolor[]           = COLOR(0xff0000ff);
/* This conforms to the xdg-protocol. Set the alpha to zero to restore the old behavior */
static const float fullscreen_bg[]         = {0.1f, 0.1f, 0.1f, 1.0f}; /* You can also use glsl colors */

/* tagging - TAGCOUNT must be no greater than 31 */
#define TAGCOUNT (11) /* Ten workspaces plus the special-workspace tag. */

/* logging */
static int log_level = WLR_ERROR;

/* NOTE: ALWAYS keep a rule declared even if you don't use rules (e.g leave at least one example) */
static const Rule rules[] = {
	/* app_id             title       tags mask     isfloating   monitor */
	/* examples: */
	{ "Gimp_EXAMPLE",     NULL,       0,            1,           -1 }, /* Start on currently visible tags floating, not tiled */
	{ "firefox_EXAMPLE",  NULL,       1 << 8,       0,           -1 }, /* Start on ONLY tag "9" */
};

/* layout(s) */
static const Layout layouts[] = {
	/* symbol     arrange function */
	{ "WM Mode",       tile },
	{ "Floating Mode", NULL }, /* XP default */
	{ "[M]",      monocle },
};

/* monitors */
/* (x=-1, y=-1) is reserved as an "autoconfigure" monitor position indicator
 * WARNING: negative values other than (-1, -1) cause problems with Xwayland clients
 * https://gitlab.freedesktop.org/xorg/xserver/-/issues/899
*/
/* NOTE: ALWAYS add a fallback rule, even if you are completely sure it won't be used */
static const MonitorRule monrules[] = {
	/* name       mfact  nmaster scale layout       rotate/reflect                x    y */
	/* example of a HiDPI laptop monitor:
	{ "eDP-1",    0.5f,  1,      2,    &layouts[0], WL_OUTPUT_TRANSFORM_NORMAL,   -1,  -1 },
	*/
	/* defaults */
	{ NULL,       0.55f, 1,      1,    &layouts[1], WL_OUTPUT_TRANSFORM_NORMAL,   -1,  -1 },
};

/* keyboard */
static const struct xkb_rule_names xkb_rules = {
	/* can specify fields: rules, model, layout, variant, options */
	/* example:
	.options = "ctrl:nocaps",
	*/
	.options = NULL,
};

static const int repeat_rate = 25;
static const int repeat_delay = 600;

/* Trackpad */
static const int tap_to_click = 1;
static const int tap_and_drag = 1;
static const int drag_lock = 1;
static const int natural_scrolling = 0;
static const int disable_while_typing = 1;
static const int left_handed = 0;
static const int middle_button_emulation = 0;
/* You can choose between:
LIBINPUT_CONFIG_SCROLL_NO_SCROLL
LIBINPUT_CONFIG_SCROLL_2FG
LIBINPUT_CONFIG_SCROLL_EDGE
LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN
*/
static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;

/* You can choose between:
LIBINPUT_CONFIG_CLICK_METHOD_NONE
LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS
LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER
*/
static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

/* You can choose between:
LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE
*/
static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

/* You can choose between:
LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE
*/
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static const double accel_speed = 0.0;

/* You can choose between:
LIBINPUT_CONFIG_TAP_MAP_LRM -- 1/2/3 finger tap maps to left/right/middle
LIBINPUT_CONFIG_TAP_MAP_LMR -- 1/2/3 finger tap maps to left/middle/right
*/
static const enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;

/* If you want to use the windows key for MODKEY, use WLR_MODIFIER_LOGO */
#define MODKEY WLR_MODIFIER_LOGO

/* Commands are evaluated at keypress time, using the user's environment. */
#define SHCMD(cmd) { .v = (const char*[]){ "/bin/sh", "-c", cmd, NULL } }

static const Key keys[] = {
	{ 0, XKB_KEY_Print, spawn, SHCMD("\"$HOME/.config/hypr/scripts/screenshot.sh\" full") },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_S, spawn, SHCMD("\"$HOME/.config/hypr/scripts/screenshot.sh\" area") },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_F, spawn, SHCMD("grim -g \"$(slurp)\" - | tee ~/Pictures/screenshot-$(date +%Y%m%d-%H%M%S).png | wl-copy") },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_X, spawn, SHCMD("hyprpicker -a") },
	{ WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT, XKB_KEY_l, spawn, SHCMD("loginctl lock-session") },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_L, spawn, SHCMD("loginctl lock-session") },
	{ MODKEY, XKB_KEY_Return, spawn, SHCMD("kitty") },
	{ MODKEY, XKB_KEY_e, spawn, SHCMD("thunar") },
	{ MODKEY, XKB_KEY_d, spawn, SHCMD("qs -c hypraero ipc call hypraero toggleStart || fuzzel") },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_D, spawn, SHCMD("fuzzel") },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_B, spawn, SHCMD("pkill -HUP quickshell") },
	{ 0, XKB_KEY_XF86AudioRaiseVolume, spawn, SHCMD("wpctl set-volume -l 1.4 @DEFAULT_AUDIO_SINK@ 5%+") },
	{ 0, XKB_KEY_XF86AudioLowerVolume, spawn, SHCMD("wpctl set-volume -l 1.4 @DEFAULT_AUDIO_SINK@ 5%-") },
	{ 0, XKB_KEY_XF86AudioMute, spawn, SHCMD("wpctl set-mute @DEFAULT_AUDIO_SINK@ toggle") },
	{ 0, XKB_KEY_XF86AudioMicMute, spawn, SHCMD("pactl -- set-source-mute 0 toggle") },
	{ 0, XKB_KEY_XF86MonBrightnessDown, spawn, SHCMD("brightnessctl s 10%-") },
	{ 0, XKB_KEY_XF86MonBrightnessUp, spawn, SHCMD("brightnessctl s +10%") },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Q, killclient, {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_M, quit, {0} },
	{ MODKEY, XKB_KEY_f, togglefullscreen, {0} },
	{ MODKEY, XKB_KEY_space, togglefloating, {0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_space, xpallfloat, {0} },
	{ MODKEY, XKB_KEY_Tab, focusstack, {.i = 1} },
	{ WLR_MODIFIER_ALT, XKB_KEY_Tab, focusstack, {.i = 1} },
	{ WLR_MODIFIER_ALT|WLR_MODIFIER_SHIFT, XKB_KEY_Tab, focusstack, {.i = -1} },
	{ MODKEY, XKB_KEY_c, xpgeometry, {.i = 8} },
	{ MODKEY, XKB_KEY_a, toggleview, {.ui = 1u << 10} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_A, tag, {.ui = 1u << 10} },
	{ MODKEY, XKB_KEY_h, xpfocus, {.i = 0} },
	{ MODKEY, XKB_KEY_l, xpfocus, {.i = 1} },
	{ MODKEY, XKB_KEY_k, xpfocus, {.i = 2} },
	{ MODKEY, XKB_KEY_j, xpfocus, {.i = 3} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Left, xpgeometry, {.i = 0} },
	{ MODKEY|WLR_MODIFIER_CTRL, XKB_KEY_Left, xpgeometry, {.i = 4} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Right, xpgeometry, {.i = 1} },
	{ MODKEY|WLR_MODIFIER_CTRL, XKB_KEY_Right, xpgeometry, {.i = 5} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Up, xpgeometry, {.i = 2} },
	{ MODKEY|WLR_MODIFIER_CTRL, XKB_KEY_Up, xpgeometry, {.i = 6} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_Down, xpgeometry, {.i = 3} },
	{ MODKEY|WLR_MODIFIER_CTRL, XKB_KEY_Down, xpgeometry, {.i = 7} },
	{ MODKEY, XKB_KEY_1, view, {.ui = 1u << 0} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_exclam, xptagfollow, {.ui = 1u << 0} },
	{ MODKEY, XKB_KEY_2, view, {.ui = 1u << 1} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_at, xptagfollow, {.ui = 1u << 1} },
	{ MODKEY, XKB_KEY_3, view, {.ui = 1u << 2} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_numbersign, xptagfollow, {.ui = 1u << 2} },
	{ MODKEY, XKB_KEY_4, view, {.ui = 1u << 3} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_dollar, xptagfollow, {.ui = 1u << 3} },
	{ MODKEY, XKB_KEY_5, view, {.ui = 1u << 4} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_percent, xptagfollow, {.ui = 1u << 4} },
	{ MODKEY, XKB_KEY_6, view, {.ui = 1u << 5} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_asciicircum, xptagfollow, {.ui = 1u << 5} },
	{ MODKEY, XKB_KEY_7, view, {.ui = 1u << 6} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_ampersand, xptagfollow, {.ui = 1u << 6} },
	{ MODKEY, XKB_KEY_8, view, {.ui = 1u << 7} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_asterisk, xptagfollow, {.ui = 1u << 7} },
	{ MODKEY, XKB_KEY_9, view, {.ui = 1u << 8} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_parenleft, xptagfollow, {.ui = 1u << 8} },
	{ MODKEY, XKB_KEY_0, view, {.ui = 1u << 9} },
	{ MODKEY|WLR_MODIFIER_SHIFT, XKB_KEY_parenright, xptagfollow, {.ui = 1u << 9} },
	{ MODKEY|WLR_MODIFIER_ALT, XKB_KEY_Up, xpworkspace, {.i = 1} },
	{ MODKEY|WLR_MODIFIER_ALT, XKB_KEY_Down, xpworkspace, {.i = -1} },
#define CHVT(n) { WLR_MODIFIER_CTRL|WLR_MODIFIER_ALT, XKB_KEY_XF86Switch_VT_##n, chvt, {.ui = (n)} }
	CHVT(1), CHVT(2), CHVT(3), CHVT(4), CHVT(5), CHVT(6),
	CHVT(7), CHVT(8), CHVT(9), CHVT(10), CHVT(11), CHVT(12),
};

static const Button buttons[] = {
	{ MODKEY, BTN_LEFT, moveresize, {.ui = CurMove} },
	{ MODKEY, BTN_RIGHT, moveresize, {.ui = CurResize} },
};
