#define BT_HIDP_DATA_IN        0xa1
#define BT_HIDP_DATA_OUT       0xa2
struct bt_hidp_hdr {
    uint8_t hdr;
    uint8_t protocol;
} __packed;

#define BT_HIDP_WII_LED_REPORT 0x11
#define BT_HIPD_WII_IR_CAM_EN1 0x13
#define BT_HIDP_WII_SPKR_EN    0x14
#define BT_HIDP_WII_STATUS_REQ 0x15
#define BT_HIDP_WII_SPKR_MUTE  0x19
#define BT_HIPD_WII_IR_CAM_EN2 0x1a
struct bt_hidp_wii_conf {
    uint8_t conf;
} __packed;

#define BT_HIDP_WII_REP_MODE   0x12
struct bt_hidp_wii_rep_mode {
    uint8_t options;
    uint8_t mode;
} __packed;

#define BT_HIDP_WII_EEPROM     0x00
#define BT_HIDP_WII_REG        0x04
#define BT_HIDP_WII_WR_MEM     0x16
struct bt_hidp_wii_wr_mem {
    uint8_t bank;
    uint8_t offset[3];
    uint8_t len;
    uint8_t data[16];
} __packed;

#define BT_HIDP_WII_RD_MEM     0x17
struct bt_hidp_wii_rd_mem {
    uint8_t bank;
    uint8_t offset[3];
    uint16_t len;
} __packed;

#define BT_HIDP_WII_SPKR_WR    0x18
struct bt_hidp_wii_spkr_wr {
    uint8_t len;
    uint8_t data[20];
} __packed;

#define BT_HIDP_WII_FLAGS_BATT_LOW (1 << 0)
#define BT_HIDP_WII_FLAGS_EXT_CONN (1 << 1)
#define BT_HIDP_WII_FLAGS_SPKR_ON  (1 << 2)
#define BT_HIDP_WII_FLAGS_IR_ON    (1 << 3)
#define BT_HIDP_WII_FLAGS_LED1     (1 << 4)
#define BT_HIDP_WII_FLAGS_LED2     (1 << 5)
#define BT_HIDP_WII_FLAGS_LED3     (1 << 6)
#define BT_HIDP_WII_FLAGS_LED4     (1 << 7)
#define BT_HIDP_WII_STATUS     0x20
struct bt_hidp_wii_status {
    uint8_t buttons[2];
    uint8_t flags;
    uint8_t unknown[2];
    uint8_t battery;
} __packed;

#define BT_HIDP_WII_RD_DATA    0x21
struct bt_hidp_wii_rd_data {
    uint8_t buttons[2];
    uint8_t len_err;
    uint8_t offset[2];
    uint8_t data[16];
} __packed;

#define BT_HIDP_WII_ACK        0x22
struct bt_hidp_wii_ack {
    uint8_t buttons[2];
    uint8_t report;
    uint8_t err;
} __packed;

#define BT_HIDP_WII_CORE       0x30
struct bt_hidp_wii_core {
    uint8_t buttons[2];
} __packed;

#define BT_HIDP_WII_CORE_ACC   0x31
struct bt_hidp_wii_core_acc {
    uint8_t buttons[2];
    uint8_t acc[3];
} __packed;

#define BT_HIDP_WII_CORE_EXT8  0x32
struct bt_hidp_wii_core_ext8 {
    uint8_t buttons[2];
    uint8_t ext[8];
} __packed;

#define BT_HIDP_WII_CORE_ACC_IR 0x33
struct bt_hidp_wii_core_acc_ir {
    uint8_t buttons[2];
    uint8_t acc[3];
    uint8_t ir[12];
} __packed;

#define BT_HIDP_WII_CORE_EXT19 0x34
struct bt_hidp_wii_core_ext19 {
    uint8_t buttons[2];
    uint8_t ext[19];
} __packed;

#define BT_HIDP_WII_CORE_ACC_EXT 0x35
struct bt_hidp_wii_core_acc_ext {
    uint8_t buttons[2];
    uint8_t acc[3];
    uint8_t ext[16];
} __packed;

#define BT_HIDP_WII_CORE_CORE_IR_EXT 0x36
struct bt_hidp_wii_core_ir_ext {
    uint8_t buttons[2];
    uint8_t ir[10];
    uint8_t ext[9];
} __packed;

#define BT_HIDP_WII_CORE_ACC_IR_EXT 0x37
struct bt_hidp_wii_core_acc_ir_ext {
    uint8_t buttons[2];
    uint8_t acc[3];
    uint8_t ir[10];
    uint8_t ext[6];
} __packed;

#define BT_HIDP_WII_EXT        0x3d
struct bt_hidp_wii_ext {
    uint8_t ext[21];
} __packed;

#define BT_HIDP_WII_CORE_ACCX_IR 0x3e
#define BT_HIDP_WII_CORE_ACCY_IR 0x3f
struct bt_hidp_wii_core_acc_ir_i {
    uint8_t buttons[2];
    uint8_t acc;
    uint8_t ir[18];
} __packed;

struct bt_hidp_data {
    struct bt_hidp_hdr hidp_hdr;
    union {
        struct bt_hidp_wii_conf wii_conf;
        struct bt_hidp_wii_rep_mode wii_rep_mode;
        struct bt_hidp_wii_wr_mem wii_wr_mem;
        struct bt_hidp_wii_rd_mem wii_rd_mem;
        struct bt_hidp_wii_spkr_wr wii_spkr_wr;
        struct bt_hidp_wii_status wii_status;
        struct bt_hidp_wii_rd_data wii_rd_data;
        struct bt_hidp_wii_ack wii_ack;
        struct bt_hidp_wii_core wii_core;
        struct bt_hidp_wii_core_acc wii_core_acc;
        struct bt_hidp_wii_core_ext8 wii_core_ext8;
        struct bt_hidp_wii_core_acc_ir wii_core_acc_ir;
        struct bt_hidp_wii_core_ext19 wii_core_ext19;
        struct bt_hidp_wii_core_acc_ext wii_core_acc_ext;
        struct bt_hidp_wii_core_ir_ext wii_core_ir_ext;
        struct bt_hidp_wii_core_acc_ir_ext wii_core_acc_ir_ext;
        struct bt_hidp_wii_ext wii_ext;
        struct bt_hidp_wii_core_acc_ir_i wii_core_acc_ir_i;
    } hidp_data;
} __packed;
