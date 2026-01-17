#!/bin/bash

curl -X POST "http://$HRKLTZ_OPENCTRL_CORE_IP/gamepad" \
     -H "Content-Type: application/json" \
     -d '{"buttons":0,"leftX":0,"leftY":0,"rightX":0,"rightY":0,"leftTrigger":0,"rightTrigger":0,"hat":0}'