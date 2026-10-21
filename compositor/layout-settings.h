/* Per-output layout preference, restored before the first client is mapped. */
static char *layout_settings_path(void)
{
	return g_build_filename(g_get_user_config_dir(), "xpwm", "layouts.ini", NULL);
}

static char *layout_settings_group(Monitor *m)
{
	char *hash = g_compute_checksum_for_string(G_CHECKSUM_SHA256, m->wlr_output->name, -1);
	char *group = g_strconcat("Output-", hash, NULL);
	g_free(hash); return group;
}

static void restore_layout_mode(Monitor *m)
{
	char *path = layout_settings_path(), *group = layout_settings_group(m), *mode = NULL;
	GKeyFile *ini = g_key_file_new();
	if (g_key_file_load_from_file(ini, path, G_KEY_FILE_NONE, NULL)) {
		mode = g_key_file_get_string(ini, group, "Mode", NULL);
		if (!mode) mode = g_key_file_get_string(ini, "Desktop", "Mode", NULL);
	}
	for (size_t i = 0; mode && i < LENGTH(layouts); i++) {
		if (strcmp(mode, layouts[i].symbol)) continue;
		m->sellt = 0; m->lt[0] = &layouts[i];
		for (size_t other = 0; other < LENGTH(layouts); other++) {
			if (!!layouts[other].arrange == !!layouts[i].arrange) continue;
			m->lt[1] = &layouts[other]; break;
		}
		break;
	}
	strncpy(m->ltsymbol, m->lt[m->sellt]->symbol, LENGTH(m->ltsymbol));
	g_key_file_unref(ini); g_free(path); g_free(group); g_free(mode);
}

static void save_layout_mode(Monitor *m)
{
	char *path = layout_settings_path(), *dir = g_path_get_dirname(path), *contents = NULL;
	GKeyFile *ini = g_key_file_new();
	GError *error = NULL;
	Monitor *output;
	if (!g_key_file_load_from_file(ini, path, G_KEY_FILE_KEEP_COMMENTS, &error)
			&& !g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOENT)) goto done;
	g_clear_error(&error);
	/* Preserve each connected output and use the last selection when a
	 * connector name changes between sessions (for example nested vs TTY). */
	g_key_file_set_string(ini, "Desktop", "Mode", m->lt[m->sellt]->symbol);
	wl_list_for_each(output, &mons, link) {
		char *output_group = layout_settings_group(output);
		g_key_file_set_string(ini, output_group, "Output", output->wlr_output->name);
		g_key_file_set_string(ini, output_group, "Mode", output->lt[output->sellt]->symbol);
		g_free(output_group);
	}
	if (g_mkdir_with_parents(dir, 0700) < 0) {
		wlr_log(WLR_ERROR, "Unable to create layout settings directory: %s", dir);
		goto done;
	}
	contents = g_key_file_to_data(ini, NULL, NULL);
	g_file_set_contents(path, contents, -1, &error);
done:
	if (error) wlr_log(WLR_ERROR, "Unable to save layout mode: %s", error->message);
	g_clear_error(&error); g_key_file_unref(ini); g_free(path); g_free(dir); g_free(contents);
}
