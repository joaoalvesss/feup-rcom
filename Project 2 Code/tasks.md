## TUX 2
#### switch
- $ ifconfig eth0 up
- $ ifconfig eth0 172.16.Y1.1/24 
- *>* /system reset-configuration
- *>* /interface bridge add name=bridgeY0
- *>* /interface bridge add name=bridgeY1
- *>* /interface bridge port remove [find interface=etherN1]
- *>* /interface bridge port remove [find interface=etherN2]
- *>* /interface bridge port remove [find interface=etherN3]
- *>* /interface bridge port add bridge=bridgeY0 interface=etherN1
- *>* /interface bridge port add bridge=bridgeY0 interface=etherN2
- *>* /interface bridge port add bridge=bridgeY1 interface=etherN3
- *>* /interface bridge port remove [find interface=etherN4]
- *>* /interface bridge port add bridge=bridgeY1 interface=etherN4
- $ route add -net 172.16.Y0.0/24 gw 172.16.Y1.253
- $ arp -d *ip_address*
- *>* /interface bridge port remove [find interface=etherN5]
- *>* /interface bridge port add bridge=bridge51 interface=etherN5

#### router
- *>* /system reset-configuration
- *>* /ipaddress add address=172.16.1.Y9/24 interface=ether1
- *>* /ip address add address=172.16.Y1.254/24 interface=ether2
- $ route add default gw 172.16.51.254
- *>* /ip route add dst-address=172.16.Y0.0/24 gateway=172.16.Y1.253  
- *>* /ip route add dst-address=0.0.0.0/0 gateway=172.16.Y1.254    
- /ip firewall nat enable 0

#### task 5 and 6
- add " *nameserver 172.16.1.1* " -> I321 in file /etc/resolv.conf (172.16.2.1 for I320)
- ping google.com in all tuxs
- compile and run *download.c* in tux3

## TUX 3
- $ ifconfig eth0 up 
- $ ifconfig eth0 172.16.Y0.1/24 
- $ arp -a
- $ arp -d *ip_address*
- $ route add -net 172.16.Y1.0/24 gw 172.16.50.254
- $ arp -d *ip_address*
- $ route add default gw 172.16.50.254
 

## TUX 4
- $ ifconfig eth0 up 
- $ ifconfig eth0 172.16.Y0.254/24 
- $ ifconfig eth1 up 
- $ ifconfig eth1 172.16.Y1.253/24 
- $ sysctlnet.ipv4.ip_forward=1
- $ sysctlnet.ipv4.icmp_echo_ignore_broadcasts=0
- $ arp -d *ip_address*
- $ route add default gw 172.16.51.254
