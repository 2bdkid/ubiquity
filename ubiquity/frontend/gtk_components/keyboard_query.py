import gtk, gobject
from ubiquity.keyboard_detector import *

class Keyrow(gtk.HBox):
    def __init__(self):
        gtk.HBox.__init__(self, spacing=24)

    def add_character(self, key):
        l = gtk.Label('<big>%s</big>' % key)
        l.set_use_markup(True)
        self.add(l)
        l.show()

    def clear(self):
        for ch in self.get_children():
            self.remove(ch)

class KeyboardQuery(gtk.Window):
    __gtype_name__ = 'KeyboardQuery'
    __gsignals__ = { 'layout_result' : (gobject.SIGNAL_RUN_FIRST,
                    gobject.TYPE_NONE, (gobject.TYPE_STRING,)) }
    def __init__(self):
        gtk.Window.__init__(self)
        
        self.set_title('')
        self.set_modal(True)
        self.set_type_hint(gtk.gdk.WINDOW_TYPE_HINT_DIALOG)
        self.vbox = gtk.VBox()
        
        # TODO i18n
        self.heading = gtk.Label('Please press one of the following keys:')
        self.vbox.add(self.heading)
        
        self.keyrow = Keyrow()
        self.vbox.add(self.keyrow)
        
        self.buttons = gtk.HButtonBox()
        self.buttons.set_border_width(4)
        self.buttons.set_layout(gtk.BUTTONBOX_END)
        # FIXME evand 2009-12-16: i18n
        no = gtk.Button(stock=gtk.STOCK_NO)
        yes = gtk.Button(stock=gtk.STOCK_YES)
        self.buttons.add(no)
        self.buttons.add(yes)
        self.vbox.add(self.buttons)
        
        self.add(self.vbox)

        yes.connect('clicked', self.have_key)
        no.connect('clicked', self.no_have_key)
        self.connect('key_press_event', self.key_press_event)
        
        self.keyboard_detect = KeyboardDetector()
        self.buttons.hide()

    def run(self, *args):
        self.show_all()
        r = self.keyboard_detect.read_step(0)
        self.process(r)
    
    def process(self, r):
        self.keyrow.clear()
        for k in self.keyboard_detect.symbols:
            self.keyrow.add_character(k)
        if r == KeyboardDetector.PRESS_KEY:
            # TODO i18n
            self.heading.set_label('Please press one of the following keys:')
            self.buttons.hide()
        elif (r == KeyboardDetector.KEY_PRESENT or
              r == KeyboardDetector.KEY_PRESENT_P):
            # TODO i18n
            self.heading.set_label('Is the following key present on your keyboard?')
            self.buttons.show()
        elif r == KeyboardDetector.RESULT:
            self.emit('layout_result', self.keyboard_detect.result)
            self.hide()
        else:
            raise Exception, 'should not have got here'

    def have_key(self, *args):
        r = self.keyboard_detect.read_step(self.keyboard_detect.present)
        self.process(r)

    def no_have_key(self, *args):
        r = self.keyboard_detect.read_step(self.keyboard_detect.not_present)
        self.process(r)

    def key_press_event(self, widget, event):
        # FIXME need to account for possible remapping.  Find the API to translate
        # kernel keycodes to X keycodes (xkb).
        # MIN_KEYCODE = 8
        
        code = event.hardware_keycode - 8
        if code > 255:
            return
        if code in self.keyboard_detect.keycodes:
            # XKB doesn't support keycodes > 255.
            c = self.keyboard_detect.keycodes[code]
            r = self.keyboard_detect.read_step(c)
            self.process(r)

gobject.type_register(KeyboardQuery)