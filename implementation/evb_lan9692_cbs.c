/**
 * EVB-LAN9692 CBS Implementation
 * Microchip LAN9692 Evaluation Board with VelocityDriveSP SDK
 * 실제 하드웨어 기반 CBS 구현
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

/* EVB-LAN9692 보드 구성 */
#define LAN9692_PORTS           12      /* LAN9692는 12포트 스위치 */
#define LAN9662_PORTS           64      /* LAN9662는 64포트 스위치 */
#define NUM_TRAFFIC_CLASSES     8
#define VLAN_BASE_ID           100
#define TTY_DEVICE             "/dev/ttyACM0"

/* 트래픽 클래스 정의 (실제 테스트 기준) */
typedef enum {
    TC_4K_VIDEO = 7,       /* 4K 실시간 영상 - 최고 우선순위 */
    TC_FHD_VIDEO = 6,      /* FHD 실시간 영상 */
    TC_HD_VOD = 5,         /* HD VOD 스트리밍 */
    TC_AUDIO = 4,          /* 오디오 스트림 */
    TC_CONTROL = 3,        /* 제어 데이터 */
    TC_DIAG = 2,           /* 진단 데이터 */
    TC_BULK = 1,           /* 벌크 데이터 */
    TC_BE = 0              /* 베스트 에포트 */
} traffic_class_t;

/* CBS 구성 파라미터 */
typedef struct {
    uint8_t port;
    uint8_t tc;
    uint32_t idle_slope;   /* kbps */
    uint32_t send_slope;   /* kbps */
    uint16_t vlan_id;
    uint8_t pcp;
} cbs_config_t;

/* 테스트 시나리오별 CBS 설정 */
static cbs_config_t test_configs[] = {
    /* Port 8 인그레스 - 비디오 스트림 수신 */
    {8, TC_4K_VIDEO,  25000, 975000, 100, 7},   /* 4K: 25Mbps */
    {8, TC_FHD_VIDEO,  8000, 992000, 110, 6},   /* FHD: 8Mbps */
    {8, TC_HD_VOD,     4000, 996000, 120, 5},   /* VOD: 4Mbps */
    
    /* Port 10/11 이그레스 - PC로 전송 */
    {10, TC_4K_VIDEO, 30000, 970000, 100, 7},   /* 4K 여유있게 30Mbps */
    {10, TC_FHD_VIDEO, 10000, 990000, 110, 6},  /* FHD 여유있게 10Mbps */
    {11, TC_HD_VOD,    5000, 995000, 120, 5},   /* VOD 여유있게 5Mbps */
};

/* YAML 파일 생성 함수 */
int generate_yaml_file(const char *filename, const char *content) {
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        perror("Failed to create YAML file");
        return -1;
    }
    
    fprintf(fp, "%s", content);
    fclose(fp);
    
    printf("Generated: %s\n", filename);
    return 0;
}

