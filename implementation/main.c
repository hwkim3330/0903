/**
 * Main test application for LAN9692 CBS implementation
 * Demonstrates CBS configuration for video streaming QoS
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "lan9692_cbs.h"

/* Test configuration */
#define VIDEO_STREAM_1_BW_MBPS    15  /* 15 Mbps for video stream 1 */
#define VIDEO_STREAM_2_BW_MBPS    15  /* 15 Mbps for video stream 2 */
#define CBS_RESERVATION_MBPS      20  /* Reserve 20 Mbps per stream */

static volatile int running = 1;

/* Signal handler for clean shutdown */
void signal_handler(int sig) {
    printf("\nShutting down CBS test...\n");
    running = 0;
}

/* Configure CBS for video streaming scenario */
int configure_video_streaming_cbs(void) {
    switch_config_t config;
    int ret;
    
    memset(&config, 0, sizeof(config));
    
    /* Enable VLAN and PTP */
    config.vlan_enabled = true;
    config.ptp_enabled = true;
    
    /* Configure Port 0 (Source) - No CBS needed */
    config.ports[0].port_id = 0;
    config.ports[0].port_speed = PORT_SPEED_1GBPS;
    
    /* Configure Port 1 (Sink 1) - CBS for egress traffic */
    config.ports[1].port_id = 1;
    config.ports[1].port_speed = PORT_SPEED_1GBPS;
    
    /* TC7 - Video Stream 1 */
    config.ports[1].tc_config[TC_VIDEO_STREAM_1].enabled = true;
    config.ports[1].tc_config[TC_VIDEO_STREAM_1].idle_slope = 
        lan9692_cbs_calculate_idle_slope(CBS_RESERVATION_MBPS, PORT_SPEED_1GBPS);
    config.ports[1].tc_config[TC_VIDEO_STREAM_1].send_slope = 
        PORT_SPEED_1GBPS - config.ports[1].tc_config[TC_VIDEO_STREAM_1].idle_slope;
    
    /* Calculate credit limits */
    config.ports[1].tc_config[TC_VIDEO_STREAM_1].hi_credit = 
        (1522 * config.ports[1].tc_config[TC_VIDEO_STREAM_1].idle_slope) / PORT_SPEED_1GBPS;
    config.ports[1].tc_config[TC_VIDEO_STREAM_1].lo_credit = 
        (1522 * config.ports[1].tc_config[TC_VIDEO_STREAM_1].send_slope) / PORT_SPEED_1GBPS;
    
    /* Configure Port 2 (Sink 2) - CBS for egress traffic */
    config.ports[2].port_id = 2;
    config.ports[2].port_speed = PORT_SPEED_1GBPS;
    
    /* TC6 - Video Stream 2 */
    config.ports[2].tc_config[TC_VIDEO_STREAM_2].enabled = true;
    config.ports[2].tc_config[TC_VIDEO_STREAM_2].idle_slope = 
        lan9692_cbs_calculate_idle_slope(CBS_RESERVATION_MBPS, PORT_SPEED_1GBPS);
    config.ports[2].tc_config[TC_VIDEO_STREAM_2].send_slope = 
        PORT_SPEED_1GBPS - config.ports[2].tc_config[TC_VIDEO_STREAM_2].idle_slope;
    
    /* Calculate credit limits */
    config.ports[2].tc_config[TC_VIDEO_STREAM_2].hi_credit = 
        (1522 * config.ports[2].tc_config[TC_VIDEO_STREAM_2].idle_slope) / PORT_SPEED_1GBPS;
    config.ports[2].tc_config[TC_VIDEO_STREAM_2].lo_credit = 
        (1522 * config.ports[2].tc_config[TC_VIDEO_STREAM_2].send_slope) / PORT_SPEED_1GBPS;
    
    /* Configure Port 3 (BE Traffic Generator) - No CBS */
    config.ports[3].port_id = 3;
    config.ports[3].port_speed = PORT_SPEED_1GBPS;
    
    /* Initialize CBS */
    ret = lan9692_cbs_init(&config);
    if (ret < 0) {
        fprintf(stderr, "Failed to initialize CBS: %d\n", ret);
        return ret;
    }
    
    printf("CBS configuration completed successfully\n");
    printf("Video Stream 1: Reserved %d Mbps on TC%d\n", 
           CBS_RESERVATION_MBPS, TC_VIDEO_STREAM_1);
    printf("Video Stream 2: Reserved %d Mbps on TC%d\n", 
           CBS_RESERVATION_MBPS, TC_VIDEO_STREAM_2);
    
    return 0;
}

