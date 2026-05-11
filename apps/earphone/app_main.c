#include "system/includes.h"
/*#include "btcontroller_config.h"*/
#include "btstack/btstack_task.h"
#include "app_config.h"
#include "app_action.h"
#include "asm/pwm_led.h"
#include "tone_player.h"
#include "ui_manage.h"
#include "gpio.h"
#include "app_main.h"
#include "asm/charge.h"
#include "update.h"
#include "app_power_manage.h"
#include "audio_config.h"
#include "app_charge.h"
#include "bt_profile_cfg.h"
#include "dev_manager/dev_manager.h"
#include "update_loader_download.h"
#include "fft_and_pca.h"
#include "w25q128.h"
#include "string.h"

#ifndef CONFIG_MEDIA_NEW_ENABLE
#ifndef CONFIG_MEDIA_DEVELOP_ENABLE
#include "audio_dec_server.h"
#endif
#endif

#if TCFG_USER_TWS_ENABLE
#include "bt_tws.h"
#endif

#if TCFG_AUDIO_ANC_ENABLE
#include "audio_anc.h"
#endif

#define LOG_TAG_CONST       APP
#define LOG_TAG             "[APP]"
#define LOG_ERROR_ENABLE
#define LOG_DEBUG_ENABLE
#define LOG_INFO_ENABLE
/* #define LOG_DUMP_ENABLE */
#define LOG_CLI_ENABLE
#include "debug.h"

#ifdef CONFIG_BOARD_AISPEECH_VAD_ASR
u8 user_at_cmd_send_support = 1;
#endif

