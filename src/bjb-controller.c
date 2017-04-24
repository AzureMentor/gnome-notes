/*
 * bjb-controller.c
 * Copyright (C) Pierre-Yves Luyten 2012, 2013 <py@luyten.fr>
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

/*
 * bjb-controler is window-wide.
 * mainly useful for BjbMainView,
 * it controls the window behaviour.
 */

#include <libgd/gd.h>

#include "bjb-bijiben.h"
#include "bjb-controller.h"
#include "bjb-main-view.h"
#include "bjb-window-base.h"


/*
 * The start-up number of items to show,
 * and its incrementation
 */

#define BJB_ITEMS_SLICE 48


/* Gobject */

struct _BjbControllerPrivate
{
  /* needle, notebook and group define what the controller shows */
  BijiManager    *manager;
  gchar          *needle;
  BijiNotebook   *notebook;
  BijiItemsGroup  group;
  GtkTreeModel   *model;

  BjbWindowBase  *window;

  /*  Private  */
  GList          *items_to_show;
  gint            n_items_to_show;
  gboolean        remaining_items;
  GMutex          mutex;

  gboolean        connected;
  gulong          manager_change;
};


enum {
  PROP_0,
  PROP_BOOK,
  PROP_WINDOW,
  PROP_NEEDLE,
  PROP_MODEL,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL, };

/* Currently used by toolbars */
enum {
  SEARCH_CHANGED,
  DISPLAY_NOTES_CHANGED, // either search or manager change
  BJB_CONTROLLER_SIGNALS
};

static guint bjb_controller_signals [BJB_CONTROLLER_SIGNALS] = { 0 };

#define BJB_CONTROLLER_GET_PRIVATE(o)  (G_TYPE_INSTANCE_GET_PRIVATE ((o), BJB_TYPE_CONTROLLER, BjbControllerPrivate))

G_DEFINE_TYPE (BjbController, bjb_controller, G_TYPE_OBJECT);

/* GObject */

static void
bjb_controller_init (BjbController *self)
{
  BjbControllerPrivate *priv  ;
  GtkListStore     *store ;

  priv = self->priv = G_TYPE_INSTANCE_GET_PRIVATE
    (self, BJB_TYPE_CONTROLLER, BjbControllerPrivate);

  /* Create the columns */
  store = gtk_list_store_new (GD_MAIN_COLUMN_LAST,
                              G_TYPE_STRING,      // urn
                              G_TYPE_STRING,      // uri
                              G_TYPE_STRING,      // name
                              G_TYPE_STRING,      // author
                              CAIRO_GOBJECT_TYPE_SURFACE, // icon then note
                              G_TYPE_INT64,       // mtime
                              G_TYPE_BOOLEAN,     // state
                              G_TYPE_UINT);       // pulse

  priv->model = GTK_TREE_MODEL(store) ;
  priv->items_to_show = NULL;
  priv->n_items_to_show = BJB_ITEMS_SLICE;
  priv->needle = NULL;
  priv->notebook = NULL;
  priv->group = BIJI_LIVING_ITEMS;
  priv->connected = FALSE;

}

static void
free_items_store (BjbController *self)
{
  gtk_list_store_clear (GTK_LIST_STORE (self->priv->model));
}

static void
bjb_controller_finalize (GObject *object)
{
  BjbController *self = BJB_CONTROLLER(object);
  BjbControllerPrivate *priv = self->priv ;


  g_object_unref (priv->model);

  g_free (priv->needle);
  g_list_free (priv->items_to_show);

  if (priv->notebook)
    g_free (priv->notebook);

  G_OBJECT_CLASS (bjb_controller_parent_class)->finalize (object);
}

