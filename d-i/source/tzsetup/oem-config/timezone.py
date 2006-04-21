# -*- coding: UTF-8 -*-

# Copyright (C) 2005, 2006 Canonical Ltd.
# Written by Colin Watson <cjwatson@ubuntu.com>.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import os
import gobject
import gtk
import debconf
from wizardstep import WizardStep

def _find_in_choices(choices, item):
    for index in range(len(choices)):
        if choices[index] == item:
            return index
    return None

class Timezone(WizardStep):
    def prepare(self, db):
        super(Timezone, self).prepare(db)

        self.translate_title('oem-config/menu/timezone')

        select_zone = self.glade.get_widget('select_zone_combo')
        cell = gtk.CellRendererText()
        select_zone.pack_start(cell, True)
        select_zone.add_attribute(cell, 'text', 0)
        list_store = gtk.ListStore(gobject.TYPE_STRING)
        select_zone.set_model(list_store)

    def ok_handler(self, widget, data=None):
        zone = self.glade.get_widget('select_zone_combo').get_active_text()
        self.preseed('time/zone', self.zone_c_map[unicode(zone)])

        super(Timezone, self).ok_handler(widget, data)

    def run(self, priority, question):
        if question == 'time/zone':
            select_zone = self.glade.get_widget('select_zone_combo')
            self.translate_labels({'select_zone_label': question})
            list_store = select_zone.get_model()
            list_store.clear()

            choices = self.choices(question)
            choices_c = self.choices_untranslated(question)

            self.zone_c_map = {}
            for i in range(len(choices)):
                list_store.append([choices[i]])
                self.zone_c_map[choices[i]] = choices_c[i]

            timezone = self.db.get(question)
            if timezone == '':
                if os.path.isfile('/etc/timezone'):
                    timezone = open('/etc/timezone').readline().strip()
                elif os.path.islink('/etc/localtime'):
                    timezone = os.readlink('/etc/localtime')
                    if timezone.startswith('/usr/share/zoneinfo/'):
                        timezone = timezone[len('/usr/share/zoneinfo/'):]
                    else:
                        timezone = None
                else:
                    timezone = None

            if timezone is None:
                select_zone.set_active(0)
            else:
                active = _find_in_choices(choices_c, timezone)
                if active is None:
                    select_zone.set_active(0)
                else:
                    select_zone.set_active(active)

        elif question == 'tzsetup/selected':
            # ignored for now
            return True

        return super(Timezone, self).run(priority, question)

stepname = 'Timezone'
