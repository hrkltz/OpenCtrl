#!/bin/bash

curl -X POST "http://$HRKLTZ_OPENCTRL_CORE_IP/mouse" \
     -H "Content-Type: application/json" \
     -d '{"buttons":1,"dx":10,"dy":0,"wheel":0}'