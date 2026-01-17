#!/bin/bash

curl -X POST "http://$HRKLTZ_OPENCTRL_CORE_IP/keyboard" \
     -H "Content-Type: application/json" \
     -d '{"modifiers":2,"keys":[4,5,0,0,0,0]}'