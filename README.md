# TSN Credit-Based Shaper (CBS) Implementation and Performance Evaluation

## Project Overview
Implementation and performance evaluation of a 4-port Credit-Based Shaper TSN switch for QoS provisioning in Automotive Ethernet using Microchip LAN9692.

## Contents

- **Paper**: Full research paper in Korean and English
- **Implementation**: CBS implementation code for LAN9692
- **Experiments**: Test setup and results
- **Documentation**: Technical documentation and guides

## Key Features

- Credit-Based Shaper (CBS) implementation for TSN
- 4-port automotive Ethernet switch configuration
- QoS guarantee for time-sensitive traffic
- Performance evaluation with video streaming

## Hardware

- Microchip LAN9692 4-port TSN Switch
- Evaluation board with MDIO interface
- Test PCs for traffic generation and monitoring

## Software Requirements

- Linux-based development environment
- Microchip LAN9692 SDK
- FFmpeg for video streaming
- iperf3 for traffic generation
- Wireshark for packet analysis

## Quick Start

```bash
# Clone repository
git clone https://github.com/hwkim3330/0903

# Build CBS implementation
cd implementation
make

# Run experiments
cd ../experiments
./run_tests.sh
```

## Documentation

- [Installation Guide](docs/installation.md)
- [CBS Configuration](docs/cbs_config.md)
- [Test Procedures](docs/testing.md)
- [API Reference](docs/api.md)

## Results

Performance comparison between CBS-enabled and disabled scenarios:
- **Throughput**: Guaranteed bandwidth for video streams
- **Frame Loss**: <0.1% with CBS vs >15% without CBS
- **Video Quality**: Smooth playback with CBS enabled

## Paper

The full research paper is available in:
- [Korean Version](paper/paper_kr.md)
- [HTML Presentation](https://hwkim3330.github.io/0903)

## License

MIT License

## Contact

For questions and support, please open an issue on GitHub.