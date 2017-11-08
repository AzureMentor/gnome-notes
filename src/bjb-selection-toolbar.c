/* bjb-selection-toolbar.c
 * Copyright © 2012, 2013 Red Hat, Inc.
 * Copyright © 2013 Pierre-Yves LUYTEN <py@luyten.fr>
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <libgd/gd.h>

#include "bjb-application.h"
#include "bjb-color-button.h"
#include "bjb-main-view.h"
#include "bjb-organize-dialog.h"
#include "bjb-selection-toolbar.h"
#include "bjb-share.h"
#include "bjb-window-base.h"

enum
{
  PROP_0,
  PROP_BJB_SELECTION,
  PROP_BJB_MAIN_VIEW,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };



struct _BjbSelectionToolbar
{
  GtkRevealer         parent_instance;

  GtkActionBar       *bar;
  BjbMainView        *view ;
  GtkWidget          *widget ;
  GdMainView         *selection ;

  /* Header bar members. Classic view */
  GtkWidget          *toolbar_trash;
  GtkWidget          *toolbar_color;
  GtkWidget          *toolbar_tag;
  GtkWidget          *toolbar_share;
  GtkWidget          *toolbar_detach;

  /* Header bar, archive view */
  GtkWidget          *toolbar_restore;
  GtkWidget          *toolbar_delete;
};

G_DEFINE_TYPE (BjbSelectionToolbar, bjb_selection_toolbar, GTK_TYPE_REVEALER)


/*
 * Color dialog is transient and could damage the display of self
 * We do not want a modal window since the app may have several
 * The fix is to hide self untill dialog has run
 *
 */
static void
hide_self (GtkWidget *self)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);
}


static void
action_color_selected_items (GtkWidget *w, BjbSelectionToolbar *self)
{
  GList *l, *selection;
  GdkRGBA color = {0,0,0,0};

  gtk_widget_set_visible (GTK_WIDGET (self), TRUE);
  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (w), &color);
  selection = bjb_main_view_get_selected_items (self->view);

  for (l=selection; l !=NULL; l=l->next)
  {
    if (BIJI_IS_NOTE_OBJ (l->data))
      biji_note_obj_set_rgba (l->data, &color);
  }

  bjb_main_view_set_selection_mode (self->view, FALSE);
  g_list_free (selection);
}


static void
action_tag_selected_items (GtkWidget *w, BjbSelectionToolbar *self)
{
  GList *selection;

  selection = bjb_main_view_get_selected_items (self->view);
  bjb_organize_dialog_new
    (GTK_WINDOW (bjb_main_view_get_window (self->view)), selection);

  bjb_main_view_set_selection_mode (self->view, FALSE);
  g_list_free (selection);
}


static void
action_trash_selected_items (GtkWidget *w, BjbSelectionToolbar *self)
{
  GList *l, *selection;

  selection = bjb_main_view_get_selected_items (self->view);
  for (l=selection; l !=NULL; l=l->next)
    biji_item_trash (BIJI_ITEM (l->data));

  bjb_main_view_set_selection_mode (self->view, FALSE);
  g_list_free (selection);
}


static void
action_pop_up_note_callback (GtkWidget *w, BjbSelectionToolbar *self)
{
  GList *l, *selection;

  selection = bjb_main_view_get_selected_items (self->view);

  for (l=selection; l !=NULL; l=l->next)
  {
    bijiben_new_window_for_note (g_application_get_default (),
                                 BIJI_NOTE_OBJ (l->data));
  }

  bjb_main_view_set_selection_mode (self->view, FALSE);
  g_list_free (selection);
}


static void
action_share_item_callback (GtkWidget *w, BjbSelectionToolbar *self)
{
  GList *l, *selection;

  selection = bjb_main_view_get_selected_items (self->view);

  for (l=selection; l!= NULL; l=l->next)
  {
     on_email_note_callback (w, l->data);
  }

  bjb_main_view_set_selection_mode (self->view, FALSE);
  g_list_free (selection);
}


