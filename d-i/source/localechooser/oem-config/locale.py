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

class Locale(WizardStep):
    def __init__(self, glade):
        super(Locale, self).__init__(glade)
        self.shortlist = False

    def prepare(self, db):
        super(Locale, self).prepare(db)

        label_qs = {}
        label_qs['language_label'] = 'languagechooser/language-name'
        label_qs['country_label'] = 'countrychooser/country-name'
        self.translate_labels(label_qs)

        self.restart = False

        language_combo = self.glade.get_widget('language_combo')
        cell = gtk.CellRendererText()
        language_combo.pack_start(cell, True)
        language_combo.add_attribute(cell, 'text', 0)
        list_store = gtk.ListStore(gobject.TYPE_STRING)
        language_combo.set_model(list_store)
        current_language = self.value_index('languagechooser/language-name')
        self.language_choices = self.split_choices(
            unicode(self.db.metaget('languagechooser/language-name',
                                    'choices-en.utf-8'), 'utf-8'))
        self.language_choices_c = self.choices_untranslated(
            'languagechooser/language-name')
        self.language_display_map = {}
        for i in range(len(self.language_choices)):
            if i == current_language:
                shortchoice = re.sub(r'.*? *- (.*)', r'\1',
                                     self.language_choices[i])
            else:
                shortchoice = re.sub(r'(.*?) *- (.*)', r'\1 (\2)',
                                     self.language_choices[i])
            list_store.append([shortchoice])
            self.language_display_map[shortchoice] = self.language_choices[i]
        language_combo.set_active(current_language)
        language_combo.connect('changed', self.language_handler)

        country_combo = self.glade.get_widget('country_combo')
        cell = gtk.CellRendererText()
        country_combo.pack_start(cell, True)
        country_combo.add_attribute(cell, 'text', 0)
        list_store = gtk.ListStore(gobject.TYPE_STRING)
        country_combo.set_model(list_store)
        self.update_country_list('countrychooser/country-name')
        country_combo.connect('changed', self.country_handler)

    def update_country_list(self, question):
        country_combo = self.glade.get_widget('country_combo')
        list_store = country_combo.get_model()
        list_store.clear()
        country_choices = self.choices(question)
        for choice in country_choices:
            list_store.append([choice])
        try:
            country_combo.set_active(self.value_index(question))
        except ValueError:
            pass

    def preseed_language(self, widget):
        language = self.language_display_map[unicode(widget.get_active_text())]
        for i in range(len(self.language_choices)):
            if self.language_choices[i] == language:
                self.preseed('languagechooser/language-name',
                             self.language_choices_c[i])
                break
        else:
            raise ValueError, language

    def preseed_country(self, widget):
        country = unicode(widget.get_active_text())
        if self.shortlist:
            self.preseed_as_c('countrychooser/shortlist', country)
        else:
            self.preseed_as_c('countrychooser/country-name', country)

    def language_handler(self, widget, data=None):
        self.preseed_language(widget)
        # We now need to run through most of localechooser, but stop just
        # before the end. This can be done by backing up from
        # localechooser/supported-locales, so leave a note for ourselves to
        # do so.
        self.restart = True
        self.succeeded = True
        # The debconf frontend will now restart, so shut down the dialog
        # while we still can.
        self.dialog.hide()
        gtk.main_quit()

    def country_handler(self, widget, data=None):
        self.preseed_country(widget)

    def ok_handler(self, widget, data=None):
        self.preseed_language(self.glade.get_widget('language_combo'))
        self.preseed_country(self.glade.get_widget('country_combo'))
        super(Locale, self).ok_handler(widget, data)

    def run(self, priority, question):
        if question == 'localechooser/supported-locales':
            if self.restart:
                self.succeeded = False
                self.done = True
                return False
            else:
                return True

        if question == 'countrychooser/shortlist':
            self.shortlist = True
        else:
            self.shortlist = False
        self.update_country_list(question)
        return super(Locale, self).run(priority, question)

stepname = 'Locale'