static void
bjb_controller_get_property (GObject  *object,
               guint     property_id,
               GValue   *value,
               GParamSpec *pspec)
{
  BjbController *self = BJB_CONTROLLER (object);

  switch (property_id)
  {
  case PROP_BOOK:
    g_value_set_object (value, self->priv->manager);
    break;
  case PROP_WINDOW:
    g_value_set_object (value, self->priv->window);
    break;
  case PROP_NEEDLE:
    g_value_set_string(value, self->priv->needle);
    break;
  case PROP_MODEL:
    g_value_set_object(value, self->priv->model);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

static void
bjb_controller_set_property (GObject  *object,
               guint     property_id,
               const GValue *value,
               GParamSpec *pspec)
{
  BjbController *self = BJB_CONTROLLER (object);

  switch (property_id)
  {
  case PROP_BOOK:
    bjb_controller_set_manager(self,g_value_get_object(value));
    break;
  case PROP_WINDOW:
    self->priv->window = g_value_get_object (value);
    break;
  case PROP_NEEDLE:
    self->priv->needle = g_strdup (g_value_get_string (value));
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    break;
  }
}

/* get iter at this item
 * or get iter at first note if item == NULL*/
static gboolean
bjb_controller_get_iter (BjbController *self,
                         BijiItem *item,
                         GtkTreeIter **iter)
{
  BjbControllerPrivate *priv = self->priv;
  gboolean retval = FALSE;
  gboolean try;
  const gchar *needle = NULL;

  if (item && BIJI_IS_ITEM (item))
      needle = biji_item_get_uuid (item);

  try = gtk_tree_model_get_iter_first (priv->model, *iter);

  if (!try)
    *iter = NULL;

  while (try)
  {
    gchar *item_path;
    gtk_tree_model_get (priv->model, *iter, GD_MAIN_COLUMN_URI, &item_path,-1);

    /* If we look for the item, check by uid */
    if (needle && g_strcmp0 (item_path, needle) == 0)
      retval = TRUE;

    /* Else we check for the first note */
    else if (!needle && BIJI_IS_NOTE_OBJ (
              biji_manager_get_item_at_path (self->priv->manager, item_path)))
      retval = TRUE;

    g_free (item_path);

    if (retval)
      break;

    else
      try = gtk_tree_model_iter_next (priv->model, *iter);
  }

  return retval;
}

/* Either append, prepend, or, if iter provider,
 * prepend just before this iter */
static void
bjb_controller_add_item (BjbController *self,
                         BijiItem      *item,
                         gboolean       prepend,
                         GtkTreeIter   *sibling)
{
  GtkTreeIter    iter;
  GtkListStore  *store;
  cairo_surface_t *surface = NULL;
  const gchar   *uuid;
  BjbWindowBase  *win;
  gint scale;

  g_return_if_fail (BIJI_IS_ITEM (item));
  store = GTK_LIST_STORE (self->priv->model);

  /* Only append notes which are not templates. Currently useless */
  if (BIJI_IS_NOTE_OBJ (item)
      && biji_note_obj_is_template (BIJI_NOTE_OBJ (item)))
    return;


  if (sibling)
    gtk_list_store_insert_before (store, &iter, sibling);

  else if (prepend)
    gtk_list_store_insert (store, &iter, 0);

  else
    gtk_list_store_append (store, &iter);


  /* First , if there is a gd main view , and if gd main view
   * is a list, then load the smaller emblem */
  win = self->priv->window;
  scale = gtk_widget_get_scale_factor (GTK_WIDGET (win));

  if (bjb_window_base_get_main_view (win)
      && bjb_main_view_get_view_type
                (bjb_window_base_get_main_view (win)) == GD_MAIN_VIEW_LIST)
    surface = biji_item_get_emblem (item, scale);

  /* Else, load the icon */
  if (!surface)
    surface = biji_item_get_icon (item, scale);

  /* Appart from pixbuf, both icon & list view types
   * currently use the same model */
  uuid = biji_item_get_uuid (item);

  gtk_list_store_set (store, &iter,
       GD_MAIN_COLUMN_ID, uuid,
       GD_MAIN_COLUMN_URI, uuid,
       GD_MAIN_COLUMN_PRIMARY_TEXT, biji_item_get_title (item),
       GD_MAIN_COLUMN_SECONDARY_TEXT, NULL,
       GD_MAIN_COLUMN_ICON, surface,
       GD_MAIN_COLUMN_MTIME, biji_item_get_mtime (item),
       -1);

}

/* If the user searches for notes, is the note searched? */
static void
bjb_controller_add_item_if_needed (BjbController *self,
                                   BijiItem      *item,
                                   gboolean       prepend,
                                   GtkTreeIter   *sibling)
{
  gboolean need_to_add_item = FALSE;
  const gchar *content;
  const gchar *title;
  BjbControllerPrivate *priv = self->priv;

  /* No note... */
  if (!item || !BIJI_IS_ITEM (item))
    return;

  /* No search - we add the note */
  if (!priv->needle || g_strcmp0 (priv->needle, "")==0)
  {
    if (!priv->notebook)
      need_to_add_item = TRUE;

    /* To do : we might have a notebook
     * but have to udpate the view */
  }

  /* a search.. we test...*/
  else
  {
    title = biji_item_get_title (item);

    /* matching title... */
    if (g_strrstr (title, priv->needle) != NULL)
      need_to_add_item = TRUE;

    /* matching content */
    else if (BIJI_IS_NOTE_OBJ (item))
    {
      content = biji_note_obj_get_raw_text (BIJI_NOTE_OBJ (item));
      if (g_strrstr (content, priv->needle) != NULL)
        need_to_add_item = TRUE;
    }
  }

  if (need_to_add_item)
    bjb_controller_add_item (self, item, prepend, sibling);
}


static gint
most_recent_item_first (gconstpointer a, gconstpointer b)
{
  BijiItem *one = BIJI_ITEM (a);
  BijiItem *other = BIJI_ITEM (b);
  glong result = 0;

  /* Always sort notebooks before notes */
  if (BIJI_IS_NOTEBOOK (a))
  {
    if (BIJI_IS_NOTE_OBJ (b))
      result = -1;
  }

  else if (BIJI_IS_NOTEBOOK (b))
  {
    result = 1;
  }

  /* If comparing two notes or
   * two notebooks, use the most recent cookbook */
  else
  {
    result =   biji_item_get_mtime (other)
             - biji_item_get_mtime (one);
  }

  return result;
}



static void
sort_items (GList **to_show)
{
  *to_show = g_list_sort (*to_show, most_recent_item_first);
}


void
bjb_controller_update_view (BjbController *self)
{
  GList *items, *l;
  BjbWindowViewType type;


  /* Do not update if nothing to show */
  type = bjb_window_base_get_view_type (self->priv->window);
  if (! (type == BJB_WINDOW_BASE_MAIN_VIEW
         || type == BJB_WINDOW_BASE_ARCHIVE_VIEW))
    return;

  items = self->priv->items_to_show;
  free_items_store (self);

  sort_items (&items);

  for (l = items; l != NULL; l = l->next)
  {
    bjb_controller_add_item (self, l->data, FALSE, NULL);
  }
}


static void
notify_displayed_items_changed (BjbController *self)
{
  g_signal_emit (G_OBJECT (self),
                 bjb_controller_signals[DISPLAY_NOTES_CHANGED],
                 0,
                 (self->priv->items_to_show != NULL),
                 self->priv->remaining_items);
}

static void
update (BjbController *self)
{
  BjbWindowViewType type;

  /* If the user already edits a note, he does not want the view
   * to go back */
  if (bjb_window_base_get_view_type (self->priv->window) != BJB_WINDOW_BASE_NOTE_VIEW)
  {
    if (self->priv->group == BIJI_LIVING_ITEMS)
      type = BJB_WINDOW_BASE_MAIN_VIEW;
    else
      type = BJB_WINDOW_BASE_ARCHIVE_VIEW;

    bjb_window_base_switch_to (self->priv->window, type);
  }


  bjb_controller_update_view (self);
  notify_displayed_items_changed (self);
}



static void
_add_if_group_match (BjbController  *self,
                    GList         **to_show,
                    gpointer       *item,
                    gint           *count)
{
  gboolean trashed, match;
  BijiNoteObj *note;

  if (BIJI_IS_NOTE_OBJ (*item))
  {
    match = FALSE;
    note = BIJI_NOTE_OBJ (*item);
    trashed = biji_note_obj_is_trashed (note);

    if ((trashed==FALSE && self->priv->group == BIJI_LIVING_ITEMS) ||
        (trashed==TRUE  && self->priv->group == BIJI_ARCHIVED_ITEMS))
      match = TRUE;
  }

  else
  {
    match = TRUE;
  }

  if (match)
  {
    *to_show = g_list_prepend (*to_show, *item);
    *count = *count + 1;
  }
}


static void
update_controller_callback (GList *result,
                            gpointer user_data)
{
  BjbController *self;
  BjbControllerPrivate *priv;
  GList *l;
  gint i;

  self = BJB_CONTROLLER (user_data);
  priv = self->priv;
  priv->remaining_items = FALSE;



  if (!result && priv->group == BIJI_LIVING_ITEMS)
  {
    bjb_window_base_switch_to (priv->window, BJB_WINDOW_BASE_NO_RESULT);
    return;
  }

  sort_items (&result);
  i = 0;

  /* We are supposed to show n items. While not reach,
   * add to the list. As soon as reached, keep in mind that */

  for (l=result ; l!= NULL ; l=l->next)
  {
    if (i< priv->n_items_to_show)
    {
      _add_if_group_match (self,
                           &priv->items_to_show,
                           &l->data,
			   &i);
    }

    else if (l->next != NULL)
    {
      priv->remaining_items = TRUE;
      break;
    }
  }

  priv->items_to_show = g_list_reverse (priv->items_to_show);
  update (self);

  switch (self->priv->group)
  {
    case BIJI_ARCHIVED_ITEMS:
      bjb_window_base_switch_to (priv->window, BJB_WINDOW_BASE_ARCHIVE_VIEW);
      break;

    default:
      break;
  }

}



void
bjb_controller_apply_needle (BjbController *self)
{
  GList *result;
  BjbControllerPrivate *priv = self->priv;
  gchar *needle;

  needle = priv->needle;
  g_clear_pointer (&priv->items_to_show, g_list_free);

  /* Show all items
   * If no items, tell it - unless trash is visited */
  if (needle == NULL || g_strcmp0 (needle,"") == 0)
  {
    result = biji_manager_get_items (self->priv->manager, self->priv->group);

    if (result == NULL && priv->group == BIJI_LIVING_ITEMS)
      bjb_window_base_switch_to (self->priv->window, BJB_WINDOW_BASE_NO_NOTE);

    else
      update_controller_callback (result, self);

    g_list_free (result);

    return;
  }

  /* There is a research, apply lookup */
  biji_get_items_matching_async (self->priv->manager, self->priv->group, needle, update_controller_callback, self);
}

static void
on_needle_changed (BjbController *self)
{
  bjb_controller_apply_needle (self);
  g_signal_emit (self, bjb_controller_signals[SEARCH_CHANGED], 0);
}


/* Return FALSE to end timeout, see below on_manager_changed */
static gboolean
bjb_controller_set_window_active (BjbController *self)
{
  bjb_window_base_set_active (self->priv->window, TRUE);
  return FALSE;
}


/* Depending on the change at data level,
 * the view has to be totaly refreshed or just amended */
static void
on_manager_changed (BijiManager            *manager,
                    BijiItemsGroup          group,
                    BijiManagerChangeFlag   flag,
                    gpointer               *biji_item,
                    BjbController          *self)
{
  BjbControllerPrivate *priv = self->priv;
  BijiItem    *item = BIJI_ITEM (biji_item);
  GtkTreeIter iter;
  GtkTreeIter *p_iter = &iter;

  g_print ("manager changed\n");

  if (group != self->priv->group)
  {
    g_print ("no controller\n");
    g_debug ("Controller received signal for group %i while %i",
             group, self->priv->group);
    return;
  }

  g_mutex_lock (&priv->mutex);


  switch (flag)
  {
    /* If this is a *new* item, per def prepend */
    case BIJI_MANAGER_ITEM_ADDED:
          if (BIJI_IS_NOTE_OBJ (item))
            bjb_controller_get_iter (self, NULL, &p_iter);

          else if (BIJI_IS_NOTEBOOK (item))
            p_iter = NULL;

          bjb_controller_add_item_if_needed (self, item, TRUE, p_iter);
          priv->n_items_to_show ++;
          priv->items_to_show = g_list_prepend (priv->items_to_show, item);
          notify_displayed_items_changed (self);
      break;

    /* Same comment, prepend but notebook before note */
    case BIJI_MANAGER_NOTE_AMENDED:
    case BIJI_MANAGER_ITEM_ICON_CHANGED:
      if (bjb_controller_get_iter (self, item, &p_iter))
      {
        gtk_list_store_remove (GTK_LIST_STORE (priv->model), p_iter);

        if (BIJI_IS_NOTE_OBJ (item))
          bjb_controller_get_iter (self, NULL, &p_iter);

        else if (BIJI_IS_NOTEBOOK (item))
          p_iter = NULL;

        bjb_controller_add_item_if_needed (self, item, TRUE, p_iter);
      }
      break;

    case BIJI_MANAGER_ITEM_DELETED:
    case BIJI_MANAGER_ITEM_TRASHED:
    case BIJI_MANAGER_ITEM_RESTORED:
      if (bjb_controller_get_iter (self, item, &p_iter))
        gtk_list_store_remove (GTK_LIST_STORE (priv->model), p_iter);

      priv->items_to_show = g_list_remove (priv->items_to_show, item);
      if (priv->items_to_show == NULL)
        bjb_window_base_switch_to (self->priv->window, BJB_WINDOW_BASE_NO_NOTE);

      else
        notify_displayed_items_changed (self);

      break;

    /* Apply the needle to display the relevant items.
     * The window will ping to tell it's now active
     * Use another thread for this, because controller is now up to date,
     * and we need to unlock mutex,
     * since activating window can call this function! */
    default:
      bjb_controller_apply_needle (self);
      if (flag == BIJI_MANAGER_MASS_CHANGE)
        g_timeout_add (1, (GSourceFunc) bjb_controller_set_window_active, self);
  }

  g_mutex_unlock (&priv->mutex);
}

static void
bjb_controller_connect (BjbController *self)
{
  BjbControllerPrivate *priv = self->priv;

  if (!priv->connected)
  {
    priv->manager_change = g_signal_connect (self->priv->manager, "changed",
                                     G_CALLBACK(on_manager_changed), self);
    priv->connected = TRUE;
  }

  bjb_controller_update_view (self);
}

void
bjb_controller_disconnect (BjbController *self)
{
  BjbControllerPrivate *priv = self->priv;

  g_signal_handler_disconnect (priv->manager, priv->manager_change);
  priv->manager_change = 0;
}

static void
bjb_controller_constructed (GObject *obj)
{
  G_OBJECT_CLASS(bjb_controller_parent_class)->constructed(obj);

  bjb_controller_connect (BJB_CONTROLLER (obj));
}

static void
bjb_controller_class_init (BjbControllerClass *klass)
{
  GObjectClass* object_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (klass, sizeof (BjbControllerPrivate));

  object_class->get_property = bjb_controller_get_property;
  object_class->set_property = bjb_controller_set_property;
  object_class->finalize = bjb_controller_finalize;
  object_class->constructed = bjb_controller_constructed;

  bjb_controller_signals[SEARCH_CHANGED] = g_signal_new ( "search-changed" ,
                                                  G_OBJECT_CLASS_TYPE (klass),
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  g_cclosure_marshal_VOID__VOID,
                                                  G_TYPE_NONE,
                                                  0);

  bjb_controller_signals[DISPLAY_NOTES_CHANGED] = g_signal_new ( "display-items-changed" ,
                                                  G_OBJECT_CLASS_TYPE (klass),
                                                  G_SIGNAL_RUN_LAST,
                                                  0,
                                                  NULL,
                                                  NULL,
                                                  _biji_marshal_VOID__BOOLEAN_BOOLEAN,
                                                  G_TYPE_NONE,
                                                  2,
                                                  G_TYPE_BOOLEAN,
                                                  G_TYPE_BOOLEAN);

  properties[PROP_BOOK] = g_param_spec_object ("manager",
                                               "Book",
                                               "The BijiManager",
                                               BIJI_TYPE_MANAGER,
                                               G_PARAM_READWRITE |
                                               G_PARAM_CONSTRUCT |
                                               G_PARAM_STATIC_STRINGS);

  properties[PROP_WINDOW] = g_param_spec_object ("window",
                                                 "GtkWindow",
                                                 "BjbWindowBase",
                                                 BJB_TYPE_WINDOW_BASE,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_CONSTRUCT |
                                                 G_PARAM_STATIC_STRINGS);

  properties[PROP_NEEDLE] = g_param_spec_string ("needle",
                                                 "Needle",
                                                 "String to search notes",
                                                 NULL,
                                                 G_PARAM_READWRITE |
                                                 G_PARAM_CONSTRUCT |
                                                 G_PARAM_STATIC_STRINGS);


  properties[PROP_MODEL] = g_param_spec_object ("model",
                                                "Model",
                                                "The GtkTreeModel",
                                                GTK_TYPE_TREE_MODEL,
                                                G_PARAM_READABLE  |
                                                G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

}

BjbController *
bjb_controller_new (BijiManager  *manager,
                    GtkWindow     *window,
                    gchar         *needle)
{
  return g_object_new ( BJB_TYPE_CONTROLLER,
              "manager", manager,
              "window", window,
              "needle", needle,
              NULL);
}

void
bjb_controller_set_manager (BjbController *self, BijiManager  *manager )
{
  self->priv->manager = manager ;
}

void
bjb_controller_set_needle (BjbController *self, const gchar *needle )
{
  if (self->priv->needle)
    g_free (self->priv->needle);

  self->priv->needle = g_strdup (needle);
  on_needle_changed (self);
}

gchar *
bjb_controller_get_needle (BjbController *self)
{
  if (!self->priv->needle)
    return NULL;

  return self->priv->needle;
}

GtkTreeModel *
bjb_controller_get_model  (BjbController *self)
{
  return self->priv->model ;
}


gboolean
bjb_controller_shows_item (BjbController *self)
{
  return (self->priv->items_to_show != NULL);
}

BijiNotebook *
bjb_controller_get_notebook (BjbController *self)
{
  return self->priv->notebook;
}


void
bjb_controller_set_notebook (BjbController *self,
                             BijiNotebook *coll)
{
  /* Going back from a notebook */
  if (!coll)
  {
    if (!self->priv->notebook)
      return;

    bjb_window_base_switch_to (self->priv->window, BJB_WINDOW_BASE_SPINNER_VIEW);
    self->priv->notebook = NULL;
    bjb_controller_apply_needle (self);
    return;
  }

  /* Opening an __existing__ notebook */
  bjb_window_base_switch_to (self->priv->window, BJB_WINDOW_BASE_SPINNER_VIEW);
  g_clear_pointer (&self->priv->items_to_show, g_list_free);
  g_clear_pointer (&self->priv->needle, g_free);

  self->priv->needle = g_strdup ("");
  self->priv->notebook = coll;
  biji_get_items_with_notebook_async (self->priv->manager,
                                        biji_item_get_title (BIJI_ITEM (coll)),
                                        update_controller_callback,
                                        self);
}



BijiItemsGroup
bjb_controller_get_group (BjbController *self)
{
  return self->priv->group;
}



void
bjb_controller_set_group (BjbController   *self,
                          BijiItemsGroup   group)
{
  if (self->priv->group == group)
    return;

  g_clear_pointer (&self->priv->items_to_show, g_list_free);
  self->priv->group = group;

  /* Living group : refresh the ui */
  if (group == BIJI_LIVING_ITEMS)
  {
    if (self->priv->notebook != NULL)
      bjb_controller_set_notebook (self, self->priv->notebook);

    else
      bjb_controller_apply_needle (self);
    //bjb_window_base_switch_to (self->priv->window, group);
    return;
  }


  else /* Archives */
  {
    bjb_controller_apply_needle (self);
  }
}



void
bjb_controller_show_more (BjbController *self)
{
  self->priv->n_items_to_show += BJB_ITEMS_SLICE;

  /* FIXME: this method to refresh is just non sense */

  if (self->priv->notebook != NULL)
    bjb_controller_set_notebook (self, self->priv->notebook);

  else
    on_needle_changed (self);
}



gboolean
bjb_controller_get_remaining_items (BjbController *self)
{
  return self->priv->remaining_items;
}