/*任务列表 */
const struct task_info task_info_table[] = {
    {"app_core",            1,     0,   768,   256 },
    {"sys_event",           7,     0,   256,   0   },
    {"systimer",		    7,	   0,   128,   0   },
    {"btctrler",            4,     0,   512,   384 },
    {"btencry",             1,     0,   512,   128 },
    {"tws",                 5,     0,   512,   128 },
#if (BT_FOR_APP_EN)
    {"btstack",             3,     0,   1024,  256 },
#else
    {"btstack",             3,     0,   768,   256 },
#endif
    {"audio_dec",           5,     0,   800,   128 },
    {"aud_effect",          5,     1,   800,   128 },
    /*
     *为了防止dac buf太大，通话一开始一直解码，
     *导致编码输入数据需要很大的缓存，这里提高编码的优先级
     */
    {"audio_enc",           6,     0,   768,   128 },
    {"aec",					2,	   1,   768,   128 },
#if TCFG_AUDIO_HEARING_AID_ENABLE
    {"HearingAid",			6,	   0,   768,   128   },
#endif/*TCFG_AUDIO_HEARING_AID_ENABLE*/
#ifdef CONFIG_BOARD_AISPEECH_NR
    {"aispeech_enc",		2,	   1,   512,   128 },
#endif /*CONFIG_BOARD_AISPEECH_NR*/
#ifdef CONFIG_BOARD_AISPEECH_VAD_ASR
    {"asr",                 1,     0,   768,   128 },
    {"audio_asr_export_task",  1,     0,   512,   128 },
#endif/*CONFIG_BOARD_AISPEECH_VAD_ASR*/
#ifndef CONFIG_256K_FLASH
    {"aec_dbg",				3,	   0,   128,   128 },

#if AUDIO_ENC_MPT_SELF_ENABLE
    {"enc_mpt_self",		3,	   0,   512,   128 },
#endif/*AUDIO_ENC_MPT_SELF_ENABLE*/
    {"update",				1,	   0,   256,   0   },
    {"tws_ota",				2,	   0,   256,   0   },
    {"tws_ota_msg",			2,	   0,   256,   128 },
    {"dw_update",		 	2,	   0,   256,   128 },
    {"rcsp_task",		    2,	   0,   640,   128 },
    {"aud_capture",         4,     0,   512,   256 },
    {"data_export",         5,     0,   512,   256 },
    {"anc",                 3,     1,   512,   128 },
#endif

#if TCFG_GX8002_NPU_ENABLE
    {"gx8002",              2,     0,   256,   64  },
#endif /* #if TCFG_GX8002_NPU_ENABLE */
#if TCFG_GX8002_ENC_ENABLE
    {"gx8002_enc",          2,     0,   128,   64  },
#endif /* #if TCFG_GX8002_ENC_ENABLE */


#if TCFG_KWS_VOICE_RECOGNITION_ENABLE
    {"kws",                 2,     0,   256,   64  },
#endif /* #if TCFG_KWS_VOICE_RECOGNITION_ENABLE */
    {"usb_msd",           	1,     0,   512,   128 },
#if !TCFG_USB_MIC_CVP_ENABLE
    {"usbmic_write",       	2,     0,   256,   128 },
#endif
#if AI_APP_PROTOCOL
    {"app_proto",           2,     0,   768,   64  },
#endif
#if (TCFG_SPI_LCD_ENABLE||TCFG_SIMPLE_LCD_ENABLE)
    {"ui",           	    2,     0,   768,   256 },
#else
    {"ui",                  3,     0,   384 - 64,  128  },
#endif
#if (TCFG_DEV_MANAGER_ENABLE)
    {"dev_mg",           	3,     0,   512,   512 },
#endif
    {"audio_vad",           1,     1,   512,   128 },
#if TCFG_KEY_TONE_EN
    {"key_tone",            5,     0,   256,   32  },
#endif
#if (TCFG_WIRELESS_MIC_ENABLE)
    {"wl_mic_enc",          2,     0,   768,   128 },
#endif
#if (TUYA_DEMO_EN)
    {"user_deal",           7,     0,   512,   512 },//定义线程 tuya任务调度
    {"dw_update",           2,     0,   256,   128 },
#endif
#if TCFG_AUDIO_SPATIAL_EFFECT_ENABLE
    {"imu_trim",            1,     0,   256,   128 },
#endif /*TCFG_AUDIO_SPATIAL_EFFECT_ENABLE*/
#if TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN
    {"speak_to_chat",       2,     0,   256,   128 },
    {"icsd_adt",            2,     0,   512,   128 },
    {"icsd_src",            2,     1,   512,   128 },
#endif /*TCFG_AUDIO_ANC_ACOUSTIC_DETECTOR_EN*/
    {"pmu_task",            6,      0,  256,   128  },
    {"WindDetect",          2,      0,  256,   128  },

    {"pca",                 1,      0,  256,   128  },
    {"vad_task",            1,      0,  256,   128  },
    {"pa7_key_polling",     7,      0,  256,   128  },
    {0, 0},
};


APP_VAR app_var;

#if (TCFG_W25Q128_ENABLE && TCFG_W25Q128_COUNTER_TEST_ENABLE)
static void w25q128_counter_test_task(void *priv);
#endif

static void w25q128_boot_probe(void)
{
#if TCFG_W25Q128_ENABLE
    printf("\n>>> Initializing W25Q128 Flash...\n");
    if (w25q128_init() == 0) {
        printf(">>> W25Q128 Flash initialized successfully!\n");
#if TCFG_W25Q128_BOOT_TEST_ENABLE
        printf(">>> Starting W25Q128 read/write test...\n");
        w25q128_test();
#endif
#if TCFG_W25Q128_COUNTER_TEST_ENABLE
        int task_ret = task_create(w25q128_counter_test_task, NULL, "w25q_cnt");
        if (task_ret == 0) {
            printf(">>> W25Q128 counter test task created\n");
        } else {
            printf(">>> W25Q128 counter test task create failed! ret=%d\n", task_ret);
        }
#endif
    } else {
        printf(">>> W25Q128 Flash initialization FAILED!\n");
    }
#endif
}

