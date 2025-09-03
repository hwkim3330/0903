# CBS Configuration Guide

## Overview

Credit-Based Shaper (CBS) is a traffic shaping mechanism defined in IEEE 802.1Qav that provides bandwidth reservation for time-sensitive streams in TSN networks.

## CBS Parameters

### Key Parameters

1. **idleSlope**: Rate at which credit increases when queue is empty (bps)
2. **sendSlope**: Rate at which credit decreases during transmission (bps)
3. **hiCredit**: Maximum positive credit limit (bytes)
4. **loCredit**: Maximum negative credit limit (bytes)

### Parameter Calculation

```c
// Basic formulas
sendSlope = idleSlope - portSpeed
hiCredit = maxFrameSize * idleSlope / portSpeed
loCredit = maxFrameSize * |sendSlope| / portSpeed
```

## Configuration Examples

### Example 1: Video Streaming (15 Mbps)

```c
// Reserve 20 Mbps for 15 Mbps video stream
cbs_config_t video_config = {
    .idle_slope = 20000000,    // 20 Mbps
    .send_slope = 980000000,   // 980 Mbps (1Gbps - 20Mbps)
    .hi_credit = 304,          // (1522 * 20M) / 1G
    .lo_credit = 1490,         // (1522 * 980M) / 1G
    .enabled = true
};
```

### Example 2: Control Traffic (5 Mbps)

```c
// Reserve 8 Mbps for control messages
cbs_config_t control_config = {
    .idle_slope = 8000000,     // 8 Mbps
    .send_slope = 992000000,   // 992 Mbps
    .hi_credit = 122,          // (1522 * 8M) / 1G
    .lo_credit = 1510,         // (1522 * 992M) / 1G
    .enabled = true
};
```

### Example 3: Audio Streaming (2 Mbps)

```c
// Reserve 3 Mbps for audio stream
cbs_config_t audio_config = {
    .idle_slope = 3000000,     // 3 Mbps
    .send_slope = 997000000,   // 997 Mbps
    .hi_credit = 46,           // (1522 * 3M) / 1G
    .lo_credit = 1517,         // (1522 * 997M) / 1G
    .enabled = true
};
```

## Traffic Class Mapping

### VLAN to TC Mapping

```c
// Map VLANs to traffic classes
lan9692_set_vlan_tc_mapping(100, 7);  // Video 1 → TC7
lan9692_set_vlan_tc_mapping(101, 6);  // Video 2 → TC6
lan9692_set_vlan_tc_mapping(200, 5);  // Audio → TC5
lan9692_set_vlan_tc_mapping(300, 4);  // Control → TC4
```

### PCP to TC Mapping

```c
// Map Priority Code Points to traffic classes
lan9692_set_pcp_tc_mapping(7, 7);  // Highest priority
lan9692_set_pcp_tc_mapping(6, 6);
lan9692_set_pcp_tc_mapping(5, 5);
lan9692_set_pcp_tc_mapping(4, 4);
lan9692_set_pcp_tc_mapping(3, 3);
lan9692_set_pcp_tc_mapping(2, 2);
lan9692_set_pcp_tc_mapping(1, 1);
lan9692_set_pcp_tc_mapping(0, 0);  // Best effort
```

## Best Practices

### 1. Bandwidth Allocation

- Reserve 10-20% more bandwidth than actual requirement
- Sum of all idleSlopes must not exceed port speed
- Consider burst traffic patterns

### 2. Credit Limits

- Use maximum Ethernet frame size (1522 bytes) for calculations
- Adjust for jumbo frames if enabled
- Monitor credit utilization during operation

### 3. Traffic Classification

- Use VLAN for static stream identification
- Use PCP for dynamic priority assignment
- Combine both for flexible QoS management

## Configuration Script

```bash
#!/bin/bash
# CBS configuration script

# Set video stream parameters
./lan9692_cbs_test config \
    --port 1 \
    --tc 7 \
    --idle-slope 20000000 \
    --enable

# Set control traffic parameters
./lan9692_cbs_test config \
    --port 1 \
    --tc 4 \
    --idle-slope 8000000 \
    --enable

# Apply configuration
./lan9692_cbs_test apply
```

## Monitoring CBS Status

```c
// Check CBS status
uint32_t status;
lan9692_cbs_get_status(port, &status);

// Dump configuration
lan9692_cbs_dump_config(port);
```

## Troubleshooting

### Issue: Video stream stuttering

**Solution**: Increase idleSlope reservation
```c
config.idle_slope = bandwidth_required * 1.2;  // 20% margin
```

### Issue: High latency for control traffic

**Solution**: Use higher traffic class
```c
lan9692_set_vlan_tc_mapping(control_vlan, 6);  // Higher priority
```

### Issue: BE traffic starvation

**Solution**: Adjust CBS parameters
```c
// Reduce video reservation slightly
video_config.idle_slope = 18000000;  // 18 Mbps instead of 20
```

## Advanced Configuration

### Dynamic Adjustment

```c
// Monitor and adjust CBS parameters dynamically
void adjust_cbs_parameters(uint8_t port, uint8_t tc) {
    uint32_t status;
    lan9692_cbs_get_status(port, &status);
    
    if (status & CBS_CREDIT_EXHAUSTED) {
        // Increase reservation
        current_config.idle_slope *= 1.1;
        lan9692_cbs_configure_tc(port, tc, &current_config);
    }
}
```

### Multi-Class Configuration

```c
// Configure multiple traffic classes
for (int tc = 4; tc <= 7; tc++) {
    cbs_config_t config = {
        .idle_slope = calculate_idle_slope(tc),
        .send_slope = PORT_SPEED_1GBPS - config.idle_slope,
        .hi_credit = calculate_hi_credit(config.idle_slope),
        .lo_credit = calculate_lo_credit(config.send_slope),
        .enabled = true
    };
    lan9692_cbs_configure_tc(port, tc, &config);
}
```