# -*- coding: UTF-8 -*-

# Copyright (C) 2005, 2006 Canonical Ltd.
# Written by Tollef Fog Heen <tfheen@ubuntu.com> and
# Colin Watson <cjwatson@ubuntu.com>
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

import re
import os
from ubiquity.filteredcommand import FilteredCommand
from ubiquity import keyboard_names
from ubiquity import misc

class ConsoleSetup(FilteredCommand):
    def prepare(self):
        self.preseed('console-setup/ask_detect', 'false')
        # Technically we should provide a version as the second argument,
        # but that isn't currently needed and it would require querying
        # apt/dpkg for the current version, which would be slow, so we don't
        # bother for now.
        return (['/usr/lib/ubiquity/console-setup/console-setup.postinst',
                 'configure'],
                ['^console-setup/layout'])

    def run(self, priority, question):
        # TODO cjwatson 2006-09-07: we're going to need a separate UI
        # element for variant
        if question == 'console-setup/layout':
            # TODO cjwatson 2006-09-07: no console-setup support for layout
            # choice translation yet
            self.frontend.set_keyboard_choices(
                self.choices_untranslated(question))
            self.frontend.set_keyboard(self.db.get(question))
            return super(ConsoleSetup, self).run(priority, question)
        else:
            return True

    def ok_handler(self):
        keyboard = self.frontend.get_keyboard()
        if keyboard is not None:
            self.preseed('console-setup/layout', keyboard)
        return super(ConsoleSetup, self).ok_handler()

    # TODO cjwatson 2006-09-07: This is duplication from console-setup, but
    # currently difficult to avoid; we need to apply the keymap immediately
    # when the user selects it in the UI (and before they move to the next
    # page), so this needs to be fast and moving through console-setup to
    # get the corrections it applies will be too slow.
    def adjust_keyboard(self, model, layout, variant, options):
        """Apply any necessary tweaks to the supplied model, layout, variant,
        and options."""

        if layout in ('am', 'ara', 'ben', 'bd', 'bg', 'bt', 'by', 'deva', 'ge',
                      'gh', 'gr', 'guj', 'guru', 'il', 'in', 'ir', 'iku', 'jp',
                      'kan', 'kh', 'la', 'lao', 'lk', 'mk', 'mm', 'mn', 'mv',
                      'mal', 'ori', 'pk', 'ru', 'scc', 'sy', 'syr', 'tel',
                      'th', 'tj', 'tam', 'ua', 'uz'):
            latin = False
            real_layout = 'us,%s' % layout
        elif layout == 'cs':
            if variant.startswith('latin'):
                latin = True
                real_layout = layout
            else:
                latin = False
                real_layout = 'cs,cs'
        else:
            latin = True
            real_layout = layout

        if latin:
            real_variant = variant
        elif real_layout == 'cs,cs':
            if variant == 'yz':
                real_variant = 'latinyz,%s' % variant
            elif variant == 'alternatequotes':
                real_variant = 'latinalternatequotes,%s' % variant
            else:
                real_variant = 'latin,%s' % variant
        else:
            real_variant = ',%s' % variant

        if latin:
            real_options = options
        else:
            # TODO cjwatson 2006-09-07: use existing options and remove any
            # existing grp:*toggle; honour crazy preseeding; probably not
            # quite right, especially for Apples which may need a level 3
            # shift
            real_options = ['grp:alt_shift_toggle']

        return (model, real_layout, real_variant, real_options)

    def apply_keyboard(self, layout):
        model = self.db.get('console-setup/modelcode')
        if layout in keyboard_names.layouts:
            layout = keyboard_names.layouts[layout]
            (model, layout, variant, options) = \
                self.adjust_keyboard(model, layout, '', [])
            self.debug("Setting keyboard layout: %s %s %s %s" %
                       (model, layout, variant, options))
            self.apply_real_keyboard(model, layout, variant, options)
        else:
            self.debug("Unknown keyboard layout '%s'" % layout)

    def apply_real_keyboard(self, model, layout, variant, options):
        args = []
        if model is not None and model != '':
            args.extend(("-model", model))
        args.extend(("-layout", layout))
        if variant != '':
            args.extend(("-variant", variant))
        for option in options:
            args.extend(("-option", option))
        misc.ex("setxkbmap", *args)

    def cleanup(self):
        # TODO cjwatson 2006-09-07: I'd use dexconf, but it seems reasonable
        # for somebody to edit /etc/X11/xorg.conf on the live CD and expect
        # that to be carried over to the installed system (indeed, we've
        # always supported that up to now). So we get this horrible mess
        # instead ...

        model = self.db.get('console-setup/modelcode')
        layout = self.db.get('console-setup/layoutcode')
        variant = self.db.get('console-setup/variantcode')
        options = self.db.get('console-setup/optionscode')
        self.apply_real_keyboard(model, layout, variant, options.split(','))

        if layout == '':
            return

        oldconfigfile = '/etc/X11/xorg.conf'
        newconfigfile = '/etc/X11/xorg.conf.new'
        oldconfig = open(oldconfigfile)
        newconfig = open(newconfigfile, 'w')

        re_section_inputdevice = re.compile(r'\s*Section\s+"InputDevice"\s*$')
        re_driver_kbd = re.compile(r'\s*Driver\s+"kbd"\s*$')
        re_endsection = re.compile(r'\s*EndSection\s*$')
        re_option_xkbmodel = re.compile(r'(\s*Option\s*"XkbModel"\s*).*')
        re_option_xkblayout = re.compile(r'(\s*Option\s*"XkbLayout"\s*).*')
        re_option_xkbvariant = re.compile(r'(\s*Option\s*"XkbVariant"\s*).*')
        re_option_xkboptions = re.compile(r'(\s*Option\s*"XkbOptions"\s*).*')
        in_inputdevice = False
        in_inputdevice_kbd = False
        done = {'model': model == '', 'layout': False,
                'variant': variant == '', 'options': options == ''}

        for line in oldconfig:
            line = line.rstrip('\n')
            if re_section_inputdevice.match(line) is not None:
                in_inputdevice = True
            elif in_inputdevice and re_driver_kbd.match(line) is not None:
                in_inputdevice_kbd = True
            elif re_endsection.match(line) is not None:
                if in_inputdevice_kbd:
                    if not done['model']:
                        print >>newconfig, ('\tOption\t\t"XkbModel"\t"%s"' %
                                            model)
                    if not done['layout']:
                        print >>newconfig, ('\tOption\t\t"XkbLayout"\t"%s"' %
                                            layout)
                    if not done['variant']:
                        print >>newconfig, ('\tOption\t\t"XkbVariant"\t"%s"' %
                                            variant)
                    if not done['options']:
                        print >>newconfig, ('\tOption\t\t"XkbOptions"\t"%s"' %
                                            options)
                in_inputdevice = False
                in_inputdevice_kbd = False
                done = {'model': model == '', 'layout': False,
                        'variant': variant == '', 'options': options == ''}
            elif in_inputdevice_kbd:
                match = re_option_xkbmodel.match(line)
                if match is not None:
                    if model == '':
                        # hmm, not quite sure what to do here; guessing that
                        # forcing to pc105 will be reasonable
                        line = match.group(1) + '"pc105"'
                    else:
                        line = match.group(1) + '"%s"' % model
                    done['model'] = True
                else:
                    match = re_option_xkblayout.match(line)
                    if match is not None:
                        line = match.group(1) + '"%s"' % layout
                        done['layout'] = True
                    else:
                        match = re_option_xkbvariant.match(line)
                        if match is not None:
                            if variant == '':
                                continue # delete this line
                            else:
                                line = match.group(1) + '"%s"' % variant
                            done['variant'] = True
                        else:
                            match = re_option_xkboptions.match(line)
                            if match is not None:
                                if options == '':
                                    continue # delete this line
                                else:
                                    line = match.group(1) + '"%s"' % options
                                done['options'] = True
            print >>newconfig, line

        newconfig.close()
        oldconfig.close()
        os.rename(newconfigfile, oldconfigfile)