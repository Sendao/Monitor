#! /bin/sh
### BEGIN INIT INFO
# Provides:          monitor
# Required-Start:    $syslog $time $remote_fs
# Required-Stop:     $syslog $time $remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Controller for other services
# Description:       Written by Scott Powell
### END INIT INFO

PATH=/bin:/usr/bin:/sbin:/usr/sbin
DAEMON=/var/rad/monitor /var/rad/monitor.cfg
PIDFILE=/var/run/monitor.pid
NAME=monitor

test -x $DAEMON || exit 0

. /lib/lsb/init-functions

case "$1" in
  start)
	log_daemon_msg "Starting deferred execution scheduler" "$NAME"
	start_daemon -p $PIDFILE $DAEMON
	log_end_msg $?
    ;;
  stop)
	log_daemon_msg "Stopping deferred execution scheduler" "$NAME"
	killproc -p $PIDFILE $DAEMON
	log_end_msg $?
    ;;
  force-reload|restart)
    $0 stop
    $0 start
    ;;
  status)
    status_of_proc -p $PIDFILE $DAEMON $NAME && exit 0 || exit $?
    ;;
  *)
    echo "Usage: /etc/init.d/$NAME {start|stop|restart|force-reload|status}"
    exit 1
    ;;
esac

exit 0
