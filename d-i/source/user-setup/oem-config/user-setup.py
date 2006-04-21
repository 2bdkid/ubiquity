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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

import gtk
import debconf
from wizardstep import WizardStep

class User(WizardStep):
    def prepare(self, db):
        super(User, self).prepare(db)

        label_qs = {}
        label_qs['user_fullname_label'] = 'passwd/user-fullname'
        label_qs['user_name_label'] = 'passwd/username'
        label_qs['user_password_label'] = 'passwd/user-password'
        label_qs['user_password_confirm_label'] = 'passwd/user-password-again'
        self.translate_labels(label_qs)

        # TODO: skip this if there's already a user configured, or re-ask
        # and create a new one, or what?

    def ok_handler(self, widget, data=None):
        fullname = self.glade.get_widget('user_fullname_entry').get_text()
        username = self.glade.get_widget('user_name_entry').get_text()
        password = self.glade.get_widget('user_password_entry').get_text()
        password_confirm = \
            self.glade.get_widget('user_password_confirm_entry').get_text()

        # TODO: validation!

        self.preseed('passwd/user-fullname', fullname)
        self.preseed('passwd/username', username)
        # TODO: maybe encrypt these first
        self.preseed('passwd/user-password', password)
        self.preseed('passwd/user-password-again', password_confirm)
        self.preseed('passwd/user-uid', '')

        super(User, self).ok_handler(widget, data)

stepname = 'User'
