#!/usr/bin/expect -f

#### Make an account in the accounts table with 255 access with a clear text password and enter it here
#### MUST enable telnet for this to work.
set user 
set pass 

set server 127.0.0.1
set port 9000
set command lock
#############################NOTHING BELOW THIS NEEDS TO BE EDITED!#############################
spawn telnet $server $port
expect "Username:"
send "$user\n"
expect "Password:"
send "$pass\n" 
expect "yourname>"
expect "yourname>"
send "$command\n"
expect "yourname>"
send "broadcast The server will be rebooting in 10 minutes. Please plan accordingly. The server is locked until reboot.\n"
sleep 55
send "broadcast The server will be rebooting in 9 minutes. Please plan accordingly. The server is locked until reboot.\n"
sleep 59
send "broadcast The server will be rebooting in 8 minutes. Please plan accordingly. The server is locked until reboot.\n"
sleep 59
send "broadcast The server will be rebooting in 7 minutes. Please plan accordingly. The server is locked until reboot.\n"
sleep 59
send "broadcast The server will be rebooting in 6 minutes. Please plan accordingly. The server is locked until reboot.\n"
sleep 59
send "broadcast The server will be rebooting in 5 minutes. Please plan accordingly. The server is locked until reboot.\n"
sleep 59
send "broadcast The server will be rebooting in 4 minutes. Please plan accordingly. The server is locked until reboot.\n"
sleep 59
send "broadcast The server will be rebooting in 3 minutes. Please plan accordingly. The server is locked until reboot.\n"
sleep 59
send "broadcast The server will be rebooting in 2 minutes. Please plan accordingly. The server is locked until reboot.\n"
sleep 59
send "broadcast The server will be rebooting in 1 minutes. Please plan accordingly. The server is locked until reboot.\n"
expect "yourname>"
send "exit\n"

sleep 59