/* Monitor CBS status */
void monitor_cbs_status(void) {
    uint32_t status;
    int ret;
    
    printf("\n=== CBS Status Monitor ===\n");
    
    for (int port = 0; port < NUM_PORTS; port++) {
        ret = lan9692_cbs_get_status(port, &status);
        if (ret == 0) {
            printf("Port %d Status: 0x%08X\n", port, status);
            lan9692_cbs_dump_config(port);
        }
    }
}

/* Test CBS with different scenarios */
void run_cbs_test_scenario(int scenario) {
    printf("\n=== Running Test Scenario %d ===\n", scenario);
    
    switch(scenario) {
        case 1:
            printf("Scenario 1: CBS Disabled - All traffic treated equally\n");
            /* Disable CBS on all ports */
            for (int port = 0; port < NUM_PORTS; port++) {
                lan9692_cbs_enable_port(port, false);
            }
            break;
            
        case 2:
            printf("Scenario 2: CBS Enabled - Video streams prioritized\n");
            /* Enable CBS on sink ports */
            lan9692_cbs_enable_port(1, true);
            lan9692_cbs_enable_port(2, true);
            break;
            
        case 3:
            printf("Scenario 3: Increased bandwidth reservation\n");
            /* Reconfigure with higher bandwidth */
            cbs_config_t high_bw_config;
            high_bw_config.enabled = true;
            high_bw_config.idle_slope = lan9692_cbs_calculate_idle_slope(30, PORT_SPEED_1GBPS);
            high_bw_config.send_slope = PORT_SPEED_1GBPS - high_bw_config.idle_slope;
            high_bw_config.hi_credit = (1522 * high_bw_config.idle_slope) / PORT_SPEED_1GBPS;
            high_bw_config.lo_credit = (1522 * high_bw_config.send_slope) / PORT_SPEED_1GBPS;
            
            lan9692_cbs_configure_tc(1, TC_VIDEO_STREAM_1, &high_bw_config);
            lan9692_cbs_configure_tc(2, TC_VIDEO_STREAM_2, &high_bw_config);
            break;
            
        default:
            printf("Unknown scenario\n");
            break;
    }
    
    /* Monitor for 10 seconds */
    for (int i = 0; i < 10 && running; i++) {
        sleep(1);
        printf(".");
        fflush(stdout);
    }
    printf("\n");
}

int main(int argc, char *argv[]) {
    int ret;
    int scenario = 2;  /* Default to CBS enabled */
    
    /* Parse command line arguments */
    if (argc > 1) {
        scenario = atoi(argv[1]);
    }
    
    /* Setup signal handler */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("LAN9692 CBS Test Application\n");
    printf("============================\n\n");
    
    /* Configure CBS for video streaming */
    ret = configure_video_streaming_cbs();
    if (ret < 0) {
        fprintf(stderr, "Failed to configure CBS\n");
        return EXIT_FAILURE;
    }
    
    /* Run test scenario */
    run_cbs_test_scenario(scenario);
    
    /* Monitor CBS status */
    while (running) {
        monitor_cbs_status();
        sleep(5);
    }
    
    printf("\nTest completed\n");
    return EXIT_SUCCESS;
}