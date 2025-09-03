/**
 * LAN9692 Credit-Based Shaper Implementation
 * CBS configuration for Microchip LAN9692 TSN Switch
 */

#include "lan9692_cbs.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>

/* MDIO Interface */
static int mdio_fd = -1;
static void *reg_base = NULL;

/* Register Access Functions */
static uint32_t lan9692_read_reg(uint32_t offset) {
    if (reg_base == NULL) return 0;
    return *((volatile uint32_t*)((uint8_t*)reg_base + offset));
}

static void lan9692_write_reg(uint32_t offset, uint32_t value) {
    if (reg_base == NULL) return;
    *((volatile uint32_t*)((uint8_t*)reg_base + offset)) = value;
}

/* Initialize memory mapping for register access */
static int lan9692_init_mdio(void) {
    mdio_fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (mdio_fd < 0) {
        perror("Failed to open /dev/mem");
        return -1;
    }
    
    reg_base = mmap(NULL, 0x10000, PROT_READ | PROT_WRITE, 
                    MAP_SHARED, mdio_fd, LAN9692_BASE_ADDR);
    if (reg_base == MAP_FAILED) {
        perror("Failed to mmap registers");
        close(mdio_fd);
        return -1;
    }
    
    return 0;
}

/* Calculate Send Slope from Idle Slope */
static uint32_t calculate_send_slope(uint32_t idle_slope, uint32_t port_speed) {
    return port_speed - idle_slope;
}

/* Calculate Hi/Lo Credit limits */
static void calculate_credit_limits(cbs_config_t *config, uint32_t port_speed) {
    /* Hi Credit = Maximum frame size * idle_slope / port_speed */
    uint32_t max_frame_size = 1522; /* Ethernet MTU + headers */
    config->hi_credit = (max_frame_size * config->idle_slope) / port_speed;
    
    /* Lo Credit = -max_frame_size * send_slope / port_speed */
    config->lo_credit = (max_frame_size * config->send_slope) / port_speed;
}

/* Initialize CBS for LAN9692 switch */
int lan9692_cbs_init(switch_config_t *config) {
    int ret;
    
    /* Initialize MDIO interface */
    ret = lan9692_init_mdio();
    if (ret < 0) {
        return ret;
    }
    
    /* Configure each port */
    for (int port = 0; port < NUM_PORTS; port++) {
        port_cbs_config_t *port_config = &config->ports[port];
        
        /* Reset CBS for this port */
        lan9692_cbs_reset_credits(port);
        
        /* Configure each traffic class */
        for (int tc = 0; tc < MAX_TRAFFIC_CLASSES; tc++) {
            if (port_config->tc_config[tc].enabled) {
                ret = lan9692_cbs_configure_tc(port, tc, &port_config->tc_config[tc]);
                if (ret < 0) {
                    printf("Failed to configure CBS for port %d, TC %d\n", port, tc);
                    return ret;
                }
            }
        }
        
        /* Enable CBS for the port if any TC is configured */
        bool enable = false;
        for (int tc = 0; tc < MAX_TRAFFIC_CLASSES; tc++) {
            if (port_config->tc_config[tc].enabled) {
                enable = true;
                break;
            }
        }
        
        if (enable) {
            lan9692_cbs_enable_port(port, true);
        }
    }
    
    /* Configure VLAN if enabled */
    if (config->vlan_enabled) {
        /* Map VLAN 100 to TC7 (Video Stream 1) */
        lan9692_set_vlan_tc_mapping(100, TC_VIDEO_STREAM_1);
        
        /* Map VLAN 101 to TC6 (Video Stream 2) */
        lan9692_set_vlan_tc_mapping(101, TC_VIDEO_STREAM_2);
    }
    
    /* Configure PCP mapping */
    lan9692_set_pcp_tc_mapping(7, TC_VIDEO_STREAM_1);
    lan9692_set_pcp_tc_mapping(6, TC_VIDEO_STREAM_2);
    lan9692_set_pcp_tc_mapping(0, TC_BEST_EFFORT);
    
    printf("CBS initialization completed successfully\n");
    return 0;
}

