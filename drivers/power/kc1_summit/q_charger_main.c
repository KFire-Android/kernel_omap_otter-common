/* Version 1.0 2011/4/15
 * 
 */

#include <linux/init.h>
#include <linux/module.h>

#include <linux/interrupt.h>
#include <linux/delay.h>

#include <linux/slab.h>
#include <linux/i2c/twl.h>

#include <linux/i2c/twl6030-gpadc.h>
#include <linux/wakelock.h>


#include "q_charger.h"


//static struct blocking_notifier_head notifier_list;
//extern u32 wakeup_timer_seconds;
static struct wake_lock chrg_lock;
static int sdp4430_batt_table[] = {
    /* adc code for temperature in degree C */
    929, 925, /* -2 ,-1 */
    920, 917, 912, 908, 904, 899, 895, 890, 885, 880, /* 00 - 09 */
    875, 869, 864, 858, 853, 847, 841, 835, 829, 823, /* 10 - 19 */
    816, 810, 804, 797, 790, 783, 776, 769, 762, 755, /* 20 - 29 */
    748, 740, 732, 725, 718, 710, 703, 695, 687, 679, /* 30 - 39 */
    671, 663, 655, 647, 639, 631, 623, 615, 607, 599, /* 40 - 49 */
    591, 583, 575, 567, 559, 551, 543, 535, 527, 519, /* 50 - 59 */
    511, 504, 496 /* 60 - 62 */
};

/*
 * Setup the twl6030 BCI module to measure battery
 * temperature
 */
int twl6030battery_temp_setup(void)
{
    int ret;
    u8 rd_reg = 0;

    ret = twl_i2c_read_u8(TWL_MODULE_MADC, &rd_reg, TWL6030_GPADC_CTRL);
    rd_reg |= GPADC_CTRL_TEMP1_EN | GPADC_CTRL_TEMP2_EN |
        GPADC_CTRL_TEMP1_EN_MONITOR | GPADC_CTRL_TEMP2_EN_MONITOR |
        GPADC_CTRL_SCALER_DIV4;
    ret |= twl_i2c_write_u8(TWL_MODULE_MADC, rd_reg, TWL6030_GPADC_CTRL);

    return ret;
}
static irqreturn_t q_charger_ctrl_interrupt(int irq, void *_di)
{
    struct q_charger_driver_info *di = _di;
    u8 present_charge_state = 0;
    twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &present_charge_state,CONTROLLER_STAT1);
    if(present_charge_state & VBUS_DET){
        dev_dbg(di->dev,"%s VBUS_DET\n",__func__);
    }else{
        dev_dbg(di->dev,"%s NO VBUS\n",__func__);
    }
    return IRQ_HANDLED;
}
static irqreturn_t q_summit_charger_interrupt(int irq, void *_di)
{
    struct q_charger_driver_info *di = _di;
    dev_dbg(di->dev,"%s\n",__func__);
    summit_read_all_interrupt_status(di);
    return IRQ_HANDLED;
}

