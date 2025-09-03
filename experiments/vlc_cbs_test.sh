#!/bin/bash

# EVB-LAN9692 VLC CBS 테스트 스크립트
# VLC를 이용한 영상 전송 + CBS 성능 검증

set -euo pipefail

# 색상 정의
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 테스트 환경 설정
VIDEO_PATH="/home/kim/Downloads/243-135867787.mp4"
RESULT_DIR="results/$(date +%Y%m%d_%H%M%S)"
BOARD_TTY="/dev/ttyACM0"

# 네트워크 설정
DEV_SND=${DEV_SND:-enp9s0}
VLAN_ID=100
SRC_IP="10.0.100.1"
DST_IP_PC1="10.0.100.2"
DST_IP_PC2="10.0.100.3"

# VLC 스트리밍 포트
PORT_4K=5005
PORT_FHD=5006
PORT_VOD=5007

mkdir -p "$RESULT_DIR"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}   EVB-LAN9692 CBS VLC Test Suite${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# 1. VLAN 인터페이스 설정 (송신측)
setup_sender_vlan() {
    echo -e "${YELLOW}[1] 송신측 VLAN 설정${NC}"
    
    # 기존 설정 정리
    sudo ip link del r100 2>/dev/null || true
    sudo ip link del r101 2>/dev/null || true
    
    # VLAN 100 인터페이스 생성
    sudo ip link add link "$DEV_SND" name r100 type vlan id 100
    sudo ip addr add "$SRC_IP/24" dev r100
    sudo ip link set r100 up
    
    # VLAN egress QoS 매핑 (skb prio -> PCP)
    sudo ip link set dev r100 type vlan egress-qos-map \
        0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7
    
    # 라우팅 설정
    sudo ip route add "$DST_IP_PC1/32" dev r100 src "$SRC_IP"
    sudo ip route add "$DST_IP_PC2/32" dev r100 src "$SRC_IP"
    
    # tc 필터 설정 (포트별 우선순위)
    sudo tc qdisc replace dev r100 clsact
    
    # 4K 비디오 (포트 5005) -> TC7 (PCP 7)
    sudo tc filter add dev r100 egress protocol ip prio 10 u32 \
        match ip dport $PORT_4K 0xffff \
        action skbedit priority 7
    
    # FHD 비디오 (포트 5006) -> TC6 (PCP 6)
    sudo tc filter add dev r100 egress protocol ip prio 20 u32 \
        match ip dport $PORT_FHD 0xffff \
        action skbedit priority 6
    
    # VOD (포트 5007) -> TC5 (PCP 5)
    sudo tc filter add dev r100 egress protocol ip prio 30 u32 \
        match ip dport $PORT_VOD 0xffff \
        action skbedit priority 5
    
    echo -e "${GREEN}송신측 VLAN 설정 완료${NC}"
    ip -d link show r100 | head -10
}

# 2. 수신측 설정 스크립트 생성
generate_receiver_script() {
    cat > "$RESULT_DIR/receiver_pc1.sh" << 'EOF'
#!/bin/bash
# PC1 수신측 스크립트 (10.0.100.2)

DEV_RCV=${1:-enp8s0}
VLAN_ID=100
IP_ADDR="10.0.100.2/24"

# VLAN 설정
sudo ip link add link "$DEV_RCV" name r100 type vlan id $VLAN_ID
sudo ip addr add "$IP_ADDR" dev r100
sudo ip link set r100 up

# ingress QoS 매핑 (PCP -> skb prio)
sudo ip link set dev r100 type vlan ingress-qos-map \
    0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7

# VLC 수신 (4K 스트림)
echo "Receiving 4K stream on port 5005..."
cvlc udp://@:5005 \
    --network-caching=200 \
    --drop-late-frames \
    --skip-frames \
    --sout '#display' &

# VLC 수신 (FHD 스트림)
echo "Receiving FHD stream on port 5006..."
cvlc udp://@:5006 \
    --network-caching=200 \
    --drop-late-frames \
    --skip-frames \
    --sout '#display' &

# 패킷 캡처 (PCP 확인용)
sudo tcpdump -i r100 -e -vv vlan -w capture_pc1.pcap &

echo "PC1 receiver ready. Press Ctrl+C to stop."
wait
EOF

    cat > "$RESULT_DIR/receiver_pc2.sh" << 'EOF'
#!/bin/bash
# PC2 수신측 스크립트 (10.0.100.3)

DEV_RCV=${1:-enp8s0}
VLAN_ID=100
IP_ADDR="10.0.100.3/24"

# VLAN 설정
sudo ip link add link "$DEV_RCV" name r100 type vlan id $VLAN_ID
sudo ip addr add "$IP_ADDR" dev r100
sudo ip link set r100 up

# ingress QoS 매핑
sudo ip link set dev r100 type vlan ingress-qos-map \
    0:0 1:1 2:2 3:3 4:4 5:5 6:6 7:7

# VLC 수신 (VOD 스트림)
echo "Receiving VOD stream on port 5007..."
cvlc udp://@:5007 \
    --network-caching=200 \
    --sout '#display' &

# 패킷 캡처
sudo tcpdump -i r100 -e -vv vlan -w capture_pc2.pcap &

echo "PC2 receiver ready. Press Ctrl+C to stop."
wait
EOF

    chmod +x "$RESULT_DIR/receiver_pc1.sh"
    chmod +x "$RESULT_DIR/receiver_pc2.sh"
    
    echo -e "${GREEN}수신 스크립트 생성 완료${NC}"
}

# 3. 보드 CBS 설정
configure_board_cbs() {
    local mode=$1
    
    echo -e "${YELLOW}[3] 보드 CBS 설정: $mode${NC}"
    
    cd implementation
    
    if [[ "$mode" == "enable" ]]; then
        sudo ./evb_lan9692_cbs enable
    else
        sudo ./evb_lan9692_cbs disable
    fi
    
    cd ..
    
    echo -e "${GREEN}보드 설정 완료${NC}"
}

# 4. VLC 스트리밍 시작
start_vlc_streaming() {
    echo -e "${YELLOW}[4] VLC 스트리밍 시작${NC}"
    
    # 기존 VLC 프로세스 정리
    pkill -f cvlc 2>/dev/null || true
    sleep 2
    
    # 4K 스트림 (25Mbps) -> PC1, PC2
    echo "Starting 4K stream (25Mbps)..."
    cvlc --loop "$VIDEO_PATH" \
        --sout "#transcode{vcodec=h264,vb=25000,acodec=mp4a,ab=256}:\
duplicate{dst=std{access=udp{ttl=16,mtu=1400},mux=ts,dst=$DST_IP_PC1:$PORT_4K},\
dst=std{access=udp{ttl=16,mtu=1400},mux=ts,dst=$DST_IP_PC2:$PORT_4K}}" \
        --sout-keep \
        --network-caching=100 \
        -vvv 2>&1 | tee "$RESULT_DIR/vlc_4k.log" &
    
    sleep 2
    
    # FHD 스트림 (8Mbps) -> PC1
    echo "Starting FHD stream (8Mbps)..."
    cvlc --loop "$VIDEO_PATH" \
        --sout "#transcode{vcodec=h264,vb=8000,scale=0.5,acodec=mp4a,ab=128}:\
std{access=udp{ttl=16,mtu=1400},mux=ts,dst=$DST_IP_PC1:$PORT_FHD}" \
        --sout-keep \
        --network-caching=100 \
        -vvv 2>&1 | tee "$RESULT_DIR/vlc_fhd.log" &
    
    sleep 2
    
    # VOD 스트림 (4Mbps) -> PC2
    echo "Starting VOD stream (4Mbps)..."
    cvlc --loop "$VIDEO_PATH" \
        --sout "#transcode{vcodec=h264,vb=4000,scale=0.5,acodec=mp4a,ab=128}:\
std{access=udp{ttl=16,mtu=1400},mux=ts,dst=$DST_IP_PC2:$PORT_VOD}" \
        --sout-keep \
        --network-caching=100 \
        -vvv 2>&1 | tee "$RESULT_DIR/vlc_vod.log" &
    
    echo -e "${GREEN}모든 스트림 시작됨${NC}"
}

# 5. iperf3 배경 트래픽 생성
generate_background_traffic() {
    local bandwidth=$1
    
    echo -e "${YELLOW}[5] 배경 트래픽 생성: ${bandwidth}Mbps${NC}"
    
    # iperf3 서버는 수신측에서 미리 실행되어 있어야 함
    iperf3 -u -c "$DST_IP_PC1" -B "$SRC_IP" \
        -b "${bandwidth}M" -t 60 -p 5201 \
        -J --logfile "$RESULT_DIR/iperf_${bandwidth}M.json" &
    
    echo $! > "$RESULT_DIR/iperf.pid"
}

# 6. 모니터링 및 통계 수집
monitor_performance() {
    local duration=$1
    
    echo -e "${YELLOW}[6] 성능 모니터링 (${duration}초)${NC}"
    
    # 네트워크 통계 수집 스크립트
    (
        for ((i=0; i<duration; i+=5)); do
            echo "=== Time: $i seconds ===" >> "$RESULT_DIR/network_stats.txt"
            
            # 인터페이스 통계
            ip -s link show r100 >> "$RESULT_DIR/network_stats.txt"
            
            # tc 필터 통계
            sudo tc -s filter show dev r100 egress >> "$RESULT_DIR/network_stats.txt"
            
            # 보드 통계
            sudo dr mup1cc -d "$BOARD_TTY" -m get -i implementation/fetch_stats.yaml \
                >> "$RESULT_DIR/board_stats.txt" 2>&1
            
            sleep 5
        done
    ) &
    
    MONITOR_PID=$!
    
    # 대기
    sleep "$duration"
    
    # 모니터링 중지
    kill $MONITOR_PID 2>/dev/null || true
}

# 7. 테스트 시나리오 실행
run_test_scenario() {
    local scenario=$1
    
    echo -e "${BLUE}=== 시나리오 $scenario 시작 ===${NC}"
    
    case $scenario in
        1)
            echo "시나리오 1: CBS 비활성화 (기준선)"
            configure_board_cbs disable
            start_vlc_streaming
            sleep 10
            generate_background_traffic 800
            monitor_performance 60
            ;;
        
        2)
            echo "시나리오 2: CBS 활성화"
            configure_board_cbs enable
            start_vlc_streaming
            sleep 10
            generate_background_traffic 800
            monitor_performance 60
            ;;
        
        3)
            echo "시나리오 3: 동적 부하 테스트"
            configure_board_cbs enable
            start_vlc_streaming
            
            for load in 200 400 600 800 900; do
                echo -e "${YELLOW}부하: ${load}Mbps${NC}"
                generate_background_traffic $load
                monitor_performance 30
                kill $(cat "$RESULT_DIR/iperf.pid") 2>/dev/null || true
                sleep 5
            done
            ;;
        
        *)
            echo -e "${RED}Unknown scenario: $scenario${NC}"
            return 1
            ;;
    esac
    
    # 정리
    pkill -f cvlc 2>/dev/null || true
    kill $(cat "$RESULT_DIR/iperf.pid") 2>/dev/null || true
    
    echo -e "${GREEN}시나리오 $scenario 완료${NC}"
}

