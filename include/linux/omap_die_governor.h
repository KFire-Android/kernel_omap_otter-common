#include <linux/types.h>

#define OMAP_THERMAL_ZONE_NAME_SZ	10

/**
 * struct omap_thermal_zone - Structure to describe temperature zone
 * @name: Thermal zone name
 * @cooling_increment: cooling action increase level in this zone
 * @temp_lower: lower temperature threshold for this zone (includes hysteresis)
 * @temp_upper: higher temperature threshold for this thermal zone
 * @update_rate: temperature sensor update rate
 * @average_rate: rate of average work
 * @max_trend: max temperature trend allowed for zone
 */
struct omap_thermal_zone {
	char name[OMAP_THERMAL_ZONE_NAME_SZ];
	unsigned int cooling_increment;
	int temp_lower;
	int temp_upper;
	int update_rate;
	int average_rate;
	int max_trend;
};
#define OMAP_THERMAL_ZONE(n, i, l, u, r, a, t)		\
{							\
	.name				= n,		\
	.cooling_increment		= (i),		\
	.temp_lower			= (l),		\
	.temp_upper			= (u),		\
	.update_rate			= (r),		\
	.average_rate			= (a),		\
	.max_trend			= (t),		\
}

/**
 * struct omap_die_governor_pdata - platform data for omap_die governor
 * zones: thremal zones array
 * zones_num: number of thermal zones
 */
struct omap_die_governor_pdata {
	struct omap_thermal_zone *zones;
	int zones_num;
};

#ifdef CONFIG_OMAP_DIE_GOVERNOR
void omap_die_governor_register_pdata(struct omap_die_governor_pdata *omap_gov);
#else
static inline void omap_die_governor_register_pdata(struct
					omap_die_governor_pdata * omap_gov)
{
}
#endif

