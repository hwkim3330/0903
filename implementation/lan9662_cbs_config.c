/**
 * LAN9662 TSN Switch CBS Configuration
 * Microchip LAN9662 64-Port Gigabit Ethernet Switch
 * 실제 하드웨어 기반 구현
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

/* LAN9662 Register Map */
#define LAN9662_BASE_ADDR           0x70000000
#define LAN9662_REG_SIZE            0x10000000

/* CBS Registers - Per Port Configuration */
#define QSYS_CBS_PORT(p)            (0x0C000 + ((p) * 0x100))
#define QSYS_CBS_CIR(p,q)           (QSYS_CBS_PORT(p) + 0x00 + ((q) * 0x10))
#define QSYS_CBS_EIR(p,q)           (QSYS_CBS_PORT(p) + 0x04 + ((q) * 0x10))
#define QSYS_CBS_CBS(p,q)           (QSYS_CBS_PORT(p) + 0x08 + ((q) * 0x10))
#define QSYS_CBS_EBS(p,q)           (QSYS_CBS_PORT(p) + 0x0C + ((q) * 0x10))

/* Port Configuration */
#define DEVCPU_GCB_CHIP_MODE        0x71070000
#define DEVCPU_GCB_PORT_MODE(p)     (0x71070100 + ((p) * 0x4))

/* Queue System */
#define QSYS_QMAP                   0x0C110000
#define QSYS_QMAP_SE_BASE(se)       (QSYS_QMAP + ((se) * 0x4))

/* LAN9662 특성 */
#define LAN9662_NUM_PORTS           64
#define LAN9662_NUM_QUEUES          8
#define LAN9662_PORT_SPEED_1G       1000000000
#define LAN9662_PORT_SPEED_100M     100000000
#define LAN9662_PORT_SPEED_10M      10000000
#define LAN9662_MAX_FRAME_SIZE      9600  /* Jumbo Frame Support */

/* VOD/Live Streaming Traffic Classes */
typedef enum {
    TC_LIVE_4K_VIDEO = 7,      /* 실시간 4K 영상 */
    TC_LIVE_FHD_VIDEO = 6,     /* 실시간 FHD 영상 */
    TC_VOD_STREAMING = 5,      /* VOD 스트리밍 */
    TC_AUDIO_STREAM = 4,       /* 오디오 스트림 */
    TC_CONTROL_DATA = 3,       /* 제어 데이터 */
    TC_DIAGNOSTIC = 2,         /* 진단 데이터 */
    TC_BEST_EFFORT = 0        /* 일반 트래픽 */
} traffic_class_t;

/* Streaming Profile */
typedef struct {
    const char *name;
    uint32_t bitrate;          /* bps */
    uint32_t burst_size;       /* bytes */
    traffic_class_t tc;
    uint8_t vlan_id_start;
    uint8_t vlan_count;
} streaming_profile_t;

/* 실제 스트리밍 프로파일 */
static streaming_profile_t profiles[] = {
    {"4K HDR Live", 25000000, 65536, TC_LIVE_4K_VIDEO, 100, 4},      /* 25Mbps */
    {"FHD Live", 8000000, 32768, TC_LIVE_FHD_VIDEO, 110, 8},         /* 8Mbps */
    {"HD VOD", 4000000, 16384, TC_VOD_STREAMING, 120, 16},           /* 4Mbps */
    {"Audio HQ", 320000, 4096, TC_AUDIO_STREAM, 130, 8},             /* 320kbps */
    {"Control", 100000, 1522, TC_CONTROL_DATA, 140, 4}               /* 100kbps */
};

/* Global Variables */
static void *reg_base = NULL;
static int mem_fd = -1;

/* Register Access Functions */
static inline uint32_t lan9662_read(uint32_t offset) {
    if (!reg_base) return 0;
    return *((volatile uint32_t*)((uint8_t*)reg_base + offset));
}

static inline void lan9662_write(uint32_t offset, uint32_t value) {
    if (!reg_base) return;
    *((volatile uint32_t*)((uint8_t*)reg_base + offset)) = value;
    usleep(1); /* 안정화 대기 */
}

/* CBS 파라미터 계산 - 실제 하드웨어 특성 반영 */
static void calculate_cbs_params(uint32_t bitrate, uint32_t port_speed,
                                 uint32_t *cir, uint32_t *eir, 
                                 uint32_t *cbs, uint32_t *ebs) {
    /* Committed Information Rate (보장 대역폭) */
    *cir = bitrate;
    
    /* Excess Information Rate (초과 대역폭) - 버스트 허용 */
    *eir = bitrate / 4; /* 25% 추가 버스트 허용 */
    
    /* Committed Burst Size (보장 버스트 크기) */
    *cbs = (bitrate * 20) / 1000; /* 20ms 버스트 */
    if (*cbs < LAN9662_MAX_FRAME_SIZE) {
        *cbs = LAN9662_MAX_FRAME_SIZE;
    }
    
    /* Excess Burst Size (초과 버스트 크기) */
    *ebs = *cbs / 2;
}