# 8. 결과 분석
analyze_results() {
    echo -e "${YELLOW}[7] 결과 분석${NC}"
    
    # VLC 로그에서 드롭 프레임 확인
    echo "=== Frame Drop Analysis ===" > "$RESULT_DIR/analysis.txt"
    grep -i "drop\|lost" "$RESULT_DIR"/vlc_*.log >> "$RESULT_DIR/analysis.txt" || true
    
    # iperf3 결과 분석
    if [ -f "$RESULT_DIR/iperf_800M.json" ]; then
        echo "=== iperf3 Results ===" >> "$RESULT_DIR/analysis.txt"
        jq '.end.sum_received' "$RESULT_DIR/iperf_800M.json" >> "$RESULT_DIR/analysis.txt" || true
    fi
    
    # 간단한 요약 출력
    echo -e "${GREEN}=== 테스트 요약 ===${NC}"
    echo "결과 디렉토리: $RESULT_DIR"
    echo "VLC 로그: $(ls -lh "$RESULT_DIR"/vlc_*.log 2>/dev/null | wc -l)개"
    echo "네트워크 통계: $(wc -l < "$RESULT_DIR/network_stats.txt" 2>/dev/null || echo 0)줄"
    echo "보드 통계: $(wc -l < "$RESULT_DIR/board_stats.txt" 2>/dev/null || echo 0)줄"
}