#if (TCFG_W25Q128_ENABLE && TCFG_W25Q128_COUNTER_TEST_ENABLE)
#define W25Q128_COUNTER_TEST_ADDR       0x001000
#define W25Q128_COUNTER_RECORD_MAGIC    0x57323551
#define W25Q128_COUNTER_RECORD_SIZE     12
#define W25Q128_COUNTER_RECORDS_MAX     (W25Q128_SECTOR_SIZE / W25Q128_COUNTER_RECORD_SIZE)

static void w25q128_u32_to_le(u8 *buf, u32 value)
{
    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
}

static u32 w25q128_le_to_u32(const u8 *buf)
{
    return ((u32)buf[0]) |
           ((u32)buf[1] << 8) |
           ((u32)buf[2] << 16) |
           ((u32)buf[3] << 24);
}

static int w25q128_counter_find_tail(u32 *next_index, u32 *next_value)
{
    u8 record[W25Q128_COUNTER_RECORD_SIZE];
    u32 last_value = 0;
    u32 index;

    for (index = 0; index < W25Q128_COUNTER_RECORDS_MAX; index++) {
        u32 addr = W25Q128_COUNTER_TEST_ADDR + index * W25Q128_COUNTER_RECORD_SIZE;

        if (w25q128_read_data(addr, record, sizeof(record)) != 0) {
            return -1;
        }

        u32 magic = w25q128_le_to_u32(&record[0]);
        u32 value = w25q128_le_to_u32(&record[4]);
        u32 value_inv = w25q128_le_to_u32(&record[8]);

        if (magic == 0xffffffff && value == 0xffffffff && value_inv == 0xffffffff) {
            *next_index = index;
            *next_value = (index == 0) ? 1 : (last_value + 1);
            return 0;
        }

        if (magic != W25Q128_COUNTER_RECORD_MAGIC || value_inv != ~value) {
            printf(">>> W25Q128 counter record broken at index=%u, erase test sector\n", index);
            return -2;
        }

        last_value = value;
    }

    *next_index = W25Q128_COUNTER_RECORDS_MAX;
    *next_value = last_value + 1;
    return 0;
}

static int w25q128_counter_write_record(u32 index, u32 value)
{
    u8 write_buf[W25Q128_COUNTER_RECORD_SIZE];
    u8 read_buf[W25Q128_COUNTER_RECORD_SIZE];
    u32 addr = W25Q128_COUNTER_TEST_ADDR + index * W25Q128_COUNTER_RECORD_SIZE;

    w25q128_u32_to_le(&write_buf[0], W25Q128_COUNTER_RECORD_MAGIC);
    w25q128_u32_to_le(&write_buf[4], value);
    w25q128_u32_to_le(&write_buf[8], ~value);

    if (w25q128_write_data(addr, write_buf, sizeof(write_buf)) != 0) {
        printf(">>> W25Q128 counter write failed: index=%u value=%u\n", index, value);
        return -1;
    }

    if (w25q128_read_data(addr, read_buf, sizeof(read_buf)) != 0) {
        printf(">>> W25Q128 counter readback failed: index=%u\n", index);
        return -2;
    }

    if (memcmp(write_buf, read_buf, sizeof(write_buf)) != 0) {
        printf(">>> W25Q128 counter verify mismatch: index=%u value=%u\n", index, value);
        printf(">>> readback: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
               read_buf[0], read_buf[1], read_buf[2], read_buf[3],
               read_buf[4], read_buf[5], read_buf[6], read_buf[7],
               read_buf[8], read_buf[9], read_buf[10], read_buf[11]);
        return -3;
    }

    printf(">>> W25Q128 counter OK: index=%u addr=0x%06x value=%u\n", index, addr, value);
    return 0;
}