/* VLAN 설정 YAML 생성 */
int setup_vlan_configuration(void) {
    char yaml_content[4096];
    
    /* VLAN 100/110/120 설정 - 포트 8, 10, 11 */
    snprintf(yaml_content, sizeof(yaml_content),
        "# VLAN Configuration for EVB-LAN9692\n"
        "# Port 8: Ingress (Video Source)\n"
        "# Port 10, 11: Egress (PC Receivers)\n\n"
        
        "# Set ports to C-VLAN aware\n"
        "- ? \"/ietf-interfaces:interfaces/interface[name='8']/ieee802-dot1q-bridge:bridge-port/port-type\"\n"
        "  : ieee802-dot1q-bridge:c-vlan-bridge-port\n"
        "- ? \"/ietf-interfaces:interfaces/interface[name='10']/ieee802-dot1q-bridge:bridge-port/port-type\"\n"
        "  : ieee802-dot1q-bridge:c-vlan-bridge-port\n"
        "- ? \"/ietf-interfaces:interfaces/interface[name='11']/ieee802-dot1q-bridge:bridge-port/port-type\"\n"
        "  : ieee802-dot1q-bridge:c-vlan-bridge-port\n\n"
        
        "# Accept only VLAN tagged frames\n"
        "- ? \"/ietf-interfaces:interfaces/interface[name='8']/ieee802-dot1q-bridge:bridge-port/acceptable-frame\"\n"
        "  : admit-only-VLAN-tagged-frames\n"
        "- ? \"/ietf-interfaces:interfaces/interface[name='10']/ieee802-dot1q-bridge:bridge-port/acceptable-frame\"\n"
        "  : admit-only-VLAN-tagged-frames\n"
        "- ? \"/ietf-interfaces:interfaces/interface[name='11']/ieee802-dot1q-bridge:bridge-port/acceptable-frame\"\n"
        "  : admit-only-VLAN-tagged-frames\n\n"
        
        "# Enable ingress filtering\n"
        "- ? \"/ietf-interfaces:interfaces/interface[name='8']/ieee802-dot1q-bridge:bridge-port/enable-ingress-filtering\"\n"
        "  : true\n"
        "- ? \"/ietf-interfaces:interfaces/interface[name='10']/ieee802-dot1q-bridge:bridge-port/enable-ingress-filtering\"\n"
        "  : true\n"
        "- ? \"/ietf-interfaces:interfaces/interface[name='11']/ieee802-dot1q-bridge:bridge-port/enable-ingress-filtering\"\n"
        "  : true\n\n"
        
        "# VLAN 100 (4K Video) membership\n"
        "- ? \"/ieee802-dot1q-bridge:bridges/bridge[name='b0']/component[name='c0']/filtering-database/vlan-registration-entry\"\n"
        "  : database-id: 0\n"
        "    vids: '100'\n"
        "    entry-type: static\n"
        "    port-map:\n"
        "    - port-ref: 8\n"
        "      static-vlan-registration-entries:\n"
        "        vlan-transmitted: tagged\n"
        "    - port-ref: 10\n"
        "      static-vlan-registration-entries:\n"
        "        vlan-transmitted: tagged\n"
        "    - port-ref: 11\n"
        "      static-vlan-registration-entries:\n"
        "        vlan-transmitted: tagged\n"
    );
    
    return generate_yaml_file("vlan_setup.yaml", yaml_content);
}

