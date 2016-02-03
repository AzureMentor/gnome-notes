/*
 * bjb-bijiben.c
 * Copyright (C) Pierre-Yves LUYTEN 2011 <py@luyten.fr>
 * 
 * bijiben is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * bijiben is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n.h>
#include <stdlib.h>
#include <libedataserver/libedataserver.h> /* ESourceRegistry */
#include <libecal/libecal.h>               /* ECalClient      */


#include <libbiji/libbiji.h>

#include "bjb-app-menu.h"
#include "bjb-bijiben.h"
#include "bjb-settings.h"
#include "bjb-main-view.h"
#include "bjb-note-view.h"
#include "bjb-window-base.h"

struct _BijibenPriv
{
  BijiManager *manager;
  BjbSettings *settings;

  /* Controls. to_open is for startup */

  gboolean     first_run;
  gboolean     is_loaded;
  gboolean     new_note;
  GQueue       *files_to_open; // paths
};


G_DEFINE_TYPE (Bijiben, bijiben, GTK_TYPE_APPLICATION);

static void
bijiben_new_window_internal (Bijiben *app,
                             GFile *file,
                             BijiItem *item);


static void
on_window_activated_cb   (BjbWindowBase *window,
                          gboolean win_is_available,
                          Bijiben *self)
{
  BijibenPriv *priv;
  gchar *path;
  BijiItem *item;
  GList *notfound, *l;

  item = NULL;
  priv = self->priv;
  priv->is_loaded = TRUE;
  notfound = NULL;

  while ((path = g_queue_pop_head (priv->files_to_open)))
  {
    item = biji_manager_get_item_at_path (priv->manager, path);

    if (item != NULL)
    {
      /* If that's a note, detach it */
      if (BIJI_IS_NOTE_OBJ (item))
      {
	bijiben_new_window_for_note (G_APPLICATION (self), BIJI_NOTE_OBJ (item));
      }

      /* Else, check */
      else
      {
        if (win_is_available)
           bjb_window_base_switch_to_item (window, item);

        else
           bijiben_new_window_internal (self, NULL, item);
      }
      g_free (path);
    }

    else
    {
      notfound = g_list_prepend (notfound, path);
    }
  }

  /* We just wait for next provider to be loaded.
   * TODO should also check if all providers are loaded
   * in order to trigger file reading. */
  for (l = notfound; l != NULL; l=l->next)
  {
    g_queue_push_head (priv->files_to_open, l->data);
  }


  /* All requested notes are loaded, but we have a new one to create...
   * This implementation is not really safe,
   * we might have loaded SOME provider(s)
   * but not the default one - more work is needed here */
  if (notfound == NULL &&
      priv->new_note == TRUE)
  {
    priv->new_note = FALSE;
    item = BIJI_ITEM (biji_manager_note_new (
                        priv->manager,
                        NULL,
                        bjb_settings_get_default_location (self->priv->settings)));
    bijiben_new_window_internal (self, NULL, item);
  }
}


static void
bijiben_new_window_internal (Bijiben     *self,
                             GFile       *file,
                             BijiItem    *item)
{
  BjbWindowBase *window;
  BijiNoteObj   *note;
  gchar         *path;
  GList         *windows;
  gboolean       not_first_window;

  note = NULL;

  windows = gtk_application_get_windows (GTK_APPLICATION (self));
  not_first_window = (gboolean) g_list_length (windows);

  if (file != NULL)
  {
    path = g_file_get_parse_name (file);
    note = BIJI_NOTE_OBJ (biji_manager_get_item_at_path (self->priv->manager, path));
    g_free (path);
  }

  else if (item != NULL && BIJI_IS_NOTE_OBJ (item))
  {
    note = BIJI_NOTE_OBJ (item);
  }


  window = BJB_WINDOW_BASE (bjb_window_base_new (note));
  g_signal_connect (window, "activated",
                    G_CALLBACK (on_window_activated_cb), self);

  gtk_widget_show (GTK_WIDGET (window));

  if (not_first_window)
    gtk_window_set_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
}

void
bijiben_new_window_for_note (GApplication *app,
                             BijiNoteObj *note)
{
  bijiben_new_window_internal (BIJIBEN_APPLICATION (app), NULL, BIJI_ITEM (note));
}

static void
bijiben_activate (GApplication *app)
{
  GList *windows = gtk_application_get_windows (GTK_APPLICATION (app));

  // ensure the last used window is raised
  gtk_window_present (g_list_nth_data (windows, 0));
}


/* If the app is already loaded, just open the file.
 * Else, keep it under the hood */

