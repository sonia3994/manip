laserumbilical stop
 sticky
 tension 25
laserumbilical wait
laserumbilical tension 25
%1 disconnect laserumbilical
%1 to 0 0 1415
eastrope stop
eastrope sticky
eastrope tension 90
westrope stop
westrope sticky
westrope tension 90
centralrope maxSpeed .5
centralrope stop
centralrope sticky
centralrope tension 70
eastrope wait
westrope wait
wait 30
eastrope sticky
eastrope tension 50
westrope sticky
westrope tension 50
centralrope wait
eastrope wait
westrope wait
wait 1
laserumbilical stop
%1 connect laserumbilical
%1 locate 0 13.75 1429.39
eastrope minlength
westrope minlength
centralrope maxSpeed 3
%1 to 0 0 1385
eastrope tension 10
westrope tension 10
%1 wait
say >>> Reconnect side ropes then type "donecal %1"