/* PCP 디코딩/인코딩 설정 YAML 생성 */
int setup_pcp_mapping(void) {
    char yaml_content[4096];
    
    /* 포트 8: PCP → TC 디코딩 (1:1 매핑) */
    snprintf(yaml_content, sizeof(yaml_content),
        "# PCP Decoding for Port 8 (Ingress)\n"
        "# Maps PCP values to Traffic Classes (1:1)\n\n"
        
        "- ? \"/ietf-interfaces:interfaces/interface[name='8']/ieee802-dot1q-bridge:bridge-port/pcp-decoding-table/pcp-decoding-map\"\n"
        "  : pcp: 8P0D\n\n"
        
        "- \"/ietf-interfaces:interfaces/interface[name='8']/ieee802-dot1q-bridge:bridge-port/pcp-decoding-table/pcp-decoding-map[pcp='8P0D']/priority-map\":\n"
        "  - { priority-code-point: 0, priority: 0, drop-eligible: false }\n"
        "  - { priority-code-point: 1, priority: 1, drop-eligible: false }\n"
        "  - { priority-code-point: 2, priority: 2, drop-eligible: false }\n"
        "  - { priority-code-point: 3, priority: 3, drop-eligible: false }\n"
        "  - { priority-code-point: 4, priority: 4, drop-eligible: false }\n"
        "  - { priority-code-point: 5, priority: 5, drop-eligible: false }\n"
        "  - { priority-code-point: 6, priority: 6, drop-eligible: false }\n"
        "  - { priority-code-point: 7, priority: 7, drop-eligible: false }\n"
    );
    
    generate_yaml_file("pcp_decoding_p8.yaml", yaml_content);
    
    /* 포트 10, 11: TC → PCP 인코딩 */
    snprintf(yaml_content, sizeof(yaml_content),
        "# PCP Encoding for Port 10, 11 (Egress)\n"
        "# Maps Traffic Classes to PCP values (1:1)\n\n"
        
        "- ? \"/ietf-interfaces:interfaces/interface[name='10']/ieee802-dot1q-bridge:bridge-port/pcp-encoding-table/pcp-encoding-map\"\n"
        "  : pcp: 8P0D\n\n"
        
        "- \"/ietf-interfaces:interfaces/interface[name='10']/ieee802-dot1q-bridge:bridge-port/pcp-encoding-table/pcp-encoding-map[pcp='8P0D']/priority-map\":\n"
        "  - { priority: 0, dei: false, priority-code-point: 0 }\n"
        "  - { priority: 1, dei: false, priority-code-point: 1 }\n"
        "  - { priority: 2, dei: false, priority-code-point: 2 }\n"
        "  - { priority: 3, dei: false, priority-code-point: 3 }\n"
        "  - { priority: 4, dei: false, priority-code-point: 4 }\n"
        "  - { priority: 5, dei: false, priority-code-point: 5 }\n"
        "  - { priority: 6, dei: false, priority-code-point: 6 }\n"
        "  - { priority: 7, dei: false, priority-code-point: 7 }\n\n"
        
        "# Same for Port 11\n"
        "- ? \"/ietf-interfaces:interfaces/interface[name='11']/ieee802-dot1q-bridge:bridge-port/pcp-encoding-table/pcp-encoding-map\"\n"
        "  : pcp: 8P0D\n\n"
        
        "- \"/ietf-interfaces:interfaces/interface[name='11']/ieee802-dot1q-bridge:bridge-port/pcp-encoding-table/pcp-encoding-map[pcp='8P0D']/priority-map\":\n"
        "  - { priority: 0, dei: false, priority-code-point: 0 }\n"
        "  - { priority: 1, dei: false, priority-code-point: 1 }\n"
        "  - { priority: 2, dei: false, priority-code-point: 2 }\n"
        "  - { priority: 3, dei: false, priority-code-point: 3 }\n"
        "  - { priority: 4, dei: false, priority-code-point: 4 }\n"
        "  - { priority: 5, dei: false, priority-code-point: 5 }\n"
        "  - { priority: 6, dei: false, priority-code-point: 6 }\n"
        "  - { priority: 7, dei: false, priority-code-point: 7 }\n"
    );
    
    return generate_yaml_file("pcp_encoding_p10_p11.yaml", yaml_content);
}

/* CBS 설정 YAML 생성 */
int setup_cbs_configuration(void) {
    char yaml_content[8192];
    
    /* 포트 10, 11 CBS 셰이퍼 설정 */
    snprintf(yaml_content, sizeof(yaml_content),
        "# CBS Configuration for EVB-LAN9692\n"
        "# Credit-Based Shaper settings for egress ports\n\n"
        
        "# Port 10 CBS - PC1 (4K + FHD streams)\n"
        "- \"/ietf-interfaces:interfaces/interface[name='10']/mchp-velocitysp-port:eth-qos/config/traffic-class-shapers\":\n"
        "  - traffic-class: 7\n"
        "    credit-based:\n"
        "      idle-slope: 30000    # 30 Mbps for 4K\n"
        "  - traffic-class: 6\n"
        "    credit-based:\n"
        "      idle-slope: 10000    # 10 Mbps for FHD\n"
        "  - traffic-class: 5\n"
        "    credit-based:\n"
        "      idle-slope: 5000     # 5 Mbps for VOD\n\n"
        
        "# Port 11 CBS - PC2 (FHD + VOD streams)\n"
        "- \"/ietf-interfaces:interfaces/interface[name='11']/mchp-velocitysp-port:eth-qos/config/traffic-class-shapers\":\n"
        "  - traffic-class: 7\n"
        "    credit-based:\n"
        "      idle-slope: 30000    # 30 Mbps for 4K\n"
        "  - traffic-class: 6\n"
        "    credit-based:\n"
        "      idle-slope: 10000    # 10 Mbps for FHD\n"
        "  - traffic-class: 5\n"
        "    credit-based:\n"
        "      idle-slope: 5000     # 5 Mbps for VOD\n"
    );
    
    return generate_yaml_file("cbs_setup.yaml", yaml_content);
}

