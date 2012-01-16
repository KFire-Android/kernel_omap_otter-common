#include "smb347.h"

static enum power_supply_property summit_usb_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
};

static enum power_supply_property summit_ac_props[] = {
    POWER_SUPPLY_PROP_ONLINE,
};

static int summit_ac_get_property(struct power_supply *psy,
                    enum power_supply_property psp,
                    union power_supply_propval *val)
{
    struct summit_smb347_info *di = container_of(psy,struct summit_smb347_info, ac);

    switch (psp) {
    	case POWER_SUPPLY_PROP_ONLINE:
    		val->intval = di->ac_online;
    	break;
    	default:
    		return -EINVAL;
    }

    return 0;
}
static int summit_usb_get_property(struct power_supply *psy,
                    enum power_supply_property psp,
                    union power_supply_propval *val)
{
    struct summit_smb347_info *di = container_of(psy,struct summit_smb347_info, usb);

    switch (psp) {
    	case POWER_SUPPLY_PROP_ONLINE:
    		val->intval = di->usb_online;
    	break;
    	default:
    		return -EINVAL;
    }

    return 0;
}

void create_summit_powersupplyfs(struct summit_smb347_info *di)
{
    int ret;

    di->usb.name = "usb";
    di->usb.type = POWER_SUPPLY_TYPE_USB;
    di->usb.properties = summit_usb_props;
    di->usb.num_properties = ARRAY_SIZE(summit_usb_props);
    di->usb.get_property = summit_usb_get_property;

    di->ac.name = "ac";
    di->ac.type = POWER_SUPPLY_TYPE_MAINS;
    di->ac.properties = summit_ac_props;
    di->ac.num_properties = ARRAY_SIZE(summit_ac_props);
    di->ac.get_property = summit_ac_get_property;

    ret = power_supply_register(di->dev, &di->usb);
    if (ret) {
    	dev_dbg(di->dev,"failed to register usb power supply\n");
    	//goto usb_failed;
    }

    ret = power_supply_register(di->dev, &di->ac);
    if (ret) {
    	dev_dbg(di->dev,"failed to register ac power supply\n");
    	//goto ac_failed;
    }
}
void remove_summit_powersupplyfs(struct summit_smb347_info *di)
{
    power_supply_unregister(&di->ac);
    power_supply_unregister(&di->usb);
}