# 메인 실행
main() {
    # 환경 확인
    if [ ! -f "$VIDEO_PATH" ]; then
        echo -e "${RED}비디오 파일이 없습니다: $VIDEO_PATH${NC}"
        exit 1
    fi
    
    if [ ! -c "$BOARD_TTY" ]; then
        echo -e "${RED}보드 연결을 확인하세요: $BOARD_TTY${NC}"
        exit 1
    fi
    
    # CBS 실행 파일 빌드
    if [ ! -f "implementation/evb_lan9692_cbs" ]; then
        echo "Building CBS tool..."
        cd implementation
        gcc -o evb_lan9692_cbs evb_lan9692_cbs.c
        cd ..
    fi
    
    # 설정
    setup_sender_vlan
    generate_receiver_script
    
    # 테스트 실행
    echo -e "${BLUE}테스트를 시작합니다${NC}"
    echo "수신 PC에서 다음 스크립트를 실행하세요:"
    echo "  PC1: $RESULT_DIR/receiver_pc1.sh"
    echo "  PC2: $RESULT_DIR/receiver_pc2.sh"
    echo ""
    read -p "수신 준비가 완료되면 Enter를 누르세요..."
    
    # 시나리오 실행
    run_test_scenario 1
    sleep 30
    
    run_test_scenario 2
    sleep 30
    
    run_test_scenario 3
    
    # 결과 분석
    analyze_results
    
    echo -e "${GREEN}========================================${NC}"
    echo -e "${GREEN}   테스트 완료!${NC}"
    echo -e "${GREEN}   결과: $RESULT_DIR${NC}"
    echo -e "${GREEN}========================================${NC}"
}

# 실행
main "$@"