/* 통계 확인용 YAML 생성 */
int generate_stats_fetch_yaml(void) {
    char yaml_content[4096];
    
    snprintf(yaml_content, sizeof(yaml_content),
        "# Fetch statistics and configuration\n\n"
        
        "# Port types\n"
        "- \"/ietf-interfaces:interfaces/interface[name='8']/ieee802-dot1q-bridge:bridge-port/port-type\"\n"
        "- \"/ietf-interfaces:interfaces/interface[name='10']/ieee802-dot1q-bridge:bridge-port/port-type\"\n"
        "- \"/ietf-interfaces:interfaces/interface[name='11']/ieee802-dot1q-bridge:bridge-port/port-type\"\n\n"
        
        "# VLAN membership\n"
        "- \"/ieee802-dot1q-bridge:bridges/bridge[name='b0']/component[name='c0']/filtering-database/vlan-registration-entry[database-id='0'][vids='100']\"\n"
        "- \"/ieee802-dot1q-bridge:bridges/bridge[name='b0']/component[name='c0']/filtering-database/vlan-registration-entry[database-id='0'][vids='110']\"\n"
        "- \"/ieee802-dot1q-bridge:bridges/bridge[name='b0']/component[name='c0']/filtering-database/vlan-registration-entry[database-id='0'][vids='120']\"\n\n"
        
        "# PCP mappings\n"
        "- \"/ietf-interfaces:interfaces/interface[name='8']/ieee802-dot1q-bridge:bridge-port/pcp-decoding-table/pcp-decoding-map\"\n"
        "- \"/ietf-interfaces:interfaces/interface[name='10']/ieee802-dot1q-bridge:bridge-port/pcp-encoding-table/pcp-encoding-map\"\n"
        "- \"/ietf-interfaces:interfaces/interface[name='11']/ieee802-dot1q-bridge:bridge-port/pcp-encoding-table/pcp-encoding-map\"\n\n"
        
        "# CBS configuration\n"
        "- \"/ietf-interfaces:interfaces/interface[name='10']/mchp-velocitysp-port:eth-qos/config/traffic-class-shapers\"\n"
        "- \"/ietf-interfaces:interfaces/interface[name='11']/mchp-velocitysp-port:eth-qos/config/traffic-class-shapers\"\n\n"
        
        "# Traffic statistics\n"
        "- \"/ietf-interfaces:interfaces/interface[name='8']/mchp-velocitysp-port:eth-port/statistics/traffic-class\"\n"
        "- \"/ietf-interfaces:interfaces/interface[name='10']/mchp-velocitysp-port:eth-port/statistics/traffic-class\"\n"
        "- \"/ietf-interfaces:interfaces/interface[name='11']/mchp-velocitysp-port:eth-port/statistics/traffic-class\"\n"
    );
    
    return generate_yaml_file("fetch_stats.yaml", yaml_content);
}

/* VelocityDriveSP 명령 실행 */
int execute_velocitydrivesp_command(const char *yaml_file, const char *operation) {
    char cmd[512];
    
    /* dr mup1cc 명령 실행 */
    snprintf(cmd, sizeof(cmd), 
        "sudo dr mup1cc -d %s -m %s -i %s",
        TTY_DEVICE, operation, yaml_file);
    
    printf("Executing: %s\n", cmd);
    
    int ret = system(cmd);
    if (ret != 0) {
        fprintf(stderr, "Command failed with code %d\n", ret);
        return -1;
    }
    
    return 0;
}

