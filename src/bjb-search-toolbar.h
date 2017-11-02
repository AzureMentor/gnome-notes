/* bjb-search-toolbar.h
 * Copyright © 2012, 2013 Pierre-Yves LUYTEN <py@luyten.fr>
 * Copyright 2017 Mohammed Sadiq <sadiq@sadiqpk.org>
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

#ifndef BJB_SEARCH_TOOLBAR_H
#define BJB_SEARCH_TOOLBAR_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define BJB_TYPE_SEARCH_TOOLBAR (bjb_search_toolbar_get_type ())

G_DECLARE_FINAL_TYPE (BjbSearchToolbar, bjb_search_toolbar, BJB, SEARCH_TOOLBAR, GtkSearchBar)


BjbSearchToolbar  *bjb_search_toolbar_new                   (GtkWidget *window,
                                                             BjbController *controller);


void               bjb_search_toolbar_disconnect            (BjbSearchToolbar *self);


void               bjb_search_toolbar_connect               (BjbSearchToolbar *self);



G_END_DECLS

#endif /* BJB_SEARCH_TOOLBAR_H */