static void w25q128_counter_test_task(void *priv)
{
    u32 index = 0;
    u32 value = 1;

    printf(">>> W25Q128 counter append test start, sector=0x%06x\n", W25Q128_COUNTER_TEST_ADDR);

    while (1) {
        int ret = w25q128_counter_find_tail(&index, &value);

        if (ret != 0 || index >= W25Q128_COUNTER_RECORDS_MAX) {
            printf(">>> W25Q128 counter erase sector=0x%06x\n", W25Q128_COUNTER_TEST_ADDR);
            if (w25q128_sector_erase(W25Q128_COUNTER_TEST_ADDR) != 0) {
                printf(">>> W25Q128 counter erase failed, retry later\n");
                os_time_dly(100);
                continue;
            }
            index = 0;
            value = 1;
        }

        w25q128_counter_write_record(index, value);
        os_time_dly(100);
    }
}
#endif

// 定义PA7按键引脚
#define KEY_PA7_PIN    IO_PORTA_07

// PA7按键检测初始化
void pa7_key_init(void)
{
    // 初始化 PA7 为输入模式，开启内部上拉
    gpio_set_direction(KEY_PA7_PIN, 1);  // 设置为输入
    gpio_set_pull_up(KEY_PA7_PIN, 1);    // 开启上拉
    gpio_set_die(KEY_PA7_PIN, 1);        // 数字输入使能

    printf("PA7 key initialized\n");
    log_info("PA7 key initialized\n");
}

// PA7按键检测任务 - 优化的轮询方式
static void pa7_key_polling_task_handle(void *p)
{
    printf("PA7 key task started\n");
    log_info("PA7 key task started\n");

    pa7_key_init();

    u8 last_key_state = 1;  // 上次按键状态，默认为高电平（未按下）
    u8 stable_count = 0;    // 稳定计数器
    u8 key_pressed = 0;     // 按键按下标志

    while (1) {
        u8 current_state = gpio_read(KEY_PA7_PIN);

        // 状态变化检测
        if (current_state != last_key_state) {
            stable_count++;

            // 需要连续5次检测到相同状态才认为稳定（消抖）
            if (stable_count >= 5) {
                if (current_state == 0 && !key_pressed) {
                    // 按键按下
                    key_pressed = 1;
                    printf("Key PA7 Pressed!\n");
                    log_info("Key PA7 Pressed!\n");
                } else if (current_state == 1 && key_pressed) {
                    // 按键释放
                    key_pressed = 0;
                    printf("Key PA7 Released!\n");
                    log_info("Key PA7 Released!\n");
                }
                last_key_state = current_state;
                stable_count = 0;
            }
        } else {
            stable_count = 0;  // 状态稳定，重置计数器
        }

        os_time_dly(5);  // 5ms检测一次，降低CPU占用
    }
}

/*
 * 2ms timer中断回调函数
 */
void timer_2ms_handler()
{

}

void app_var_init(void)
{
    memset((u8 *)&bt_user_priv_var, 0, sizeof(BT_USER_PRIV_VAR));
    app_var.play_poweron_tone = 1;

}



void app_earphone_play_voice_file(const char *name);

void clr_wdt(void);

void check_power_on_key(void)
{
    u32 delay_10ms_cnt = 0;

    while (1) {
        clr_wdt();
        os_time_dly(1);

        extern u8 get_power_on_status(void);
        if (get_power_on_status()) {
            log_info("+");
            delay_10ms_cnt++;
            if (delay_10ms_cnt > 70) {
                /* extern void set_key_poweron_flag(u8 flag); */
                /* set_key_poweron_flag(1); */
                return;
            }
        } else {
            log_info("-");
            delay_10ms_cnt = 0;
            log_info("enter softpoweroff\n");
            power_set_soft_poweroff();
        }
    }
}


extern int cpu_reset_by_soft();
extern int audio_dec_init();
extern int audio_enc_init();



__attribute__((weak))
u8 get_charge_online_flag(void)
{
    return 0;
}

