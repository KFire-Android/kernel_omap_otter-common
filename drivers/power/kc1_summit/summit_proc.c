#include <linux/i2c/twl.h>
#include "smb347.h"
#include <linux/regulator/consumer.h>
#define CONTROL_DEV_CONF        0x300
#define PHY_PD                  0x1

#ifdef    CONFIG_PROC_FS
#define   SUMMIT_PROC_FILE	"driver/summit"
#define   SUMMIT_MAJOR          618

static u8 index=0xe;

static ssize_t summit_proc_read(char *page, char **start, off_t off, int count,
    		 int *eof, void *data)
{
    struct summit_smb347_info *di=data;
    //printk(KERN_INFO "%s\n",__func__);

    u32 value=0;
    char	*next = page;
    unsigned size = count;
    int t;

    if (off != 0)
    	return 0;
    //you can read value from the register of hardwae in here
    value =i2c_smbus_read_byte_data(di->client,index);
    if (value < 0) {
        printk(KERN_INFO"error reading index=0x%x value=0x%x\n",index,value);
        return value;
    }
    t = scnprintf(next, size, "%d",value);
    size -= t;
    next += t;
    printk(KERN_INFO"%s:register 0x%x: return value 0x%x\n",__func__, index, value);
    *eof = 1;
    return count - size;
}

#define STS_HW_CONDITIONS       0x21
#define STS_USB_ID          (1 << 2)


static ssize_t summit_proc_write(struct file *filp, 
    	const char *buff,unsigned long len, void *data)
{
    struct summit_smb347_info *di=data;
    struct regulator *p_regulator = regulator_get(NULL, "usb-phy");
    //printk(KERN_INFO "%s\n",__func__);
    u32 reg_val,value;
//    void __iomem *addr = NULL;
    int event  = USB_EVENT_VBUS;
    struct clk *phyclk;
    struct clk *clk48m;
    struct clk *clk32k;
    int hw_state=0;
    char messages[256], vol[256];
    phyclk = clk_get(NULL, "ocp2scp_usb_phy_ick");
    if (IS_ERR(phyclk)) {
        pr_warning("cannot clk_get ocp2scp_usb_phy_ick\n");
        return PTR_ERR(phyclk);
    }

    clk48m = clk_get(NULL, "ocp2scp_usb_phy_phy_48m");
    if (IS_ERR(clk48m)) {
        pr_warning("cannot clk_get ocp2scp_usb_phy_phy_48m\n");
        clk_put(phyclk);
        return PTR_ERR(clk48m);
    }

    clk32k = clk_get(NULL, "usb_phy_cm_clk32k");
    if (IS_ERR(clk32k)) {
        pr_warning("cannot clk_get usb_phy_cm_clk32k\n");
        clk_put(phyclk);
        clk_put(clk48m);
        return PTR_ERR(clk32k);
    }
    if (len > 256)
    	len = 256;

    if (copy_from_user(messages, buff, len))
    	return -EFAULT;
    
    if ('-' == messages[0]) {
    	/* set the register index */
    	memcpy(vol, messages+1, len-1);
    	index = (int) simple_strtoul(vol, NULL, 16);
    	printk(KERN_INFO"%s:set register 0x%x\n",__func__, index);
    }else if('+'==messages[0]) {
    	// set the register value 
    	memcpy(vol, messages+1, len-1);
    	reg_val = (int)simple_strtoul(vol, NULL, 16);
    	//you can read value from the register of hardwae in here
    	i2c_smbus_write_byte_data(di->client, index, reg_val);
    	printk(KERN_INFO"%s:register 0x%x: set value 0x%x\n",__func__, index, reg_val);
    }else if ('!' == messages[0]) {
        switch(messages[1]){
            case 'a':
                //gpio_request(119, "ADO_SPK_ENABLE");
                //gpio_direction_output(119, 0);           
    value =i2c_smbus_read_byte_data(di->client,index);
    if (value < 0) {
        printk(KERN_INFO"error reading index=0x%x value=0x%x\n",index,value);
    }
    printk(KERN_INFO"%s:register 0x%x: return value 0x%x di->mbid=%d  protect_enable=%d di->protect_event=0x%x\n",__func__, index, value,di->mbid,di->protect_enable,di->protect_event);
            break;
            case 'b':
                 //regulator_disable(p_regulator);
            # if 0
                twl_i2c_read_u8(TWL6030_MODULE_ID0, &hw_state, STS_HW_CONDITIONS);
                printk("hw_state=0x%x\n",hw_state);
		if(hw_state & STS_USB_ID){
                     printk("STS_USB_ID\n");
		}else{
                     printk("NO STS_USB_ID\n");
		}            
                command=i2c_smbus_read_byte_data(di->client,COMMAND_A);
                printk("COMMAND_A=0x%x\n",command);
                #endif
                //printk("enable clock\n");
                //clk_enable(phyclk);
                //clk_enable(clk48m);
                //clk_enable(clk32k);
                printk("phy power up");
                __raw_writel(~PHY_PD, OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x2300)));
                //summit_config_charge_current(di,FCC_2000mA,CONFIG_NO_CHANGE,CONFIG_NO_CHANGE);
                //charge_enable(di,1);
            break;
            case 'c':
                printk("disable clock\n");
                clk_disable(phyclk);
                clk_disable(clk48m);
                clk_disable(clk32k);
                printk("phy power down");
                __raw_writel(PHY_PD, OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x2300)));
                //charge_enable(di,0);
                //di->event=EVENT_CHANGE_TO_ONDEMAND;
                //printk("%s event=%d %d \n",__func__,di->event,EVENT_CHANGE_TO_ONDEMAND);
                //fsm_stateTransform(di);
                //fsm_doAction(di);    
            break;