static int __devinit q_charger_probe(struct platform_device *pdev)
{
    struct q_charger_platform_data *pdata = pdev->dev.platform_data;
    struct q_charger_driver_info *di;
    struct i2c_adapter *adap ;
    int irq;
    int ret;
    int status;
    //struct otg_transceiver *xceiv;
    u8 controller_stat = 0;

    u8 hw_state = 0;
   
    dev_dbg(di->dev,KERN_INFO "%s\n",__func__);

    di = kzalloc(sizeof(*di), GFP_KERNEL);
    if (!di)
        return -ENOMEM;
    if (!pdata) {
        dev_dbg(&pdev->dev, "platform_data not available\n");
        ret = -EINVAL;
        goto err_pdata;
    }
    di->dev = &pdev->dev;
    di->capacity=1;
    di->voltage=0;
    di->bat_online=-1;
    di->power_source=NO_USB;
    di->usb_notifier.notifier_call=summit_usb_notifier_call;
    di->tblsize=ARRAY_SIZE(sdp4430_batt_table);
    di->battery_tmp_tbl = sdp4430_batt_table;
    di->usb_phy = ioremap(0x4a100000,0x800);
    
    /* request charger ctrl interruption */
    irq = platform_get_irq(pdev, 0);
    dev_dbg(di->dev,"irq[0]=%d\n",irq);
    dev_dbg(di->dev,"irq[1]=%d\n",platform_get_irq(pdev, 1));
    ret = request_threaded_irq(irq, NULL, q_charger_ctrl_interrupt,
        0, "twl_bci_ctrl", di);
    if (ret) {
        dev_dbg(&pdev->dev, "could not request irq %d, status %d\n",
            irq, ret);
        //goto chg_irq_fail;
    }
    irq = platform_get_irq(pdev, 1);
    ret = request_threaded_irq(irq, NULL, q_summit_charger_interrupt,
        IRQF_TRIGGER_FALLING, "charger_isr", di);
    if (ret) {
        dev_dbg(&pdev->dev, "could not request irq %d, status %d\n",
            irq, ret);
        //goto chg_irq_fail;
    }
    platform_set_drvdata(pdev, di);
    
    create_q_powersupply(di);

    adap = i2c_get_adapter(1);
        di->charger_client = i2c_new_dummy(adap,0x06);
    if (!di->charger_client) {
        dev_err(&pdev->dev,
            "can't attach client %d\n");
        //status = -ENOMEM;
        //goto fail;
    }
    di->bat_client = i2c_new_dummy(adap,0x55);
    if (!di->bat_client) {
        dev_err(&pdev->dev,
            "can't attach client %d\n");
        //status = -ENOMEM;
        //goto fail;
    }

    //wake_lock_init(&chrg_lock, WAKE_LOCK_SUSPEND, "ac_chrg_wake_lock");
    INIT_DELAYED_WORK_DEFERRABLE(&di->bat_monitor_work,
                bat_monitor_work_func);
    INIT_DELAYED_WORK_DEFERRABLE(&di->ac_monitor_work,
                ac_monitor_work_func);

    /* settings for temperature sensing */
    //ret = twl6030battery_temp_setup();
    //if (ret)
        //goto temp_setup_fail;
    //q_charger_init(di);

    twl_i2c_read_u8(TWL6030_MODULE_CHARGER, &controller_stat,
        CONTROLLER_STAT1);
    if (controller_stat & VBUS_DET) {
        di->power_source=get_power_source();
        switch(get_power_source()){
            case 1:
                di->power_source=OTHER_CHARGING_PORT;
                di->ac_online=1;
            break;
            case 4:
            case 5:
                di->ac_online=1;
                di->power_source=DEDICATED_CHARGING_PORT;
            break;
            case 6:
                di->usb_online=1;
                di->power_source=STANDARD_DOWNSTREAM_PORT;
            break;
        }
        schedule_work(&di->ac_monitor_work); 
    }
    //wake_lock_init(&chrg_lock, WAKE_LOCK_SUSPEND, "pm_wake_lock");
    //wake_lock(&chrg_lock);
    
    di->xceiv=otg_get_transceiver();
    di->xceiv->set_power=summit_set_input_current_limit;
    status = otg_register_notifier(di->xceiv, &di->usb_notifier);
    
    twl6030_interrupt_unmask(TWL6030_CHARGER_CTRL_INT_MASK,
                        REG_INT_MSK_LINE_C);
    
    schedule_delayed_work(&di->bat_monitor_work,
            msecs_to_jiffies(1000 * 5));
#ifdef    CONFIG_PROC_FS
    create_q_proc_file(di);
#endif
    create_q_sysfs(di);
    return 0;

err_pdata:
    kfree(di);

    return ret;
}

static int __devexit q_charger_remove(struct platform_device *pdev)
{
    struct q_charger_driver_info *di = platform_get_drvdata(pdev);
    int irq;
    dev_dbg(di->dev,KERN_INFO "%s\n",__func__);
    usb_unregister_notify(&di->usb_notifier);
    cancel_delayed_work(&di->bat_monitor_work);
    cancel_delayed_work(&di->ac_monitor_work);
    flush_scheduled_work();
    iounmap(di->usb_phy);
    q_powersupply_exit(di);
    twl6030_interrupt_mask(TWL6030_CHARGER_CTRL_INT_MASK,
                        REG_INT_MSK_LINE_C);
    irq = platform_get_irq(pdev, 0);
    free_irq(irq, di);
    i2c_unregister_device(di->charger_client);
    i2c_unregister_device(di->bat_client);

#ifdef    CONFIG_PROC_FS
    remove_q_proc_file();
#endif
    remove_q_sysfs(di);
    //wake_lock_destroy(&chrg_lock);
    //wake_lock_destroy(&chrg_lock);
    platform_set_drvdata(pdev, NULL);
    kfree(di);

    return 0;
}

#ifdef CONFIG_PM
static int q_charger_suspend(struct platform_device *pdev,
    pm_message_t state)
{
    struct q_charger_device_info *di = platform_get_drvdata(pdev);

    return 0;
}

static int q_charger_resume(struct platform_device *pdev)
{
    struct q_charger_device_info *di = platform_get_drvdata(pdev);

    return 0;
}
#else
#define q_charger_suspend    NULL
#define q_charger_resume    NULL
#endif /* CONFIG_PM */

static struct platform_driver q_charger_driver = {
    .probe        = q_charger_probe,
    .remove        = __devexit_p(q_charger_remove),
    .suspend    = q_charger_suspend,
    .resume        = q_charger_resume,
    .driver        = {
        .name    = "q_charger",
    },
};

static int __init q_charger_main_init(void)
{
    //dev_dbg(di->dev,"%s\n",__func__);

    return platform_driver_register(&q_charger_driver);
}
module_init(q_charger_main_init);

static void __exit q_charger_main_exit(void)
{
    //dev_dbg(di->dev,"%s\n",__func__);

    platform_driver_unregister(&q_charger_driver);
}
module_exit(q_charger_main_exit);

MODULE_AUTHOR("Eric NIen");
MODULE_DESCRIPTION("platform:q_charger");
MODULE_LICENSE("GPL");
