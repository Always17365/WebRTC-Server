#!/bin/sh
# Stop mediaserver shell
# Author: Max.Chiu
# Date: 2019/08/13

APP_DIR=$(dirname $(readlink -f "$0"))/..
cd $APP_DIR
APP_PID=`cat $APP_DIR/run/mediaserver.pid`
EXIT=0

CheckAndWait() {
  for (( i=1; i<5; i++))
  do  
    wait=0
    PID=`ps -ef | grep $APP_PID | grep -v grep`
    if [ -n "$PID" ]; then
      wait=1
    fi
    
    if [ "$wait" == 1 ]; then
      # 
      echo "# Waiting Mediaserver to exit...... ($APP_PID)"
      sleep 1
    else
    echo "# Mediaserver already exit...... ($APP_PID)"
      EXIT=1
      break
    fi 
  done
  return $exit
}

# Stop mediaserver
if [ -n "$(echo $APP_PID| sed -n "/^[0-9]\+$/p")" ];then
  echo "# Stop Mediaserver ($APP_PID) "
  kill $APP_PID
  CheckAndWait
  if [ $EXIT == 0 ];then
    echo "# Stop Mediaserver force ($APP_PID) "
    kill -9 $APP_PID
  fi
fi

cd - >/dev/null 2>&1