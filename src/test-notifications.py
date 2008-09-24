#!/usr/bin/python2.5

import dbus
import time

bus = dbus.SessionBus()

proxy = bus.get_object('org.freedesktop.Notifications', '/org/freedesktop/Notifications')

iface = dbus.Interface(proxy, 'org.freedesktop.Notifications')

# iface.SystemNoteInfoprint ('foo1');
# iface.SystemNoteInfoprint ('foo2');
# iface.SystemNoteInfoprint ('foo3');

# iface.Notify ('test-send.py', '0', 'qgn_list_messagin', 'Jan Arne Petersen', 'Subject', ['default', 'default'], { 'category': 'email' }, 0)

iface.Notify ('test-send.py', '0', 'qgn_list_messagin', 'Jan Arne Petersen', 'Subject 1', ['default', 'default'], { 'category': 'email-message', 'time': dbus.Int64 (time.time() - 1800) }, 0)

iface.Notify ('test-send.py', '0', 'qgn_list_messagin', 'Jan Arne Petersen', 'Subject 2', ['default', 'default'], { 'category': 'email-message', 'time': dbus.Int64 (time.time()) }, 0)