static void
bijiben_open (GApplication  *application,
              GFile        **files,
              gint           n_files,
              const gchar   *hint)
{
  Bijiben *self;
  gint i;

  self = BIJIBEN_APPLICATION (application);

  for (i = 0; i < n_files; i++)
  {
    if (self->priv->is_loaded == TRUE)
      bijiben_new_window_internal (self, files[i], NULL);

    else
      g_queue_push_head (self->priv->files_to_open, g_file_get_parse_name (files[i]));
  }
}

static void
bijiben_init (Bijiben *self)
{
  BijibenPriv *priv;


  priv = self->priv =
    G_TYPE_INSTANCE_GET_PRIVATE (self, BIJIBEN_TYPE_APPLICATION, BijibenPriv);

  priv->settings = bjb_settings_new ();
  priv->files_to_open = g_queue_new ();
  priv->new_note = FALSE;
  priv->is_loaded = FALSE;
}


void
bijiben_import_notes (Bijiben *self, gchar *uri)
{
  g_debug ("IMPORT to %s", bjb_settings_get_default_location (self->priv->settings));

  biji_manager_import_uri (self->priv->manager,
                           bjb_settings_get_default_location (self->priv->settings),
                           uri);
}

static void
theme_changed (GtkSettings *settings)
{
  static GtkCssProvider *provider = NULL;
  gchar *theme;
  GdkScreen *screen;

  g_object_get (settings, "gtk-theme-name", &theme, NULL);
  screen = gdk_screen_get_default ();

  if (g_str_equal (theme, "Adwaita"))
  {
    if (provider == NULL)
    {
        GFile *file;

        provider = gtk_css_provider_new ();
        file = g_file_new_for_uri ("resource:///org/gnome/bijiben/Adwaita.css");
        gtk_css_provider_load_from_file (provider, file, NULL);
        g_object_unref (file);
    }

    gtk_style_context_add_provider_for_screen (screen,
                                               GTK_STYLE_PROVIDER (provider),
                                               GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  }
  else if (provider != NULL)
  {
    gtk_style_context_remove_provider_for_screen (screen,
                                                  GTK_STYLE_PROVIDER (provider));
    g_clear_object (&provider);
  }

  g_free (theme);
}

static void
bjb_apply_style (void)
{
  GtkSettings *settings;

  /* Set up a handler to load our custom css for Adwaita.
   * See https://bugzilla.gnome.org/show_bug.cgi?id=732959
   * for a more automatic solution that is still under discussion.
   */
  settings = gtk_settings_get_default ();
  g_signal_connect (settings, "notify::gtk-theme-name", G_CALLBACK (theme_changed), NULL);
  theme_changed (settings);
}

static void
manager_ready_cb (GObject *source,
                  GAsyncResult *res,
                  gpointer user_data)
{
  Bijiben *self = user_data;
  GError *error = NULL;

  self->priv->manager = biji_manager_new_finish (res, &error);
  g_application_release (G_APPLICATION (self));

  if (error != NULL)
    {
      g_warning ("Cannot initialize BijiManager: %s\n", error->message);
      g_clear_error (&error);
      return;
    }

  bijiben_new_window_internal (self, NULL, NULL);
}

static void
bijiben_startup (GApplication *application)
{
  Bijiben        *self;
  gchar          *storage_path, *default_color;
  GFile          *storage;
  GError         *error;
  gchar          *path, *uri;
  GdkRGBA         color = {0,0,0,0};


  G_APPLICATION_CLASS (bijiben_parent_class)->startup (application);
  self = BIJIBEN_APPLICATION (application);
  error = NULL;

  bjb_apply_style ();

  bjb_app_menu_set(application);

  storage_path = g_build_filename (g_get_user_data_dir (), "bijiben", NULL);
  storage = g_file_new_for_path (storage_path);

  // Create the bijiben dir to ensure.
  self->priv->first_run = TRUE;
  g_file_make_directory (storage, NULL, &error);

  // If fails it's not the first run
  if (error && error->code == G_IO_ERROR_EXISTS)
  {
    self->priv->first_run = FALSE;
    g_error_free (error);
  }

  else if (error)
  {
    g_warning ("%s", error->message);
    g_error_free (error);
  }

  else
  {
    self->priv->first_run = TRUE;
  }


  g_object_get (self->priv->settings, "color", &default_color, NULL);
  gdk_rgba_parse (&color, default_color);

  g_application_hold (application);
  biji_manager_new_async (storage, &color, manager_ready_cb, self);

  /* Automatic imports on startup */
  if (self->priv->first_run == TRUE)
  {
    path = g_build_filename (g_get_user_data_dir (), "tomboy", NULL);
    uri = g_filename_to_uri (path, NULL, NULL);
    bijiben_import_notes (self, uri);
    g_free (path);
    g_free (uri);

    path = g_build_filename (g_get_user_data_dir (), "gnote", NULL);
    uri = g_filename_to_uri (path, NULL, NULL);
    bijiben_import_notes (self, uri);
    g_free (path);
    g_free (uri);
  }

  g_free (default_color);
  g_free (storage_path);
  g_object_unref (storage);
}

static gboolean
bijiben_application_local_command_line (GApplication *application,
                                        gchar ***arguments,
                                        gint *exit_status)
{
  gboolean version = FALSE;
  gchar **remaining = NULL;
  GOptionContext *context;
  GError *error = NULL;
  gint argc = 0;
  gchar **argv = NULL;
  *exit_status = EXIT_SUCCESS;
  GFile **files;
  gint idx, len;

  const GOptionEntry options[] = {
    { "version", 0, 0, G_OPTION_ARG_NONE, &version,
      N_("Show the application's version"), NULL},
    { "new-note", 0, 0, G_OPTION_ARG_NONE, &BIJIBEN_APPLICATION(application)->priv->new_note,
      N_("Create a new note"), NULL},
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &remaining,
      NULL,  N_("[FILE...]") },
    { NULL }
  };


  context = g_option_context_new (NULL);
  g_option_context_set_summary (context,
                                _("Take notes and export them everywhere."));
  g_option_context_add_main_entries (context, options, NULL);
  g_option_context_add_group (context, gtk_get_option_group (FALSE));

  argv = *arguments;
  argc = g_strv_length (argv);

  if (!g_option_context_parse (context, &argc, &argv, &error))
  {
    /* Translators: this is a fatal error quit message
     * printed on the command line */
    g_printerr ("%s: %s\n", _("Could not parse arguments"), error->message);
    g_error_free (error);

    *exit_status = EXIT_FAILURE;
    goto out;
  }

  if (version)
  {
    g_print ("%s %s\n", _("GNOME Notes"), VERSION);
    goto out;
  }


  /* bijiben_startup */
  g_application_register (application, NULL, &error);

  if (error != NULL)
  {
    /* Translators: this is a fatal error quit message
     * printed on the command line */
    g_printerr ("%s: %s\n",
                _("Could not register the application"),
                error->message);
    g_error_free (error);

    *exit_status = EXIT_FAILURE;
    goto out;
  }

  if (BIJIBEN_APPLICATION (application)->priv->new_note)
  {
    g_application_open (application, NULL, 0, "");
    goto out;
  }

  len = 0;
  files = NULL;

  /* Convert args to GFiles */
  if (remaining != NULL)
  {
    GFile *file;
    GPtrArray *file_array;

    file_array = g_ptr_array_new ();

    for (idx = 0; remaining[idx] != NULL; idx++)
    {
      file = g_file_new_for_commandline_arg (remaining[idx]);
      if (file != NULL)
        g_ptr_array_add (file_array, file);
    }

    len = file_array->len;
    files = (GFile **) g_ptr_array_free (file_array, FALSE);
    g_strfreev (remaining);
  }

  if (files == NULL)
  {
    files = g_malloc0 (sizeof (GFile *));
    len = 0;

    files[0] = NULL;
  }

  if (len == 0)
    goto out;

  /* Invoke "Open" to create new windows */
  g_application_open (application, files, len, "");

  for (idx = 0; idx < len; idx++)
  {
    g_object_unref (files[idx]);
  }

  g_free (files);

 out:
  g_option_context_free (context);
  return TRUE;
}

