#!/bin/bash

# CBS Test Script for LAN9692
# This script runs comprehensive CBS tests and collects performance data

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RESULTS_DIR="$SCRIPT_DIR/results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Create results directory
mkdir -p "$RESULTS_DIR"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}LAN9692 CBS Performance Test Suite${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Function to run a test scenario
run_scenario() {
    local scenario=$1
    local description=$2
    local duration=$3
    
    echo -e "${YELLOW}Running Scenario $scenario: $description${NC}"
    echo "Duration: ${duration}s"
    echo "----------------------------------------"
    
    # Start CBS application
    sudo ../implementation/lan9692_cbs_test $scenario &
    CBS_PID=$!
    
    # Start video streams on Port 1
    echo "Starting video streams..."
    ffmpeg -re -i /dev/video0 -c:v h264 -b:v 15M -f mpegts udp://192.168.1.2:5000?pkt_size=1316 &
    STREAM1_PID=$!
    
    ffmpeg -re -i /dev/video1 -c:v h264 -b:v 15M -f mpegts udp://192.168.1.3:5001?pkt_size=1316 &
    STREAM2_PID=$!
    
    # Start BE traffic generator on Port 4
    echo "Starting BE traffic generator..."
    iperf3 -c 192.168.1.2 -u -b 800M -t $duration -p 5201 &
    BE_PID=$!
    
    # Monitor and collect statistics
    echo "Collecting statistics..."
    for ((i=0; i<$duration; i++)); do
        # Capture switch statistics
        echo "Time: $i seconds"
        
        # Get port statistics
        for port in 0 1 2 3; do
            echo "Port $port stats:" >> "$RESULTS_DIR/scenario${scenario}_stats_${TIMESTAMP}.log"
            cat /sys/class/net/eth${port}/statistics/rx_packets >> "$RESULTS_DIR/scenario${scenario}_stats_${TIMESTAMP}.log"
            cat /sys/class/net/eth${port}/statistics/tx_packets >> "$RESULTS_DIR/scenario${scenario}_stats_${TIMESTAMP}.log"
            cat /sys/class/net/eth${port}/statistics/rx_dropped >> "$RESULTS_DIR/scenario${scenario}_stats_${TIMESTAMP}.log"
            cat /sys/class/net/eth${port}/statistics/tx_dropped >> "$RESULTS_DIR/scenario${scenario}_stats_${TIMESTAMP}.log"
        done
        
        sleep 1
    done
    
    # Stop all processes
    echo "Stopping test processes..."
    kill $CBS_PID $STREAM1_PID $STREAM2_PID $BE_PID 2>/dev/null
    wait $CBS_PID $STREAM1_PID $STREAM2_PID $BE_PID 2>/dev/null
    
    echo -e "${GREEN}Scenario $scenario completed${NC}"
    echo ""
}

# Function to analyze results
analyze_results() {
    echo -e "${YELLOW}Analyzing test results...${NC}"
    python3 analyze_results.py "$RESULTS_DIR" "$TIMESTAMP"
}

# Main test execution
main() {
    # Check if running as root
    if [ "$EUID" -ne 0 ]; then 
        echo -e "${RED}Please run as root (sudo)${NC}"
        exit 1
    fi
    
    # Build the CBS test application if needed
    if [ ! -f "../implementation/lan9692_cbs_test" ]; then
        echo "Building CBS test application..."
        cd ../implementation
        make clean && make
        cd "$SCRIPT_DIR"
    fi
    
    # Run test scenarios
    echo -e "${GREEN}Starting test scenarios...${NC}"
    echo ""
    
    # Scenario 1: CBS Disabled (baseline)
    run_scenario 1 "CBS Disabled - Baseline" 60
    sleep 10
    
    # Scenario 2: CBS Enabled with standard reservation
    run_scenario 2 "CBS Enabled - 20Mbps per stream" 60
    sleep 10
    
    # Scenario 3: CBS Enabled with higher reservation
    run_scenario 3 "CBS Enabled - 30Mbps per stream" 60
    
    # Analyze and generate reports
    analyze_results
    
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}All tests completed successfully!${NC}"
    echo -e "${GREEN}Results saved in: $RESULTS_DIR${NC}"
    echo -e "${GREEN}========================================${NC}"
}

# Run main function
main "$@"