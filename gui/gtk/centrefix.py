#!/usr/bin/python3
from gi.repository import Gtk

def password_strength(password):
    upper = lower = digit = symbol = 0
    for char in password:
        if char.isdigit():
            digit += 1
        elif char.islower():
            lower += 1
        elif char.isupper():
            upper += 1
        else:
            symbol += 1
    length = len(password)
    if length > 5:
        length = 5
    if digit > 3:
        digit = 3
    if upper > 3:
        upper = 3
    if symbol > 3:
        symbol = 3
    strength = (((length * 0.1) - 0.2)
               + (digit * 0.1)
               + (symbol * 0.15)
               + (upper * 0.1))
    if strength > 1:
        strength = 1
    if strength < 0:
        strength = 0
    return strength

def human_password_strength(password):
    strength = password_strength(password)
    length = len(password)
    if length == 0:
        hint = ''
        color = ''
    elif length < 6:
        hint = 'too_short'
        color = 'darkred'
    elif strength < 0.5:
        hint = 'weak'
        color = 'darkred'
    elif strength < 0.75:
        hint = 'fair'
        color = 'darkorange'
    elif strength < 0.9:
        hint = 'good'
        color = 'darkgreen'
    else:
        hint = 'strong'
        color = 'darkgreen'
    return (hint, color)

class CentreFix:
    def __init__(self):
        builder = Gtk.Builder()
        builder.add_from_file('centrefix.ui')

        self.password_grid = builder.get_object(
            'password_grid')
        self.password = builder.get_object(
            'password')
        self.verified_password = builder.get_object(
            'verified_password')
        self.password_strength = builder.get_object(
            'password_strength')
        self.password_match = builder.get_object(
            'password_match')
        self.password_strength_pages = {
            'empty': 0,
            'too_short': 1,
            'weak': 2,
            'fair': 3,
            'good': 4,
            'strong': 5,
        }
        self.password_match_pages = {
            'empty': 0,
            'mismatch': 1,
            'ok': 2,
        }

        builder.connect_signals(self)
        win = builder.get_object('window1')
        win.connect("delete-event", Gtk.main_quit)
        win.show()
        Gtk.main()

    def info_loop(self, unused_widget):
        complete = True
        passw = self.password.get_text()
        vpassw = self.verified_password.get_text()
        if passw != vpassw:
            complete = False
            self.password_match.set_current_page(
                self.password_match_pages['empty'])
            if passw and (len(vpassw) / float(len(passw)) > 0.8):
                self.password_match.set_current_page(
                    self.password_match_pages['mismatch'])
        else:
            self.password_match.set_current_page(
                self.password_match_pages['empty'])

        if not passw:
            self.password_strength.set_current_page(
                self.password_strength_pages['empty'])
            complete = False
        else:
            txt = human_password_strength(passw)[0]
            self.password_strength.set_current_page(
                self.password_strength_pages[txt])
            if passw == vpassw:
                self.password_match.set_current_page(
                    self.password_match_pages['ok'])
        return complete

if __name__ == '__main__':
    CentreFix()
