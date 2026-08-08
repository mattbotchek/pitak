#!/bin/sh
set -eux
# Compile and run send-once test
sudo rm -f /run/pigpio.pid /var/run/pigpio.pid || true
sudo g++ /tmp/lora_send_once.cpp -o /tmp/pitak_test_send -lpigpio || true
sudo env LD_LIBRARY_PATH=/usr/local/lib /tmp/pitak_test_send > /tmp/pitak_test_send.log 2>&1 &
PID=$!
for i in $(seq 1 12); do
  if ! ps -p $PID > /dev/null 2>&1; then break; fi
  sleep 1
done
sleep 1
cat /tmp/pitak_test_send.log || true
if grep -q "TX: PING" /tmp/pitak_test_send.log; then
  echo "TEST OK"
  exit 0
else
  echo "TEST FAIL"
  exit 2
fi