static void
on_restore_clicked_callback      (BjbSelectionToolbar *self)
{
  GList *selection, *l;

  selection = bjb_main_view_get_selected_items (self->view);

  for (l=selection; l!=NULL; l=l->next)
    biji_item_restore (BIJI_ITEM (l->data));

  bjb_main_view_set_selection_mode (self->view, FALSE);
  g_list_free (selection);
}



static void
on_delete_clicked_callback        (BjbSelectionToolbar *self)
{
  GList *selection, *l;

  selection = bjb_main_view_get_selected_items (self->view);
  for (l=selection; l!=NULL; l=l->next)
    biji_item_delete (BIJI_ITEM (l->data));

  bjb_main_view_set_selection_mode (self->view, FALSE);
  g_list_free (selection);
}



static void
set_sensitivity (BjbSelectionToolbar *self)
{
  GList *l, *selection;
  GdkRGBA color;
  gboolean can_tag, can_color, can_share, are_notes;

  g_return_if_fail (BJB_IS_SELECTION_TOOLBAR (self));

  selection = bjb_main_view_get_selected_items (self->view);

  /* Default */
  can_color = TRUE;
  can_tag = TRUE;
  can_share = TRUE;
  are_notes = TRUE;


  /* Adapt */
  for (l=selection; l !=NULL; l=l->next)
  {
    if (can_tag == TRUE) /* tag is default. check if still applies */
    {
      if (!biji_item_is_collectable (l->data))
        can_tag = FALSE;
    }

   if (are_notes == FALSE || (BIJI_IS_NOTE_OBJ (l->data)) == FALSE)
     are_notes = FALSE;


    if (can_color == TRUE) /* color is default. check */
    {
      if (!BIJI_IS_NOTE_OBJ (l->data)
          || !biji_item_has_color (l->data)
          || !biji_note_obj_get_rgba (BIJI_NOTE_OBJ (l->data), &color))
        can_color = FALSE;

     else
       gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (self->toolbar_color), &color);
    }

    if (can_share == TRUE) /* share is default. check. */
    {
      if (!BIJI_IS_NOTE_OBJ (l->data))
        can_share = FALSE;
    }
  }


  gtk_widget_set_sensitive (self->toolbar_color, can_color);
  gtk_widget_set_sensitive (self->toolbar_tag, can_tag);
  gtk_widget_set_sensitive (self->toolbar_share, can_share);
  gtk_widget_set_sensitive (self->toolbar_detach, are_notes);

  g_list_free (selection);
}



static void
set_trash_bar_sensitivity (BjbSelectionToolbar *self)
{
  gtk_widget_set_sensitive (self->toolbar_restore, TRUE);
  gtk_widget_set_sensitive (self->toolbar_delete, TRUE);
}


static void
bjb_selection_toolbar_set_item_visibility (BjbSelectionToolbar *self)
{
  BijiItemsGroup group;

  g_return_if_fail (BJB_IS_SELECTION_TOOLBAR (self));

  group = bjb_controller_get_group (
            bjb_window_base_get_controller (
              BJB_WINDOW_BASE (
                bjb_main_view_get_window (self->view))));

  if (group == BIJI_LIVING_ITEMS)
  {
    gtk_widget_hide (self->toolbar_restore);
    gtk_widget_hide (self->toolbar_delete);

    gtk_widget_show (self->toolbar_trash);
    gtk_widget_show (self->toolbar_color);
    gtk_widget_show (self->toolbar_tag);
    gtk_widget_show (self->toolbar_share);
    gtk_widget_show (self->toolbar_detach);

    set_sensitivity (self);
  }

  else if (group == BIJI_ARCHIVED_ITEMS)
  {
    gtk_widget_hide (self->toolbar_trash);
    gtk_widget_hide (self->toolbar_color);
    gtk_widget_hide (self->toolbar_tag);
    gtk_widget_hide (self->toolbar_share);
    gtk_widget_hide (self->toolbar_detach);

    gtk_widget_show (self->toolbar_restore);
    gtk_widget_show (self->toolbar_delete);

    set_trash_bar_sensitivity (self);
  }
}





static void
bjb_selection_toolbar_fade_in (BjbSelectionToolbar *self)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), TRUE);
  //bjb_selection_toolbar_set_item_visibility
}


