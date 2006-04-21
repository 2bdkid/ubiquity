# -*- coding: UTF-8 -*-

# Copyright (C) 2005 Canonical Ltd.
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

import re
import gobject
import gtk
import debconf
from wizardstep import WizardStep

class Keyboard(WizardStep):
    def __init__(self, glade):
        super(Keyboard, self).__init__(glade)
        self.default_keymap = None

    def prepare(self, db):
        super(Keyboard, self).prepare(db)

        self.translate_title('oem-config/menu/keyboard')
        label_qs = {}
        label_qs['select_keyboard_label'] = 'kbd-chooser/method'
        self.translate_labels(label_qs)

        # cdebconf doesn't translate selects back to C for storage in the
        # database, so kbd-chooser isn't prepared for it. (I think this is a
        # cdebconf bug. Nevertheless ...)
        self.method = self.db.metaget('kbd-chooser/do_select', 'description')

    def ok_handler(self, widget, data=None):
        keymap = self.glade.get_widget('select_keyboard_combo').get_active_text()
        keymap_c = self.translate_to_c(self.question, keymap)
        self.preseed(self.question, keymap_c)
        self.method = keymap_c

        super(Keyboard, self).ok_handler(widget, data)

    def run(self, priority, question):
        if question == 'kbd-chooser/method':
            if self.done:
                return self.succeeded
            self.preseed('kbd-chooser/method', self.method)
            return True

        self.question = question

        select_keyboard_combo = self.glade.get_widget('select_keyboard_combo')
        cell = gtk.CellRendererText()
        select_keyboard_combo.pack_start(cell, True)
        select_keyboard_combo.add_attribute(cell, 'text', 0)
        list_store = gtk.ListStore(gobject.TYPE_STRING)
        select_keyboard_combo.set_model(list_store)
        for choice in self.choices(question):
            list_store.append([choice])
        try:
            select_keyboard_combo.set_active(self.value_index(question))
        except ValueError:
            pass

        return super(Keyboard, self).run(priority, question)

stepname = 'Keyboard'
