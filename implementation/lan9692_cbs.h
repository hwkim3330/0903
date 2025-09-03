/**
 * LAN9692 Credit-Based Shaper Implementation
 * Header file for CBS configuration on Microchip LAN9692 TSN Switch
 */

#ifndef LAN9692_CBS_H
#define LAN9692_CBS_H

#include <stdint.h>
#include <stdbool.h>

/* LAN9692 Register Definitions */
#define LAN9692_BASE_ADDR           0x00000000
#define LAN9692_PORT_BASE(p)        (0x1000 + ((p) * 0x1000))
#define LAN9692_CBS_BASE(p)         (LAN9692_PORT_BASE(p) + 0x0800)

/* CBS Register Offsets */
#define CBS_CTRL_REG                0x00
#define CBS_IDLE_SLOPE_A_REG        0x04
#define CBS_IDLE_SLOPE_B_REG        0x08
#define CBS_SEND_SLOPE_A_REG        0x0C
#define CBS_SEND_SLOPE_B_REG        0x10
#define CBS_HI_CREDIT_A_REG         0x14
#define CBS_HI_CREDIT_B_REG         0x18
#define CBS_LO_CREDIT_A_REG         0x1C
#define CBS_LO_CREDIT_B_REG         0x20
#define CBS_STATUS_REG               0x24

/* CBS Control Bits */
#define CBS_ENABLE_A                (1 << 0)
#define CBS_ENABLE_B                (1 << 1)
#define CBS_CREDIT_RESET            (1 << 8)
#define CBS_MODE_CREDIT_BASED       (1 << 16)

/* Traffic Class Definitions */
#define TC_VIDEO_STREAM_1           7
#define TC_VIDEO_STREAM_2           6
#define TC_BEST_EFFORT              0
#define MAX_TRAFFIC_CLASSES         8

/* Port Configuration */
#define NUM_PORTS                   4
#define PORT_SPEED_1GBPS            1000000000
#define PORT_SPEED_100MBPS          100000000

/* CBS Parameters Structure */
typedef struct {
    uint32_t idle_slope;        /* bits per second */
    uint32_t send_slope;        /* bits per second (negative) */
    uint32_t hi_credit;         /* bytes */
    uint32_t lo_credit;         /* bytes */
    bool enabled;
} cbs_config_t;

/* Port CBS Configuration */
typedef struct {
    uint8_t port_id;
    uint32_t port_speed;
    cbs_config_t tc_config[MAX_TRAFFIC_CLASSES];
} port_cbs_config_t;

/* Switch Configuration */
typedef struct {
    port_cbs_config_t ports[NUM_PORTS];
    bool ptp_enabled;
    bool vlan_enabled;
} switch_config_t;

/* Function Prototypes */

/**
 * Initialize CBS for LAN9692 switch
 * @param config: Pointer to switch configuration
 * @return: 0 on success, negative on error
 */
int lan9692_cbs_init(switch_config_t *config);

/**
 * Configure CBS for a specific port and traffic class
 * @param port: Port number (0-3)
 * @param tc: Traffic class (0-7)
 * @param config: CBS configuration parameters
 * @return: 0 on success, negative on error
 */
int lan9692_cbs_configure_tc(uint8_t port, uint8_t tc, cbs_config_t *config);

/**
 * Enable/Disable CBS for a port
 * @param port: Port number
 * @param enable: true to enable, false to disable
 * @return: 0 on success, negative on error
 */
int lan9692_cbs_enable_port(uint8_t port, bool enable);

/**
 * Calculate idle slope based on bandwidth allocation
 * @param bandwidth_mbps: Required bandwidth in Mbps
 * @param port_speed: Port speed in bps
 * @return: Idle slope value in bps
 */
uint32_t lan9692_cbs_calculate_idle_slope(uint32_t bandwidth_mbps, uint32_t port_speed);

/**
 * Get CBS status for a port
 * @param port: Port number
 * @param status: Pointer to store status
 * @return: 0 on success, negative on error
 */
int lan9692_cbs_get_status(uint8_t port, uint32_t *status);

/**
 * Set VLAN to Traffic Class mapping
 * @param vlan_id: VLAN ID
 * @param tc: Traffic class
 * @return: 0 on success, negative on error
 */
int lan9692_set_vlan_tc_mapping(uint16_t vlan_id, uint8_t tc);

/**
 * Set PCP to Traffic Class mapping
 * @param pcp: Priority Code Point (0-7)
 * @param tc: Traffic class
 * @return: 0 on success, negative on error
 */
int lan9692_set_pcp_tc_mapping(uint8_t pcp, uint8_t tc);

/**
 * Reset CBS credits for a port
 * @param port: Port number
 * @return: 0 on success, negative on error
 */
int lan9692_cbs_reset_credits(uint8_t port);

/**
 * Dump CBS configuration for debugging
 * @param port: Port number
 */
void lan9692_cbs_dump_config(uint8_t port);

#endif /* LAN9692_CBS_H */