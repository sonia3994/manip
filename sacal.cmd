%2 stop
 sticky
 tension 40
%2 wait
%2 tension 40
%1 disconnect %2
%1 to 0 0 1370
%3 maxSpeed .5
%3 stop
%3 sticky
%3 tension 100
%3 wait
wait 1
%2 stop
%1 connect %2
%1 locate 0.0 0.0 1400.0
%3 maxSpeed 3
%1 to 0 0 1370
%1 wait
say >>> Reconnect side ropes then type "donecal %1"