/* Configure CBS for a specific port and traffic class */
int lan9692_cbs_configure_tc(uint8_t port, uint8_t tc, cbs_config_t *config) {
    uint32_t cbs_base;
    uint32_t reg_offset;
    
    if (port >= NUM_PORTS || tc >= MAX_TRAFFIC_CLASSES) {
        return -EINVAL;
    }
    
    /* Calculate register base for this port */
    cbs_base = LAN9692_CBS_BASE(port);
    
    /* Select register set based on traffic class */
    /* TC7-TC6 use register set A, TC5-TC4 use register set B */
    if (tc >= 6) {
        /* Configure Class A */
        reg_offset = CBS_IDLE_SLOPE_A_REG;
        lan9692_write_reg(cbs_base + reg_offset, config->idle_slope);
        
        reg_offset = CBS_SEND_SLOPE_A_REG;
        lan9692_write_reg(cbs_base + reg_offset, config->send_slope);
        
        reg_offset = CBS_HI_CREDIT_A_REG;
        lan9692_write_reg(cbs_base + reg_offset, config->hi_credit);
        
        reg_offset = CBS_LO_CREDIT_A_REG;
        lan9692_write_reg(cbs_base + reg_offset, config->lo_credit);
        
    } else if (tc >= 4) {
        /* Configure Class B */
        reg_offset = CBS_IDLE_SLOPE_B_REG;
        lan9692_write_reg(cbs_base + reg_offset, config->idle_slope);
        
        reg_offset = CBS_SEND_SLOPE_B_REG;
        lan9692_write_reg(cbs_base + reg_offset, config->send_slope);
        
        reg_offset = CBS_HI_CREDIT_B_REG;
        lan9692_write_reg(cbs_base + reg_offset, config->hi_credit);
        
        reg_offset = CBS_LO_CREDIT_B_REG;
        lan9692_write_reg(cbs_base + reg_offset, config->lo_credit);
    }
    
    printf("Port %d TC %d: Configured CBS (idle=%u, send=%u)\n", 
           port, tc, config->idle_slope, config->send_slope);
    
    return 0;
}

/* Enable/Disable CBS for a port */
int lan9692_cbs_enable_port(uint8_t port, bool enable) {
    uint32_t cbs_base;
    uint32_t ctrl_val;
    
    if (port >= NUM_PORTS) {
        return -EINVAL;
    }
    
    cbs_base = LAN9692_CBS_BASE(port);
    ctrl_val = lan9692_read_reg(cbs_base + CBS_CTRL_REG);
    
    if (enable) {
        ctrl_val |= (CBS_ENABLE_A | CBS_ENABLE_B | CBS_MODE_CREDIT_BASED);
    } else {
        ctrl_val &= ~(CBS_ENABLE_A | CBS_ENABLE_B);
    }
    
    lan9692_write_reg(cbs_base + CBS_CTRL_REG, ctrl_val);
    
    printf("Port %d: CBS %s\n", port, enable ? "enabled" : "disabled");
    return 0;
}

/* Calculate idle slope based on bandwidth allocation */
uint32_t lan9692_cbs_calculate_idle_slope(uint32_t bandwidth_mbps, uint32_t port_speed) {
    /* Convert Mbps to bps */
    uint64_t bandwidth_bps = (uint64_t)bandwidth_mbps * 1000000;
    
    /* Idle slope should not exceed port speed */
    if (bandwidth_bps > port_speed) {
        bandwidth_bps = port_speed;
    }
    
    return (uint32_t)bandwidth_bps;
}

/* Get CBS status for a port */
int lan9692_cbs_get_status(uint8_t port, uint32_t *status) {
    uint32_t cbs_base;
    
    if (port >= NUM_PORTS || status == NULL) {
        return -EINVAL;
    }
    
    cbs_base = LAN9692_CBS_BASE(port);
    *status = lan9692_read_reg(cbs_base + CBS_STATUS_REG);
    
    return 0;
}

/* Set VLAN to Traffic Class mapping */
int lan9692_set_vlan_tc_mapping(uint16_t vlan_id, uint8_t tc) {
    uint32_t vlan_reg_offset = 0x2000 + (vlan_id * 4);
    uint32_t vlan_config;
    
    if (vlan_id > 4095 || tc >= MAX_TRAFFIC_CLASSES) {
        return -EINVAL;
    }
    
    /* Read current VLAN configuration */
    vlan_config = lan9692_read_reg(vlan_reg_offset);
    
    /* Update traffic class bits (bits 13-15) */
    vlan_config &= ~(0x7 << 13);
    vlan_config |= ((tc & 0x7) << 13);
    
    /* Write back configuration */
    lan9692_write_reg(vlan_reg_offset, vlan_config);
    
    printf("VLAN %d mapped to TC %d\n", vlan_id, tc);
    return 0;
}

