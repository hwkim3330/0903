# Installation Guide

## Prerequisites

### Hardware Requirements
- Microchip LAN9692 4-port TSN Switch
- Evaluation board with MDIO interface
- 4 Linux-based test PCs with Gigabit Ethernet
- Ethernet cables (Cat 5e or better)

### Software Requirements
- Linux kernel 5.4 or later
- GCC compiler (version 7.0+)
- Python 3.8+
- FFmpeg 4.0+
- iperf3
- Wireshark (optional, for packet analysis)

## Installation Steps

### 1. Clone the Repository

```bash
git clone https://github.com/hwkim3330/0903
cd 0903
```

### 2. Install Dependencies

#### Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y build-essential python3-pip ffmpeg iperf3
pip3 install numpy matplotlib pandas
```

#### CentOS/RHEL:
```bash
sudo yum groupinstall "Development Tools"
sudo yum install python3-pip ffmpeg iperf3
pip3 install numpy matplotlib pandas
```

### 3. Build CBS Implementation

```bash
cd implementation
make clean
make
sudo make install
```

### 4. Configure Network Interfaces

Edit `/etc/network/interfaces` or use NetworkManager:

```bash
# Port 1 - Video Source
sudo ip addr add 192.168.1.1/24 dev eth0
sudo ip link set eth0 up

# Port 2 - Video Receiver 1
sudo ip addr add 192.168.1.2/24 dev eth1
sudo ip link set eth1 up

# Port 3 - Video Receiver 2
sudo ip addr add 192.168.1.3/24 dev eth2
sudo ip link set eth2 up

# Port 4 - BE Traffic Generator
sudo ip addr add 192.168.1.4/24 dev eth3
sudo ip link set eth3 up
```

### 5. Configure VLAN (Optional)

```bash
# Create VLAN interfaces
sudo ip link add link eth0 name eth0.100 type vlan id 100
sudo ip link add link eth0 name eth0.101 type vlan id 101

# Assign IP addresses
sudo ip addr add 192.168.100.1/24 dev eth0.100
sudo ip addr add 192.168.101.1/24 dev eth0.101

# Bring up interfaces
sudo ip link set eth0.100 up
sudo ip link set eth0.101 up
```

### 6. Verify Installation

```bash
# Check CBS application
lan9692_cbs_test -h

# Test network connectivity
ping 192.168.1.2
ping 192.168.1.3
ping 192.168.1.4

# Check FFmpeg
ffmpeg -version

# Check iperf3
iperf3 -v
```

## Hardware Setup

### Physical Connections

1. Connect LAN9692 evaluation board to power supply
2. Connect MDIO interface to control PC
3. Wire the network as follows:
   - PC1 eth0 → LAN9692 Port 1
   - PC2 eth0 → LAN9692 Port 2
   - PC3 eth0 → LAN9692 Port 3
   - PC4 eth0 → LAN9692 Port 4

### LED Indicators

- Green: Link established
- Yellow: Activity detected
- Red: Error condition

## Troubleshooting

### Issue: Cannot access /dev/mem

```bash
# Run with sudo or add user to appropriate group
sudo usermod -a -G kmem $USER
```

### Issue: MDIO communication failure

```bash
# Check kernel module
lsmod | grep mdio
# Load if necessary
sudo modprobe mdio-gpio
```

### Issue: High CPU usage

```bash
# Enable hardware offloading
sudo ethtool -K eth0 gso on tso on gro on
```

## Next Steps

- Review [CBS Configuration Guide](cbs_config.md)
- Run test scenarios using [Test Procedures](testing.md)
- Check [API Reference](api.md) for custom development