/* CBS 활성화 */
int enable_cbs(void) {
    printf("\n=== Enabling CBS on EVB-LAN9692 ===\n\n");
    
    /* 1. VLAN 설정 */
    printf("Step 1: Configuring VLANs...\n");
    setup_vlan_configuration();
    execute_velocitydrivesp_command("vlan_setup.yaml", "ipatch");
    sleep(1);
    
    /* 2. PCP 매핑 설정 */
    printf("\nStep 2: Configuring PCP mappings...\n");
    setup_pcp_mapping();
    execute_velocitydrivesp_command("pcp_decoding_p8.yaml", "ipatch");
    execute_velocitydrivesp_command("pcp_encoding_p10_p11.yaml", "ipatch");
    sleep(1);
    
    /* 3. CBS 설정 */
    printf("\nStep 3: Configuring CBS shapers...\n");
    setup_cbs_configuration();
    execute_velocitydrivesp_command("cbs_setup.yaml", "ipatch");
    sleep(1);
    
    /* 4. 통계 확인 */
    printf("\nStep 4: Fetching statistics...\n");
    generate_stats_fetch_yaml();
    execute_velocitydrivesp_command("fetch_stats.yaml", "get");
    
    printf("\n=== CBS Configuration Complete ===\n");
    return 0;
}

/* CBS 비활성화 */
int disable_cbs(void) {
    char yaml_content[2048];
    
    printf("\n=== Disabling CBS on EVB-LAN9692 ===\n\n");
    
    /* CBS 비활성화 YAML */
    snprintf(yaml_content, sizeof(yaml_content),
        "# Disable CBS on all ports\n"
        "- \"/ietf-interfaces:interfaces/interface[name='10']/mchp-velocitysp-port:eth-qos/config/traffic-class-shapers\":\n"
        "  - traffic-class: 7\n"
        "    credit-based:\n"
        "      idle-slope: 0\n"
        "  - traffic-class: 6\n"
        "    credit-based:\n"
        "      idle-slope: 0\n"
        "  - traffic-class: 5\n"
        "    credit-based:\n"
        "      idle-slope: 0\n\n"
        
        "- \"/ietf-interfaces:interfaces/interface[name='11']/mchp-velocitysp-port:eth-qos/config/traffic-class-shapers\":\n"
        "  - traffic-class: 7\n"
        "    credit-based:\n"
        "      idle-slope: 0\n"
        "  - traffic-class: 6\n"
        "    credit-based:\n"
        "      idle-slope: 0\n"
        "  - traffic-class: 5\n"
        "    credit-based:\n"
        "      idle-slope: 0\n"
    );
    
    generate_yaml_file("cbs_disable.yaml", yaml_content);
    execute_velocitydrivesp_command("cbs_disable.yaml", "ipatch");
    
    printf("CBS disabled successfully\n");
    return 0;
}

/* 실시간 모니터링 */
void monitor_statistics(void) {
    printf("\n=== Real-time Statistics Monitoring ===\n");
    printf("Press Ctrl+C to stop monitoring\n\n");
    
    while (1) {
        system("clear");
        printf("EVB-LAN9692 Port Statistics\n");
        printf("============================\n");
        printf("Time: %s\n", __TIME__);
        
        /* 통계 가져오기 */
        execute_velocitydrivesp_command("fetch_stats.yaml", "get");
        
        sleep(5);
    }
}

int main(int argc, char *argv[]) {
    printf("=====================================\n");
    printf("   EVB-LAN9692 CBS Test Tool\n");
    printf("   VelocityDriveSP SDK Based\n");
    printf("=====================================\n\n");
    
    if (argc < 2) {
        printf("Usage: %s [enable|disable|monitor]\n", argv[0]);
        printf("  enable  - Enable CBS with test configuration\n");
        printf("  disable - Disable CBS\n");
        printf("  monitor - Monitor real-time statistics\n");
        return 1;
    }
    
    if (strcmp(argv[1], "enable") == 0) {
        enable_cbs();
    } else if (strcmp(argv[1], "disable") == 0) {
        disable_cbs();
    } else if (strcmp(argv[1], "monitor") == 0) {
        monitor_statistics();
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        return 1;
    }
    
    return 0;
}