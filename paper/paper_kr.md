# 4-포트 크레딧 기반 셰이퍼(Credit-Based Shaper) TSN 스위치를 활용한 차량용 QoS 보장 구현 및 성능 평가

## 요약

최근 전기/전자(E/E) 아키텍처가 영역(존) 기반 구조로 진화함에 따라, 차량 내 네트워크는 고대역폭 멀티미디어 스트림과 중요 제어 데이터를 포함한 다양한 트래픽 유형의 신뢰성 있는 전송을 요구받고 있다. 이 논문은 이러한 요구에 대응하기 위해 Microchip LAN9692 4-포트 TSN 스위치를 사용하여 크레딧 기반 셰이퍼(Credit-Based Shaper, CBS)를 구현하고, 실제 차량 인포테인먼트 시나리오에서 성능을 평가하였다. 실험 결과, CBS를 적용한 경우 네트워크 과부하 상황에서도 영상 스트림의 프레임 손실률을 0.1% 미만으로 유지하며, CBS를 적용하지 않은 경우의 15% 이상과 대조적인 성능 향상을 보였다. 이는 CBS가 차량용 네트워크의 QoS 보장에 효과적인 메커니즘임을 입증한다.

## 1. 서론

### 1.1 연구 배경

자동차 산업은 자율주행과 커넥티드 카 기술의 발전으로 패러다임 전환을 겪고 있다. 전통적인 CAN, LIN 기반 네트워크는 대역폭 한계로 인해 고화질 카메라, 라이다, 레이더 등 대용량 데이터를 생성하는 센서를 지원하기 어렵다. 이에 따라 차량용 이더넷이 차세대 차량 내 네트워크의 백본으로 부상하고 있다.

특히 영역 기반 아키텍처로의 전환은 네트워크 토폴로지를 단순화하고 배선을 줄이는 동시에, 다양한 트래픽 유형이 동일한 네트워크를 공유하도록 한다. 이러한 환경에서는 시간 민감성 트래픽(영상 스트림, 제어 신호)과 베스트 에포트 트래픽(진단 데이터, 소프트웨어 업데이트)이 공존하므로, QoS 보장이 필수적이다.

### 1.2 연구 목적

본 연구의 목적은 다음과 같다:
- Microchip LAN9692 TSN 스위치에서 CBS 메커니즘 구현
- 실제 차량 인포테인먼트 시나리오에서 CBS 성능 검증
- CBS 파라미터 최적화 방법론 제시
- 영역 기반 아키텍처에서의 TSN 적용 가이드라인 제공

## 2. 관련 연구

### 2.1 TSN 표준

IEEE 802.1 TSN(Time-Sensitive Networking) 태스크 그룹은 이더넷 기반 시간 민감성 통신을 위한 표준을 개발하고 있다. 주요 표준은 다음과 같다:

- **IEEE 802.1Qav**: Credit-Based Shaper (CBS)
- **IEEE 802.1Qbv**: Time-Aware Shaper (TAS)
- **IEEE 802.1AS**: Timing and Synchronization
- **IEEE 802.1CB**: Frame Replication and Elimination

### 2.2 차량용 이더넷 적용 사례

BMW, Audi, Mercedes-Benz 등 주요 OEM은 이미 차량용 이더넷을 양산 차량에 적용하고 있다. 초기에는 진단과 인포테인먼트에 제한적으로 사용되었으나, 최근에는 ADAS와 자율주행 시스템의 백본 네트워크로 확대되고 있다.

## 3. 크레딧 기반 셰이퍼 이론

### 3.1 CBS 동작 원리

CBS는 각 트래픽 클래스에 크레딧을 할당하여 대역폭을 제어한다. 크레딧은 다음 규칙에 따라 변동한다:

**큐가 비어 있을 때:**
```
dCredit/dt = idleSlope
```

**프레임 전송 중:**
```
dCredit/dt = sendSlope = idleSlope - portSpeed
```

크레딧이 0 이상일 때만 프레임 전송이 허용되며, 이를 통해 각 트래픽 클래스의 대역폭을 보장한다.

### 3.2 파라미터 계산

CBS의 주요 파라미터는 다음과 같이 계산된다:

**Idle Slope:**
```
idleSlope = reservedBandwidth
```

**Send Slope:**
```
sendSlope = idleSlope - portSpeed
```

**Hi Credit:**
```
hiCredit = maxFrameSize × idleSlope / portSpeed
```

**Lo Credit:**
```
loCredit = maxFrameSize × |sendSlope| / portSpeed
```

## 4. 시스템 구현

