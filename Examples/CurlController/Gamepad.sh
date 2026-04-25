#!/bin/bash

curl -X POST "http://$HRKLTZ_OPENCTRL_CORE_IP/gamepad" \
     -H "Content-Type: application/json" \
     -d '{"x":0,"y":0,"z":0,"rx":0,"ry":0,"rz":0,"hat":0,"buttons":0}'