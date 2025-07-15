
sudo iptables -A INPUT -p tcp --dport 5060 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 5060 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 10001:11000 -j ACCEPT

#sudo apt install -y iptables-persistent
sudo sh -c "iptables-save > /etc/iptables/rules.v4"

sudo systemctl start sip-receiver
sudo systemctl status sip-receiver

chmod +x kompiluj.sh
compile:
./compile.sh

start:
LD_LIBRARY_PATH=/usr/lib/arm-linux-gnueabihf ./sip_receiver