/*充电拔出,CPU软件复位, 不检测按键，直接开机*/
static void app_poweron_check(int update)
{
#if (CONFIG_BT_MODE == BT_NORMAL)
    if (!update && cpu_reset_by_soft()) {
        app_var.play_poweron_tone = 0;
        return;
    }

#if TCFG_CHARGE_OFF_POWERON_NE
    if (is_ldo5v_wakeup()) {
        app_var.play_poweron_tone = 0;
        return;
    }
#endif
//#ifdef CONFIG_RELEASE_ENABLE
#if TCFG_POWER_ON_NEED_KEY
    check_power_on_key();
#endif
//#endif

#endif
}


bool bone_task_is_open = false;
bool pca_task_is_open = false;


extern u32 timer_get_ms(void);
extern int bt_modify_name(u8 *new_name);
void app_main()
{
    int update = 0;
    u32 addr = 0, size = 0;
    struct intent it;


    log_info("app_main\n");
    log_info("w0x7ce_fix_01_app_main\n");
    app_var.start_time = timer_get_ms();

#if (defined(CONFIG_MEDIA_NEW_ENABLE) || (defined(CONFIG_MEDIA_DEVELOP_ENABLE)))
    /*解码器*/
    audio_enc_init();
    audio_dec_init();
#endif


#ifdef BT_DUT_INTERFERE
    void audio_demo(void);
    audio_demo();
#endif/*BT_DUT_INTERFERE*/
#ifdef BT_DUT_ADC_INTERFERE
    void audio_adc_mic_dut_open(void);
    audio_adc_mic_dut_open();
#endif/*BT_DUT_ADC_INTERFERE*/

    if (!UPDATE_SUPPORT_DEV_IS_NULL()) {
        update = update_result_deal();
    }

    app_var_init();

#if TCFG_MC_BIAS_AUTO_ADJUST
    mc_trim_init(update);
#endif/*TCFG_MC_BIAS_AUTO_ADJUST*/

    if (get_charge_online_flag()) {

#if(TCFG_SYS_LVD_EN == 1)
        vbat_check_init();
#endif

        init_intent(&it);
        it.name = "idle";
        it.action = ACTION_IDLE_MAIN;
        start_app(&it);
    } else {
        check_power_on_voltage();

        app_poweron_check(update);

        ui_manage_init();
        ui_update_status(STATUS_POWERON);

#if TCFG_WIRELESS_MIC_ENABLE
        extern void wireless_mic_main_run(void);
        wireless_mic_main_run();
#endif

#if  TCFG_ENTER_PC_MODE
        init_intent(&it);
        it.name = "pc";
        it.action = ACTION_PC_MAIN;
        start_app(&it);
#else
        init_intent(&it);
        it.name = "earphone";
        it.action = ACTION_EARPHONE_MAIN;
        start_app(&it);
#endif
    }

#if TCFG_CHARGE_ENABLE
    set_charge_event_flag(1);
#endif

    bt_modify_name("Hera");
    if (!pca_task_is_open) {
        pca_open();
        pca_task_is_open = true;
    }
    if (!bone_task_is_open)
    {
        bone_task_init();
        bone_task_is_open = true;
    }

    // 创建 PA7 按键检测任务
    int task_ret = task_create(pa7_key_polling_task_handle, NULL, "pa7_key_polling");
    if (task_ret == 0) {
        printf("PA7 key task created successfully\n");
    } else {
        printf("PA7 key task create failed! ret=%d\n", task_ret);
    }

    w25q128_boot_probe();
}

int __attribute__((weak)) eSystemConfirmStopStatus(void)
{
    /* 系统进入在未来时间里，无任务超时唤醒，可根据用户选择系统停止，或者系统定时唤醒(100ms)，或自己指定唤醒时间 */
    //1:Endless Sleep
    //0:100 ms wakeup
    //other: x ms wakeup
    if (get_charge_full_flag()) {
        /* log_i("Endless Sleep"); */
        power_set_soft_poweroff();
        return 1;
    } else {
        /* log_i("100 ms wakeup"); */
        return 0;
    }
}

__attribute__((used)) int *__errno()
{
    static int err;
    return &err;
}

