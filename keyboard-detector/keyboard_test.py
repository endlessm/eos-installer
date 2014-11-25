#!/usr/bin/python

from gi.repository import Gtk
import os

import keyboard_query

class KeyboardTest():

    def __init__(self):
        self.query = keyboard_query.KeyboardQuery(self)
        self.query.connect('layout_result', self.calculate_result)
        self.query.connect('delete-event', self.calculate_closed)
        self.query.connect('destroy', self.destroy)

    def run(self):
        self.query.run()

    def calculate_result(self, w, keymap):
        print keymap
        self.calculate_closed()

    def calculate_closed(self, *args):
        if self.query:
            self.query.destroy()
        self.query = None

    def destroy(self, w):
        Gtk.main_quit()

if __name__ == '__main__':

    test = KeyboardTest()
    test.run()
    Gtk.main()
