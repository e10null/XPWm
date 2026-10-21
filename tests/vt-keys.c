#define main dwl_original_main
#include "../compositor/dwl.c"
#undef main
#include <assert.h>

int main(void)
{
	unsigned int vt;
	uint32_t mods = WLR_MODIFIER_CTRL | WLR_MODIFIER_ALT;
	for (vt = 1; vt <= 12; vt++) {
		assert(vtnumber(mods, XKB_KEY_F1 + vt - 1) == vt);
		assert(vtnumber(0, XKB_KEY_XF86Switch_VT_1 + vt - 1) == vt);
		assert(vtnumber(mods, XKB_KEY_XF86Switch_VT_1 + vt - 1) == vt);
		assert(vtnumber(0, XKB_KEY_F1 + vt - 1) == 0);
		assert(vtnumber(WLR_MODIFIER_ALT, XKB_KEY_F1 + vt - 1) == 0);
	}
	assert(vtnumber(mods, XKB_KEY_a) == 0);
	/* Nested sessions do not have a real VT to change. */
	chvt(&(Arg){.ui = 1});
	puts("PASS: VT keymap symbols, modifier checks and nested-session guard");
	return 0;
}
