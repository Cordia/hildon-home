#!/usr/bin/python2.5

import time

import gobject

import dbus
import dbus.mainloop.glib

ids = []
emitted = 0

def emit_notification(iface):
	global emitted
	global ids
	emitted += 1
        id = iface.Notify ('test-send.py', '0', '', 'sms-%d' % (emitted), 'text:%d' % (emitted), ['default', 'default'], { 'category': 'sms-message', 'persistent': dbus.Byte(0), 'no-notification-window': False }, 0)
	print "%dsms (id %d)" % (emitted, id)
	ids.append(id)

	if (emitted == 50):
		close_notifications(iface)
		return False

	return True

def close_notifications(iface):
	global ids
	for id in ids:
        	iface.CloseNotification (id)
		print "(id %d) closed" % (id)


def notification_closed_handler(id):
	print 'notification closed. id: ' + str(id)

def action_invoked_handler(id, action_key):
	print 'action invoked. id: ' + str(id) + ', action_key: ' + action_key

if __name__ == '__main__':
	dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)

	bus = dbus.SessionBus()

	proxy = bus.get_object('org.freedesktop.Notifications', '/org/freedesktop/Notifications')
	iface = dbus.Interface(proxy, 'org.freedesktop.Notifications')
	proxy.connect_to_signal('NotificationClosed', notification_closed_handler, dbus_interface='org.freedesktop.Notifications')
	proxy.connect_to_signal('ActionInvoked', action_invoked_handler, dbus_interface='org.freedesktop.Notifications')

#	iface.SystemNoteInfoprint ('foo1');
#	iface.SystemNoteInfoprint ('foo2');
#	iface.SystemNoteInfoprint ('foo3');

#	iface.SystemNoteDialog ('message', 0, 'label');

# iface.Notify ('test-send.py', '0', 'qgn_list_messagin', 'Jan Arne Petersen', 'Subject', ['default', 'default'], { 'category': 'email' }, 0)

	gobject.timeout_add(50, emit_notification, iface)
	

	loop = gobject.MainLoop()
	loop.run()
