/* bjb-settings.h
 * Copyright © 2012, 2013 Pierre-Yves LUYTEN <py@luyten.fr>
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

#ifndef _BJB_SETTINGS_H_
#define _BJB_SETTINGS_H_

#include <glib-object.h>



G_BEGIN_DECLS

#define BJB_TYPE_SETTINGS             (bjb_settings_get_type ())
#define BJB_SETTINGS(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BJB_TYPE_SETTINGS, BjbSettings))
#define BJB_SETTINGS_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BJB_TYPE_SETTINGS, BjbSettingsClass))
#define BJB_IS_SETTINGS(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BJB_TYPE_SETTINGS))
#define BJB_IS_SETTINGS_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), BJB_TYPE_SETTINGS))
#define BJB_SETTINGS_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), BJB_TYPE_SETTINGS, BjbSettingsClass))

typedef struct _BjbSettingsClass BjbSettingsClass;
typedef struct _BjbSettings BjbSettings;

typedef struct _BjbSettingsPrivate BjbSettingsPrivate;

struct _BjbSettingsClass
{
  GSettingsClass parent_class;
};

struct _BjbSettings
{
  GSettings parent_instance;
  BjbSettingsPrivate *priv;
};


GType             bjb_settings_get_type                   (void) G_GNUC_CONST;


BjbSettings      *bjb_settings_new                        (void);


gboolean          bjb_settings_use_system_font            (BjbSettings *settings);


void              bjb_settings_set_use_system_font        (BjbSettings *settings,
                                                           gboolean value);


const gchar      *bjb_settings_get_default_font           (BjbSettings *settings);


const gchar      *bjb_settings_get_default_color          (BjbSettings *settings);


const gchar      *bjb_settings_get_default_location       (BjbSettings *settings);


gchar            *bjb_settings_get_system_font            (BjbSettings *settings);


void              show_bijiben_settings_window            (GtkWidget *parent_window);

G_END_DECLS

#endif /* _BJB_SETTINGS_H_ */