/* LAN9662 초기화 */
int lan9662_init(void) {
    /* /dev/mem 열기 */
    mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mem_fd < 0) {
        perror("Failed to open /dev/mem");
        return -1;
    }
    
    /* 레지스터 메모리 매핑 */
    reg_base = mmap(NULL, LAN9662_REG_SIZE, PROT_READ | PROT_WRITE,
                    MAP_SHARED, mem_fd, LAN9662_BASE_ADDR);
    if (reg_base == MAP_FAILED) {
        perror("Failed to mmap registers");
        close(mem_fd);
        return -1;
    }
    
    printf("LAN9662 초기화 완료 (Base: 0x%lx)\n", LAN9662_BASE_ADDR);
    
    /* Chip Mode 확인 */
    uint32_t chip_mode = lan9662_read(DEVCPU_GCB_CHIP_MODE - LAN9662_BASE_ADDR);
    printf("Chip Mode: 0x%08X\n", chip_mode);
    
    return 0;
}

/* 포트별 CBS 구성 */
int lan9662_configure_port_cbs(uint8_t port, streaming_profile_t *profile) {
    uint32_t cir, eir, cbs, ebs;
    
    if (port >= LAN9662_NUM_PORTS) {
        fprintf(stderr, "Invalid port number: %d\n", port);
        return -1;
    }
    
    printf("\n[Port %d] %s 프로파일 설정\n", port, profile->name);
    printf("  - Bitrate: %.2f Mbps\n", profile->bitrate / 1000000.0);
    printf("  - Traffic Class: TC%d\n", profile->tc);
    printf("  - VLAN Range: %d-%d\n", 
           profile->vlan_id_start, 
           profile->vlan_id_start + profile->vlan_count - 1);
    
    /* CBS 파라미터 계산 */
    calculate_cbs_params(profile->bitrate, LAN9662_PORT_SPEED_1G,
                        &cir, &eir, &cbs, &ebs);
    
    printf("  - CIR: %u bps, EIR: %u bps\n", cir, eir);
    printf("  - CBS: %u bytes, EBS: %u bytes\n", cbs, ebs);
    
    /* 레지스터 설정 */
    for (int queue = 0; queue < LAN9662_NUM_QUEUES; queue++) {
        if (queue == profile->tc) {
            /* 해당 TC에 CBS 설정 */
            lan9662_write(QSYS_CBS_CIR(port, queue), cir / 100); /* 100bps 단위 */
            lan9662_write(QSYS_CBS_EIR(port, queue), eir / 100);
            lan9662_write(QSYS_CBS_CBS(port, queue), cbs);
            lan9662_write(QSYS_CBS_EBS(port, queue), ebs);
            
            printf("  - Queue %d: CBS 활성화\n", queue);
        } else if (queue == TC_BEST_EFFORT) {
            /* Best Effort는 남은 대역폭 사용 */
            lan9662_write(QSYS_CBS_CIR(port, queue), 0);
            lan9662_write(QSYS_CBS_EIR(port, queue), 0);
            lan9662_write(QSYS_CBS_CBS(port, queue), 0);
            lan9662_write(QSYS_CBS_EBS(port, queue), 0);
        }
    }
    
    return 0;
}

/* VLAN to TC 매핑 설정 */
int lan9662_configure_vlan_mapping(streaming_profile_t *profile) {
    printf("\nVLAN → TC 매핑 설정\n");
    
    for (int i = 0; i < profile->vlan_count; i++) {
        uint16_t vlan_id = profile->vlan_id_start + i;
        uint32_t se_idx = vlan_id; /* Service Entry Index */
        uint32_t qmap_val = (profile->tc << 0) |  /* Queue number */
                           (1 << 3);               /* Enable */
        
        lan9662_write(QSYS_QMAP_SE_BASE(se_idx), qmap_val);
        printf("  VLAN %d → TC%d\n", vlan_id, profile->tc);
    }
    
    return 0;
}

/* 실시간 통계 모니터링 */
void lan9662_monitor_statistics(uint8_t port) {
    printf("\n=== Port %d 실시간 통계 ===\n", port);
    
    /* 포트 통계 레지스터 읽기 */
    uint32_t tx_octets = lan9662_read(0x04000000 + (port * 0x100));
    uint32_t rx_octets = lan9662_read(0x04000004 + (port * 0x100));
    uint32_t tx_frames = lan9662_read(0x04000008 + (port * 0x100));
    uint32_t rx_frames = lan9662_read(0x0400000C + (port * 0x100));
    uint32_t drops = lan9662_read(0x04000010 + (port * 0x100));
    
    printf("TX: %u bytes (%u frames)\n", tx_octets, tx_frames);
    printf("RX: %u bytes (%u frames)\n", rx_octets, rx_frames);
    printf("Drops: %u frames\n", drops);
    
    /* Queue별 통계 */
    for (int q = 0; q < LAN9662_NUM_QUEUES; q++) {
        uint32_t queue_depth = lan9662_read(0x0C200000 + (port * 0x40) + (q * 0x4));
        if (queue_depth > 0) {
            printf("Queue %d depth: %u\n", q, queue_depth);
        }
    }
}