static void
bjb_selection_toolbar_fade_out (BjbSelectionToolbar *self)
{
  gtk_revealer_set_reveal_child (GTK_REVEALER (self), FALSE);
}


static void
bjb_selection_toolbar_selection_changed (GdMainView *view, gpointer user_data)
{
  BjbSelectionToolbar *self;
  GList *selection;

  self = BJB_SELECTION_TOOLBAR (user_data);
  selection = gd_main_view_get_selection(view);

  if (g_list_length (selection) > 0)
  {
    bjb_selection_toolbar_set_item_visibility (self);
    bjb_selection_toolbar_fade_in (self);
  }

  else
    bjb_selection_toolbar_fade_out (self);

  g_list_free (selection);
}

static void
bjb_selection_toolbar_dispose (GObject *object)
{
  G_OBJECT_CLASS (bjb_selection_toolbar_parent_class)->dispose (object);
}


static void
bjb_selection_toolbar_init (BjbSelectionToolbar *self)
{
  GtkWidget                  *widget, *share;
  GtkStyleContext            *context;
  GtkSizeGroup               *size;

  widget = GTK_WIDGET (self);

  gtk_revealer_set_transition_type (
      GTK_REVEALER (self), GTK_REVEALER_TRANSITION_TYPE_SLIDE_UP);

  self->bar = GTK_ACTION_BAR (gtk_action_bar_new ());
  context = gtk_widget_get_style_context (GTK_WIDGET (self->bar));
  gtk_style_context_add_class (context, "background");
  gtk_container_add (GTK_CONTAINER (self), GTK_WIDGET (self->bar));


  /* Notes tags */
  self->toolbar_tag = gtk_button_new_with_label (_("Notebooks"));
  gtk_action_bar_pack_start (self->bar, self->toolbar_tag);
  gtk_widget_show (self->toolbar_tag);

  /* Restore (do not show) */
  self->toolbar_restore = gtk_button_new_with_label (_("Restore"));
  gtk_action_bar_pack_start (self->bar, self->toolbar_restore);

  /* Notes color */
  self->toolbar_color = bjb_color_button_new ();
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->toolbar_color),
                               _("Note color"));
  gtk_action_bar_pack_start (self->bar, self->toolbar_color);
  gtk_widget_show (self->toolbar_color);


  /* Share */
  self->toolbar_share = gtk_button_new ();
  share = gtk_image_new_from_icon_name ("mail-unread-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_image (GTK_BUTTON (self->toolbar_share), share);
  gtk_style_context_add_class (gtk_widget_get_style_context (self->toolbar_share),
                               "image-button");
  gtk_widget_set_tooltip_text (self->toolbar_share, _("Share note"));
  gtk_action_bar_pack_start (self->bar, self->toolbar_share);
  gtk_widget_show (self->toolbar_color);


  /* Detach */
  self->toolbar_detach = gtk_button_new_with_label (_("Open in another window"));
  gtk_action_bar_pack_start (self->bar, self->toolbar_detach);
  gtk_widget_show (self->toolbar_detach);


  /* Trash notes */
  self->toolbar_trash = gtk_button_new_with_label (_("Move to Trash"));
  context = gtk_widget_get_style_context (self->toolbar_trash);
  gtk_style_context_add_class (context, "destructive-action");
  gtk_action_bar_pack_end (self->bar, self->toolbar_trash);
  gtk_widget_show (self->toolbar_trash);


  /* Permanently delete (do not show )*/
  self->toolbar_delete = gtk_button_new_with_label (_("Permanently Delete"));
  context = gtk_widget_get_style_context (self->toolbar_delete);
  gtk_style_context_add_class (context, "destructive-action");
  gtk_action_bar_pack_end (self->bar, self->toolbar_delete);


  /* Align buttons */
  size = gtk_size_group_new (GTK_SIZE_GROUP_VERTICAL);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), self->toolbar_tag);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), self->toolbar_color);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), self->toolbar_share);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), self->toolbar_detach);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), self->toolbar_trash);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), self->toolbar_restore);
  gtk_size_group_add_widget (GTK_SIZE_GROUP (size), self->toolbar_delete);
  g_object_unref (size);


  gtk_widget_show (GTK_WIDGET (self->bar));
  gtk_widget_show (widget);
  bjb_selection_toolbar_fade_out (self);
}