### 4.1 하드웨어 구성

본 연구에서는 Microchip LAN9692 4-포트 TSN 스위치를 사용하였다. LAN9692의 주요 특징은:

- 4개의 10/100/1000 Mbps 포트
- IEEE 802.1Qav CBS 지원
- 8개 트래픽 클래스
- VLAN 및 PCP 기반 트래픽 분류
- IEEE 802.1AS 시간 동기화

### 4.2 소프트웨어 구현

LAN9692 CBS 구현의 핵심 코드는 다음과 같다:

```c
/* CBS 설정 구조체 */
typedef struct {
    uint32_t idle_slope;  /* bps */
    uint32_t send_slope;  /* bps */
    uint32_t hi_credit;   /* bytes */
    uint32_t lo_credit;   /* bytes */
    bool enabled;
} cbs_config_t;

/* CBS 구성 함수 */
int lan9692_cbs_configure_tc(uint8_t port, uint8_t tc, cbs_config_t *config) {
    uint32_t cbs_base = LAN9692_CBS_BASE(port);
    
    /* Idle/Send Slope 설정 */
    lan9692_write_reg(cbs_base + CBS_IDLE_SLOPE_REG, config->idle_slope);
    lan9692_write_reg(cbs_base + CBS_SEND_SLOPE_REG, config->send_slope);
    
    /* Credit 한계 설정 */
    lan9692_write_reg(cbs_base + CBS_HI_CREDIT_REG, config->hi_credit);
    lan9692_write_reg(cbs_base + CBS_LO_CREDIT_REG, config->lo_credit);
    
    return 0;
}
```

### 4.3 트래픽 분류 구성

영상 스트림을 우선순위별로 분류하기 위해 VLAN과 PCP 매핑을 구성:

```c
/* VLAN to TC 매핑 */
lan9692_set_vlan_tc_mapping(100, TC_VIDEO_STREAM_1);  /* VLAN 100 → TC7 */
lan9692_set_vlan_tc_mapping(101, TC_VIDEO_STREAM_2);  /* VLAN 101 → TC6 */

/* PCP to TC 매핑 */
lan9692_set_pcp_tc_mapping(7, TC_VIDEO_STREAM_1);     /* PCP 7 → TC7 */
lan9692_set_pcp_tc_mapping(6, TC_VIDEO_STREAM_2);     /* PCP 6 → TC6 */
lan9692_set_pcp_tc_mapping(0, TC_BEST_EFFORT);        /* PCP 0 → TC0 */
```

## 5. 실험 설계

### 5.1 실험 환경 구성

실험 환경은 다음과 같이 구성하였다:

- **포트 1**: 영상 소스 (2개의 H.264 스트림, 각 15Mbps)
- **포트 2**: 영상 수신기 1 (스트림 1 수신)
- **포트 3**: 영상 수신기 2 (스트림 2 수신)
- **포트 4**: BE 트래픽 생성기 (500-800Mbps UDP)

### 5.2 테스트 시나리오

두 가지 시나리오로 성능을 비교:

**시나리오 1: CBS 비활성화**
- 모든 트래픽을 FIFO로 처리
- 네트워크 혼잡 시 패킷 손실 발생

**시나리오 2: CBS 활성화**
- 영상 스트림에 각 20Mbps 대역폭 예약
- 크레딧 기반 우선순위 제어

### 5.3 성능 지표

다음 지표를 측정하여 CBS 효과를 정량화:
- 평균 처리량 (Mbps)
- 프레임 손실률 (%)
- 지연 시간 (ms)
- 지터 (ms)
- 영상 재생 품질 (MOS)

## 6. 실험 결과

### 6.1 처리량 분석

CBS 활성화 시 영상 스트림의 처리량이 안정적으로 유지됨:

| 트래픽 유형 | CBS 비활성화 | CBS 활성화 |
|------------|------------|-----------|
| 영상 스트림 1 | 8.2 Mbps | 14.8 Mbps |
| 영상 스트림 2 | 7.9 Mbps | 14.7 Mbps |
| BE 트래픽 | 784 Mbps | 720 Mbps |

### 6.2 프레임 손실률

CBS는 네트워크 혼잡 상황에서도 낮은 프레임 손실률을 유지:

| 시간 (초) | CBS 비활성화 (%) | CBS 활성화 (%) |
|----------|----------------|---------------|
| 0-10 | 0.2 | 0.0 |
| 10-20 | 12.5 | 0.1 |
| 20-30 | 18.3 | 0.1 |
| 30-40 | 15.7 | 0.0 |
| 40-50 | 16.2 | 0.1 |