static void
bijiben_finalize (GObject *object)
{
  Bijiben *self = BIJIBEN_APPLICATION (object);

  g_clear_object (&self->priv->manager);
  g_clear_object (&self->priv->settings);
  g_queue_free (self->priv->files_to_open);

  G_OBJECT_CLASS (bijiben_parent_class)->finalize (object);
}

static void
bijiben_class_init (BijibenClass *klass)
{
  GApplicationClass *aclass = G_APPLICATION_CLASS (klass);
  GObjectClass *oclass = G_OBJECT_CLASS (klass);

  aclass->activate = bijiben_activate;
  aclass->open = bijiben_open;
  aclass->startup = bijiben_startup;
  aclass->local_command_line = bijiben_application_local_command_line;

  oclass->finalize = bijiben_finalize;

  g_type_class_add_private (klass, sizeof (BijibenPriv));
}

Bijiben *
bijiben_new (void)
{
  return g_object_new (BIJIBEN_TYPE_APPLICATION,
                       "application-id", "org.gnome.bijiben",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}

BijiManager *
bijiben_get_manager(Bijiben *self)
{
  return self->priv->manager ;
}

const gchar *
bijiben_get_bijiben_dir (void)
{
  return DATADIR;
}

BjbSettings * bjb_app_get_settings(gpointer application)
{
  return BIJIBEN_APPLICATION(application)->priv->settings ;
}