/* Set PCP to Traffic Class mapping */
int lan9692_set_pcp_tc_mapping(uint8_t pcp, uint8_t tc) {
    uint32_t pcp_reg_offset = 0x3000;
    uint32_t pcp_config;
    
    if (pcp > 7 || tc >= MAX_TRAFFIC_CLASSES) {
        return -EINVAL;
    }
    
    /* Read current PCP mapping configuration */
    pcp_config = lan9692_read_reg(pcp_reg_offset);
    
    /* Update mapping for this PCP (3 bits per PCP) */
    uint32_t shift = pcp * 3;
    pcp_config &= ~(0x7 << shift);
    pcp_config |= ((tc & 0x7) << shift);
    
    /* Write back configuration */
    lan9692_write_reg(pcp_reg_offset, pcp_config);
    
    printf("PCP %d mapped to TC %d\n", pcp, tc);
    return 0;
}

/* Reset CBS credits for a port */
int lan9692_cbs_reset_credits(uint8_t port) {
    uint32_t cbs_base;
    uint32_t ctrl_val;
    
    if (port >= NUM_PORTS) {
        return -EINVAL;
    }
    
    cbs_base = LAN9692_CBS_BASE(port);
    ctrl_val = lan9692_read_reg(cbs_base + CBS_CTRL_REG);
    
    /* Set credit reset bit */
    ctrl_val |= CBS_CREDIT_RESET;
    lan9692_write_reg(cbs_base + CBS_CTRL_REG, ctrl_val);
    
    /* Wait for reset to complete */
    usleep(1000);
    
    /* Clear credit reset bit */
    ctrl_val &= ~CBS_CREDIT_RESET;
    lan9692_write_reg(cbs_base + CBS_CTRL_REG, ctrl_val);
    
    printf("Port %d: CBS credits reset\n", port);
    return 0;
}

/* Dump CBS configuration for debugging */
void lan9692_cbs_dump_config(uint8_t port) {
    uint32_t cbs_base;
    uint32_t ctrl, status;
    uint32_t idle_a, idle_b, send_a, send_b;
    uint32_t hi_a, hi_b, lo_a, lo_b;
    
    if (port >= NUM_PORTS) {
        return;
    }
    
    cbs_base = LAN9692_CBS_BASE(port);
    
    /* Read all CBS registers */
    ctrl = lan9692_read_reg(cbs_base + CBS_CTRL_REG);
    status = lan9692_read_reg(cbs_base + CBS_STATUS_REG);
    idle_a = lan9692_read_reg(cbs_base + CBS_IDLE_SLOPE_A_REG);
    idle_b = lan9692_read_reg(cbs_base + CBS_IDLE_SLOPE_B_REG);
    send_a = lan9692_read_reg(cbs_base + CBS_SEND_SLOPE_A_REG);
    send_b = lan9692_read_reg(cbs_base + CBS_SEND_SLOPE_B_REG);
    hi_a = lan9692_read_reg(cbs_base + CBS_HI_CREDIT_A_REG);
    hi_b = lan9692_read_reg(cbs_base + CBS_HI_CREDIT_B_REG);
    lo_a = lan9692_read_reg(cbs_base + CBS_LO_CREDIT_A_REG);
    lo_b = lan9692_read_reg(cbs_base + CBS_LO_CREDIT_B_REG);
    
    printf("\n=== Port %d CBS Configuration ===\n", port);
    printf("Control: 0x%08X (Class A: %s, Class B: %s)\n", 
           ctrl,
           (ctrl & CBS_ENABLE_A) ? "Enabled" : "Disabled",
           (ctrl & CBS_ENABLE_B) ? "Enabled" : "Disabled");
    printf("Status: 0x%08X\n", status);
    printf("\nClass A (TC7-TC6):\n");
    printf("  Idle Slope: %u bps\n", idle_a);
    printf("  Send Slope: %u bps\n", send_a);
    printf("  Hi Credit:  %u bytes\n", hi_a);
    printf("  Lo Credit:  %u bytes\n", lo_a);
    printf("\nClass B (TC5-TC4):\n");
    printf("  Idle Slope: %u bps\n", idle_b);
    printf("  Send Slope: %u bps\n", send_b);
    printf("  Hi Credit:  %u bytes\n", hi_b);
    printf("  Lo Credit:  %u bytes\n", lo_b);
    printf("================================\n\n");
}