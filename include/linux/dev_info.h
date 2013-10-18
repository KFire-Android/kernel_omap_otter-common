#ifndef __BOWSER_DEV_INFO__
#define __BOWSER_DEV_INFO__

struct dev_info_platform_data {
	char *dev_model;
};

void devinfo_set_emmc_id(u8 id);
void devinfo_set_emmc_size(u32 sectors);
void devinfo_set_emif(int id);

#endif /* __BOWSER_DEV_INFO__ */