/* VLC 스트리밍 설정 스크립트 생성 */
void generate_vlc_config(streaming_profile_t *profile, const char *source_file) {
    char filename[256];
    snprintf(filename, sizeof(filename), "vlc_stream_%s.sh", profile->name);
    
    FILE *fp = fopen(filename, "w");
    if (!fp) return;
    
    fprintf(fp, "#!/bin/bash\n");
    fprintf(fp, "# VLC Streaming Configuration for %s\n\n", profile->name);
    
    /* VLC 스트리밍 명령어 */
    fprintf(fp, "vlc -I dummy '%s' \\\n", source_file);
    fprintf(fp, "  --sout '#transcode{vcodec=h264,vb=%d,scale=Auto,acodec=aac,ab=128,channels=2,samplerate=44100,scodec=none}:", 
            profile->bitrate / 1000); /* kbps */
    fprintf(fp, "duplicate{dst=rtp{sdp=rtsp://:");
    fprintf(fp, "%d/stream.sdp},dst=display}' \\\n", 8554 + profile->tc);
    fprintf(fp, "  --network-caching=300 \\\n");
    fprintf(fp, "  --sout-rtp-proto=udp \\\n");
    fprintf(fp, "  --sout-rtp-port=5004 \\\n");
    fprintf(fp, "  --sout-rtp-sap \\\n");
    fprintf(fp, "  --sout-rtp-name='%s Stream' \\\n", profile->name);
    
    /* VLAN 태깅 */
    fprintf(fp, "  --sout-udp-vlan=%d \\\n", profile->vlan_id_start);
    fprintf(fp, "  --sout-udp-priority=%d\n", profile->tc);
    
    fclose(fp);
    chmod(filename, 0755);
    
    printf("VLC 설정 스크립트 생성: %s\n", filename);
}

/* VOD 서버 설정 */
void setup_vod_server(void) {
    printf("\n=== VOD 서버 구성 ===\n");
    
    /* nginx + RTMP 모듈 설정 */
    FILE *fp = fopen("nginx_vod.conf", "w");
    if (!fp) return;
    
    fprintf(fp, "rtmp {\n");
    fprintf(fp, "    server {\n");
    fprintf(fp, "        listen 1935;\n");
    fprintf(fp, "        chunk_size 4096;\n\n");
    
    for (int i = 0; i < sizeof(profiles)/sizeof(profiles[0]); i++) {
        fprintf(fp, "        application %s {\n", profiles[i].name);
        fprintf(fp, "            live on;\n");
        fprintf(fp, "            record off;\n");
        fprintf(fp, "            allow publish all;\n");
        fprintf(fp, "            allow play all;\n");
        
        /* HLS 설정 */
        fprintf(fp, "            hls on;\n");
        fprintf(fp, "            hls_path /var/www/hls/%s;\n", profiles[i].name);
        fprintf(fp, "            hls_fragment 3;\n");
        fprintf(fp, "            hls_playlist_length 60;\n");
        
        /* DASH 설정 */
        fprintf(fp, "            dash on;\n");
        fprintf(fp, "            dash_path /var/www/dash/%s;\n", profiles[i].name);
        fprintf(fp, "            dash_fragment 3;\n");
        fprintf(fp, "            dash_playlist_length 60;\n");
        fprintf(fp, "        }\n\n");
    }
    
    fprintf(fp, "    }\n");
    fprintf(fp, "}\n");
    
    fclose(fp);
    printf("VOD 서버 설정 완료: nginx_vod.conf\n");
}

/* 메인 테스트 프로그램 */
int main(int argc, char *argv[]) {
    printf("===========================================\n");
    printf("   LAN9662 TSN CBS 구성 및 테스트 도구\n");
    printf("   Microchip 64-Port Gigabit Switch\n");
    printf("===========================================\n\n");
    
    /* LAN9662 초기화 */
    if (lan9662_init() < 0) {
        fprintf(stderr, "LAN9662 초기화 실패\n");
        return -1;
    }
    
    /* 각 스트리밍 프로파일에 대해 CBS 구성 */
    for (int i = 0; i < sizeof(profiles)/sizeof(profiles[0]); i++) {
        /* 포트 그룹 할당: 4K는 포트 1-4, FHD는 5-12, VOD는 13-28 등 */
        int start_port = i * 16;
        int end_port = start_port + 4;
        
        for (int port = start_port; port < end_port && port < 64; port++) {
            lan9662_configure_port_cbs(port, &profiles[i]);
        }
        
        /* VLAN 매핑 설정 */
        lan9662_configure_vlan_mapping(&profiles[i]);
        
        /* VLC 설정 생성 */
        generate_vlc_config(&profiles[i], "/media/video/sample.mp4");
    }
    
    /* VOD 서버 설정 */
    setup_vod_server();
    
    /* 모니터링 루프 */
    printf("\n실시간 모니터링 시작 (Ctrl+C로 종료)\n");
    while (1) {
        sleep(5);
        for (int port = 0; port < 8; port++) {
            lan9662_monitor_statistics(port);
        }
    }
    
    return 0;
}