/*
enable
ocp2scp_usb_phy_ick,ocp2scp_usb_phy_phy_48m,	
	->4A0093E0 =0x101	
usb_phy_cm_clk32k
	->4A00 8640 =0x100
disable
	->4A0093E0 =0x30100
	->4A00 8640 =0x0
*/
            case 'd':
                //di->event=EVENT_CHANGE_TO_INTERNAL_FSM;
                //printk("%s event=%d %d \n",__func__,di->event,EVENT_CHANGE_TO_ONDEMAND);
                //fsm_stateTransform(di);
                //fsm_doAction(di);
                printk("phy power up");
                __raw_writel(0x101, OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x93E0)));
                __raw_writel(0x100, OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x8640)));
                __raw_writel(~PHY_PD, OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x2300)));
                //switch_mode(di,USB1_MODE);
            break;
            case 'e':
                printk("phy power down");
                __raw_writel(0x30100, OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x93E0)));
                __raw_writel(0x0, OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x8640)));
                __raw_writel(PHY_PD, OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x2300)));
                //switch_mode(di,USB5_MODE);
            break;
            case 'f':
                value=__raw_readl(OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x4120)));
                printk("%s : CM_CLKMODE_DPLL_CORE=0x%x\n",__func__,value);
                value=__raw_readl(OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x93E0)));
                printk("%s : CM_L3INIT_USBPHY_CLKCTRL=0x%x\n",__func__,value);
                value=__raw_readl(OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x8640)));
                printk("%s : CM_ALWON_USBPHY_CLKCTRL=0x%x\n",__func__,value);
                value=__raw_readl(OMAP2_L4_IO_ADDRESS(0x4a000000 + (0x2300)));
                printk("%s : CONTROL_DEV_CONF=0x%x\n",__func__,value);
                //switch_mode(di,HC_MODE);
                value=__raw_readl(OMAP2_L4_IO_ADDRESS(0x4a100000 + (0x620)));
                printk("%s : usb2phy=0x%x\n",__func__,value);
            break;
            case 'g':
                twl_i2c_write_u8(TWL6030_MODULE_ID0,  0x8 ,0xe5);
                twl_i2c_write_u8(TWL6030_MODULE_ID0,  0x18 ,0xa3);
                twl_i2c_write_u8(TWL6030_MODULE_ID0,  0x3f ,0xa1);
                twl_i2c_write_u8(TWL6030_MODULE_ID0,  0xe1 ,0xa2);
            # if 0
                summit_charge_enable(di,0);
                summit_charge_enable(di,1);
                mdelay(10);
                summit_check_bmd(di);
                //charge_enable(di,0);	
            #endif
            break;
            case '1':
                //summit_switch_mode(di,USB1_OR_15_MODE);
                summit_config_suspend_by_register(di,1);
            break;
            case '2':
                //summit_switch_mode(di,USB5_OR_9_MODE);
                summit_config_suspend_by_register(di,0);
            break;
            case '3':
                gpio_set_value(di->pin_susp, 0);printk(KERN_INFO "di->pin_susp=%d\n",di->pin_susp);
            break;
            case '4':
                gpio_set_value(di->pin_susp, 1);printk(KERN_INFO "di->pin_susp=%d\n",di->pin_susp);
            break;
            case '5':
                gpio_set_value(di->pin_en, 0);printk(KERN_INFO "di->pin_en=%d\n",di->pin_en);
            break;
            case '6':
                gpio_set_value(di->pin_en, 1);printk(KERN_INFO "di->pin_en=%d\n",di->pin_en);
                //summit_config_apsd(di,1);
            break;
            case '7':
                summit_config_aicl(di,0,4200);
                //summit_config_apsd(di,0);
            break;
            case '8':
                blocking_notifier_call_chain(&di->xceiv->notifier,USB_EVENT_NONE, di->xceiv->gadget);
                //summit_suspend_mode(di,1);
            break;
            case '9':
                //printk("power_source=%d\n",di->power_source);
                //di->power_source=4;
                summit_charger_reconfig(di);
                
            break;
    	}
    }
    regulator_put(p_regulator);
    return len;
}

void create_summit_procfs(struct summit_smb347_info *di)
{
    printk(KERN_INFO "%s\n",__func__);
    di->summit_proc_fs = create_proc_entry(SUMMIT_PROC_FILE, SUMMIT_MAJOR, NULL);
    if (di->summit_proc_fs) {
    	di->summit_proc_fs->data = di; 
    	di->summit_proc_fs->read_proc = summit_proc_read; 
    	di->summit_proc_fs->write_proc = summit_proc_write; 
    } else
    	printk(KERN_ERR "%sproc file create failed!\n",__func__);
}

void remove_summit_procfs(void)
{
    printk(KERN_INFO "%s\n",__func__);
    remove_proc_entry(SUMMIT_PROC_FILE, NULL);
}
#endif