### 6.3 지연 시간 및 지터

CBS는 일관된 지연 시간과 낮은 지터를 제공:

| 메트릭 | CBS 비활성화 | CBS 활성화 |
|--------|------------|-----------|
| 평균 지연 | 45.2 ms | 2.3 ms |
| 최대 지연 | 312.5 ms | 4.1 ms |
| 지터 (표준편차) | 62.3 ms | 0.8 ms |

### 6.4 영상 품질 평가

주관적 영상 품질 평가 (MOS 척도):

| 평가 항목 | CBS 비활성화 | CBS 활성화 |
|----------|------------|-----------|
| 화질 | 2.1 | 4.8 |
| 끊김 없음 | 1.8 | 4.9 |
| 동기화 | 2.3 | 4.7 |
| 전체 만족도 | 2.1 | 4.8 |

## 7. 논의

### 7.1 CBS 효과 분석

실험 결과는 CBS가 차량용 네트워크에서 QoS를 효과적으로 보장함을 보여준다:

1. **대역폭 보장**: CBS는 예약된 대역폭을 안정적으로 제공
2. **낮은 손실률**: 혼잡 상황에서도 0.1% 미만의 프레임 손실
3. **예측 가능한 지연**: 일관된 지연 시간으로 실시간 응용 지원
4. **공정한 자원 분배**: BE 트래픽도 남은 대역폭 활용 가능

### 7.2 파라미터 최적화

CBS 파라미터 설정 시 고려사항:

1. **Idle Slope**: 필요 대역폭보다 10-20% 여유 확보
2. **Credit 한계**: 최대 프레임 크기 기반 계산
3. **트래픽 클래스 할당**: 응용별 우선순위 고려

### 7.3 실제 적용 시 고려사항

1. **네트워크 토폴로지**: 스위치 위치와 연결 구조 최적화
2. **트래픽 패턴**: 버스트 트래픽 고려한 파라미터 조정
3. **확장성**: 포트 수 증가 시 대역폭 재할당 필요
4. **다른 TSN 기능과의 통합**: TAS, Frame Preemption 등과 조합

## 8. 결론

본 연구는 Microchip LAN9692 TSN 스위치를 사용하여 CBS를 구현하고, 차량용 인포테인먼트 시나리오에서 성능을 검증하였다. 주요 성과는 다음과 같다:

1. **실용적 구현**: LAN9692에서 CBS 구현 및 최적화
2. **성능 검증**: 프레임 손실률 0.1% 미만 달성
3. **가이드라인 제시**: CBS 파라미터 설정 방법론 제공
4. **실제 적용 가능성**: 차량 네트워크에 즉시 적용 가능

향후 연구 방향:
- 더 복잡한 네트워크 토폴로지에서의 성능 평가
- TAS와 CBS 조합 최적화
- 머신러닝 기반 동적 파라미터 조정
- 실차 환경에서의 장기 신뢰성 테스트

## 참고문헌

[1] IEEE 802.1Qav-2009, "IEEE Standard for Local and Metropolitan Area Networks - Virtual Bridged Local Area Networks Amendment 12: Forwarding and Queuing Enhancements for Time-Sensitive Streams," IEEE Standards Association, 2009.

[2] Microchip Technology Inc., "LAN9692 - 4-Port TSN Gigabit Ethernet Switch," Datasheet DS00004012A, 2023.

[3] M. Ashjaei et al., "Time-Sensitive Networking in Automotive Embedded Systems: State of the Art and Research Opportunities," Journal of Systems Architecture, vol. 117, 2021.

[4] S. Kehrer et al., "Automotive Ethernet: In-Vehicle Networking and Smart Mobility," 2018 Design, Automation & Test in Europe Conference, pp. 1735-1736, 2018.

[5] P. Meyer et al., "Automotive Ethernet: In-vehicle networking and smart factory," IEEE Communications Standards Magazine, vol. 5, no. 1, pp. 102-108, 2021.

[6] T. Steinbach et al., "Tomorrow's In-Car Interconnect? A Competitive Evaluation of IEEE 802.1 AVB and Time-Triggered Ethernet," IEEE Vehicular Technology Conference, 2012.

[7] J. Park et al., "Implementation and Performance Analysis of TSN-based In-Vehicle Network for Autonomous Vehicles," IEEE Access, vol. 10, pp. 45678-45691, 2022.

[8] K. Lee et al., "Credit-Based Shaper Performance Evaluation for Mixed-Criticality Traffic in Automotive TSN," KICS Summer Conference, 2023.