static void
bjb_selection_toolbar_get_property (GObject  *object,
                                    guint     property_id,
                                    GValue   *value,
                                    GParamSpec *pspec)
{
  BjbSelectionToolbar *self = BJB_SELECTION_TOOLBAR (object);

  switch (property_id)
  {
    case PROP_BJB_SELECTION:
      g_value_set_object(value, self->selection);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
bjb_selection_toolbar_set_property (GObject  *object,
                                    guint     property_id,
                                    const GValue *value,
                                    GParamSpec *pspec)
{
  BjbSelectionToolbar *self = BJB_SELECTION_TOOLBAR (object);

  switch (property_id)
  {
    case PROP_BJB_SELECTION:
      self->selection = g_value_get_object (value);
      break;
    case PROP_BJB_MAIN_VIEW:
      self->view = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
bjb_selection_toolbar_constructed(GObject *obj)
{
  BjbSelectionToolbar *self = BJB_SELECTION_TOOLBAR(obj);

  G_OBJECT_CLASS (bjb_selection_toolbar_parent_class)->constructed (obj);


  g_signal_connect (self->selection,
                    "view-selection-changed", 
                    G_CALLBACK(bjb_selection_toolbar_selection_changed),
                    self);

  /* BIJI LIVING ITEMS */
  g_signal_connect (self->toolbar_tag, "clicked",
                    G_CALLBACK (action_tag_selected_items), self);

  g_signal_connect_swapped (self->toolbar_color, "clicked",
                    G_CALLBACK (hide_self), self);

  g_signal_connect (self->toolbar_color, "color-set",
                    G_CALLBACK (action_color_selected_items), self);

  g_signal_connect (self->toolbar_share, "clicked",
                    G_CALLBACK (action_share_item_callback), self);

  g_signal_connect (self->toolbar_detach, "clicked",
                    G_CALLBACK (action_pop_up_note_callback), self);

  g_signal_connect (self->toolbar_trash, "clicked",
                    G_CALLBACK (action_trash_selected_items), self);


  /* BIJI ARCHIVED ITEMS */
  g_signal_connect_swapped (self->toolbar_restore, "clicked",
                    G_CALLBACK (on_restore_clicked_callback), self);

  g_signal_connect_swapped (self->toolbar_delete, "clicked",
                    G_CALLBACK (on_delete_clicked_callback), self);
}

static void
bjb_selection_toolbar_class_init (BjbSelectionToolbarClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = bjb_selection_toolbar_dispose;
  object_class->get_property = bjb_selection_toolbar_get_property ;
  object_class->set_property = bjb_selection_toolbar_set_property ;
  object_class->constructed = bjb_selection_toolbar_constructed ;

  properties[PROP_BJB_SELECTION] = g_param_spec_object ("selection",
                                                        "Selection",
                                                        "SelectionController",
                                                        GD_TYPE_MAIN_VIEW,
                                                        G_PARAM_READWRITE |
                                                        G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class,PROP_BJB_SELECTION,properties[PROP_BJB_SELECTION]);

  properties[PROP_BJB_MAIN_VIEW] = g_param_spec_object ("bjbmainview",
                                                        "Bjbmainview",
                                                        "BjbMainView",
                                                        BJB_TYPE_MAIN_VIEW,
                                                        G_PARAM_READWRITE  |
                                                        G_PARAM_CONSTRUCT |
                                                        G_PARAM_STATIC_STRINGS);

  g_object_class_install_property (object_class,PROP_BJB_MAIN_VIEW,properties[PROP_BJB_MAIN_VIEW]);
}


BjbSelectionToolbar *
bjb_selection_toolbar_new (GdMainView   *selection,
                           BjbMainView  *bjb_main_view)
{
  return g_object_new (BJB_TYPE_SELECTION_TOOLBAR,
                       "selection", selection,
                       "bjbmainview",bjb_main_view,
                       NULL);
}