enum {
    KEY_USER_DEAL_POST = 0,
    KEY_USER_DEAL_POST_MSG,
    KEY_USER_DEAL_POST_EVENT,
    KEY_USER_DEAL_POST_2,
};

#include "system/includes.h"
#include "system/event.h"

///自定义事件推送的线程

#define Q_USER_DEAL   0xAABBCC ///自定义队列类型
#define Q_USER_DATA_SIZE  10///理论Queue受任务声明struct task_info.qsize限制,但不宜过大,建议<=6

void user_deal_send_ver(void)
{
    //os_taskq_post("user_deal", 1,KEY_USER_DEAL_POST);
    os_taskq_post_msg("user_deal", 1, KEY_USER_DEAL_POST_MSG);
    //os_taskq_post_event("user_deal",1, KEY_USER_DEAL_POST_EVENT);
}

void user_deal_rand_set(u32 rand)
{
    os_taskq_post("user_deal", 2, KEY_USER_DEAL_POST_2, rand);
}

void user_deal_send_array(int *msg, int argc)
{
    if (argc > Q_USER_DATA_SIZE) {
        return;
    }
    os_taskq_post_type("user_deal", Q_USER_DEAL, argc, msg);
}
void user_deal_send_msg(void)
{
    os_taskq_post_event("user_deal", 1, KEY_USER_DEAL_POST_EVENT);
}

void user_deal_send_test(void)///模拟测试函数,可按键触发调用，自行看打印
{
    user_deal_send_ver();
    user_deal_rand_set(0x11223344);
    static u32 data[Q_USER_DATA_SIZE] = {0x11223344, 0x55667788, 0x11223344, 0x55667788, 0x11223344,
                                         0xff223344, 0x556677ee, 0x11223344, 0x556677dd, 0x112233ff,
                                        };
    user_deal_send_array(data, sizeof(data) / sizeof(int));
}

static void user_deal_task_handle(void *p)
{
    int msg[Q_USER_DATA_SIZE + 1] = {0, 0, 0, 0, 0, 0, 0, 0, 00, 0};
    int res = 0;
    while (1) {
        res = os_taskq_pend("taskq", msg, ARRAY_SIZE(msg));
        if (res != OS_TASKQ) {
            continue;
        }
        r_printf("user_deal_task_handle:0x%x", msg[0]);
        put_buf(msg, (Q_USER_DATA_SIZE + 1) * 4);
        if (msg[0] == Q_MSG) {
            printf("use os_taskq_post_msg");
            switch (msg[1]) {
            case KEY_USER_DEAL_POST_MSG:
                printf("KEY_USER_DEAL_POST_MSG");
                break;
            default:
                break;
            }
        } else if (msg[0] == Q_EVENT) {
            printf("use os_taskq_post_event");
            switch (msg[1]) {
            case KEY_USER_DEAL_POST_EVENT:
                printf("KEY_USER_DEAL_POST_EVENT");
                break;
            default:
                break;
            }
        } else if (msg[0] == Q_CALLBACK) {
        } else if (msg[0] == Q_USER) {
            printf("use os_taskq_post");
            switch (msg[1]) {
            case KEY_USER_DEAL_POST:
                printf("KEY_USER_DEAL_POST");
                break;
            case KEY_USER_DEAL_POST_2:
                printf("KEY_USER_DEAL_POST_2:0x%x", msg[2]);
                break;
            default:
                break;
            }
        } else if (msg[0] == Q_USER_DEAL) {
            printf("use os_taskq_post_type");
            printf("0x%x 0x%x 0x%x 0x%x 0x%x", msg[1], msg[2], msg[3], msg[4], msg[5]);
            printf("0x%x 0x%x 0x%x 0x%x 0x%x", msg[6], msg[7], msg[8], msg[9], msg[10]);
        }
        puts("");
    }
}

void user_deal_init(void)
{
    task_create(user_deal_task_handle, NULL, "user_deal");
}

void user_deal_exit(void)
{
    task_kill("user_deal");
}

