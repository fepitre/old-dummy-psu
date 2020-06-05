#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/power_supply.h>

#include <linux/vermagic.h>

static int dummy_get_psp(struct power_supply *psy,
			 enum power_supply_property psp,
			 union power_supply_propval *val);
static int dummy_set_psp(struct power_supply *psy,
			 enum power_supply_property psp,
			 const union power_supply_propval *val);

#define MAX_KEYLENGTH 256

enum dummy_psu_id {
	DUMMY_AC,
	DUMMY_BATTERY,
	DUMMY_POWER_NUM,
};

static bool module_initialized;

static char ac_name[MAX_KEYLENGTH] = "DUMMY_AC";
static char battery_name[MAX_KEYLENGTH] = "DUMMY_BAT";

static char battery_model_name[MAX_KEYLENGTH] = "Dummy battery";
static char battery_manufacturer[MAX_KEYLENGTH] = "Linux";
static char battery_serial_number[MAX_KEYLENGTH] = UTS_RELEASE;

// Power supply property values
static union power_supply_propval psp_val[] = {
	[POWER_SUPPLY_PROP_ONLINE] = { .intval = 1 },
	[POWER_SUPPLY_PROP_STATUS] = { .intval = POWER_SUPPLY_STATUS_FULL },
	[POWER_SUPPLY_PROP_HEALTH] = { .intval = POWER_SUPPLY_HEALTH_GOOD },
	[POWER_SUPPLY_PROP_PRESENT] = { .intval = 1 },
	[POWER_SUPPLY_PROP_TECHNOLOGY] = { .intval =
						   POWER_SUPPLY_TECHNOLOGY_LION },
	[POWER_SUPPLY_PROP_CAPACITY] = { .intval = 100 },
	[POWER_SUPPLY_PROP_CAPACITY_LEVEL] = { .intval =
						       POWER_SUPPLY_CAPACITY_LEVEL_NORMAL },
	[POWER_SUPPLY_PROP_TEMP] = { .intval = 42 },
	[POWER_SUPPLY_PROP_VOLTAGE_NOW] = { .intval = 10000000 },
	[POWER_SUPPLY_PROP_MODEL_NAME] = { .strval = battery_model_name },
	[POWER_SUPPLY_PROP_MANUFACTURER] = { .strval = battery_manufacturer },
	[POWER_SUPPLY_PROP_SERIAL_NUMBER] = { .strval = battery_serial_number },
};

// AC specific properties
static enum power_supply_property dummy_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

// BATTERY specific properties
static enum power_supply_property dummy_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CHARGE_TYPE,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_VOLTAGE_MAX,
	POWER_SUPPLY_PROP_VOLTAGE_MIN,
	POWER_SUPPLY_PROP_VOLTAGE_MAX_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_MIN_DESIGN,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_POWER_NOW,
	POWER_SUPPLY_PROP_ENERGY_FULL_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_EMPTY_DESIGN,
	POWER_SUPPLY_PROP_ENERGY_FULL,
	POWER_SUPPLY_PROP_ENERGY_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_MODEL_NAME,
	POWER_SUPPLY_PROP_MANUFACTURER,
	POWER_SUPPLY_PROP_SERIAL_NUMBER,
};

/*
 * This function should be included in power_supply.h
 * if upstream accept it.
 */
static inline bool power_supply_is_str_property(enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_MODEL_NAME:
	case POWER_SUPPLY_PROP_MANUFACTURER:
	case POWER_SUPPLY_PROP_SERIAL_NUMBER:
		return 1;
	default:
		break;
	}

	return 0;
}

static struct power_supply *dummy_power_supplies[DUMMY_POWER_NUM];

// Restrict writable properties to only those we consider
static int psp_writable(struct power_supply *psy,
			enum power_supply_property psp)
{
	int i;
	enum power_supply_property *psp_props;
	int psp_num;
	if (!strcmp(psy->desc->name, ac_name)) {
		psp_props = dummy_ac_props;
		psp_num = sizeof(dummy_ac_props) / sizeof(dummy_ac_props[0]);
	} else if (!strcmp(psy->desc->name, battery_name)) {
		psp_props = dummy_battery_props;
		psp_num = sizeof(dummy_battery_props) /
			  sizeof(dummy_battery_props[0]);
	} else {
		return 0;
	}

	for (i = 0; i < psp_num; i++) {
		if (psp_props[i] == psp)
			return 1;
	}
	return 0;
}

static char *dummy_power_ac_supplied_to[] = {
	battery_name,
};

static const struct power_supply_desc dummy_power_desc[] = {
    [DUMMY_AC] =
        {
            .name = ac_name,
            .type = POWER_SUPPLY_TYPE_MAINS,
            .properties = dummy_ac_props,
            .num_properties = ARRAY_SIZE(dummy_ac_props),
            .get_property = dummy_get_psp,
            .set_property = dummy_set_psp,
            .property_is_writeable = psp_writable,
        },
    [DUMMY_BATTERY] = {
        .name = battery_name,
        .type = POWER_SUPPLY_TYPE_BATTERY,
        .properties = dummy_battery_props,
        .num_properties = ARRAY_SIZE(dummy_battery_props),
        .get_property = dummy_get_psp,
        .set_property = dummy_set_psp,
        .property_is_writeable = psp_writable,
    }};

static const struct power_supply_config dummy_power_configs[] = {
	{
		/* DUMMY_AC */
		.supplied_to = dummy_power_ac_supplied_to,
		.num_supplicants = ARRAY_SIZE(dummy_power_ac_supplied_to),
	},
	{
		/* DUMMY_BATTERY */
	}
};

static int __init dummy_power_init(void)
{
	int i;
	int ret;

	BUILD_BUG_ON(DUMMY_POWER_NUM != ARRAY_SIZE(dummy_power_supplies));
	BUILD_BUG_ON(DUMMY_POWER_NUM != ARRAY_SIZE(dummy_power_configs));

	for (i = 0; i < ARRAY_SIZE(dummy_power_supplies); i++) {
		dummy_power_supplies[i] = power_supply_register(
			NULL, &dummy_power_desc[i], &dummy_power_configs[i]);
		if (IS_ERR(dummy_power_supplies[i])) {
			pr_err("%s: failed to register %s\n", __func__,
			       dummy_power_desc[i].name);
			ret = PTR_ERR(dummy_power_supplies[i]);
			goto failed;
		}
	}

	module_initialized = true;
	return 0;
failed:
	while (--i >= 0)
		power_supply_unregister(dummy_power_supplies[i]);
	return ret;
}
module_init(dummy_power_init);

static void __exit dummy_power_exit(void)
{
	int i;
	psp_val[POWER_SUPPLY_PROP_ONLINE].intval = 0;
	psp_val[POWER_SUPPLY_PROP_STATUS].intval =
		POWER_SUPPLY_STATUS_DISCHARGING;
	for (i = 0; i < ARRAY_SIZE(dummy_power_supplies); i++)
		power_supply_changed(dummy_power_supplies[i]);
	pr_info("%s: 'changed' event sent, sleeping for 5 seconds...\n",
		__func__);
	ssleep(3);

	for (i = 0; i < ARRAY_SIZE(dummy_power_supplies); i++)
		power_supply_unregister(dummy_power_supplies[i]);

	module_initialized = false;
}
module_exit(dummy_power_exit);

static inline void signal_power_supply_changed(struct power_supply *psy)
{
	if (module_initialized)
		power_supply_changed(psy);
}

static int dummy_get_psp(struct power_supply *psy,
			 enum power_supply_property psp,
			 union power_supply_propval *val)
{
	if (power_supply_is_str_property(psp)) {
		val->strval = psp_val[psp].strval;
	} else {
		val->intval = psp_val[psp].intval;
	}
	return 0;
}

static int dummy_set_psp(struct power_supply *psy,
			 enum power_supply_property psp,
			 const union power_supply_propval *val)
{
	if (power_supply_is_str_property(psp)) {
		return -EINVAL;
	} else {
		psp_val[psp].intval = val->intval;
	}
	signal_power_supply_changed(dummy_power_supplies[DUMMY_BATTERY]);
	return 0;
}

// MODULE PARAM
static int param_set_ac_name(const char *key, const struct kernel_param *kp)
{
	strncpy(ac_name, key, MAX_KEYLENGTH);
	signal_power_supply_changed(dummy_power_supplies[DUMMY_AC]);
	return 0;
}

static int param_set_battery_name(const char *key,
				  const struct kernel_param *kp)
{
	strncpy(battery_name, key, MAX_KEYLENGTH);
	signal_power_supply_changed(dummy_power_supplies[DUMMY_BATTERY]);
	return 0;
}

static int param_set_battery_model_name(const char *key,
					const struct kernel_param *kp)
{
	strncpy(battery_model_name, key, MAX_KEYLENGTH);
	signal_power_supply_changed(dummy_power_supplies[DUMMY_BATTERY]);
	return 0;
}

static int param_set_battery_manufacturer(const char *key,
					  const struct kernel_param *kp)
{
	strncpy(battery_manufacturer, key, MAX_KEYLENGTH);
	signal_power_supply_changed(dummy_power_supplies[DUMMY_BATTERY]);
	return 0;
}

static int param_set_battery_serial_number(const char *key,
					   const struct kernel_param *kp)
{
	strncpy(battery_serial_number, key, MAX_KEYLENGTH);
	signal_power_supply_changed(dummy_power_supplies[DUMMY_BATTERY]);
	return 0;
}

static const struct kernel_param_ops param_ops_ac_name = {
	.set = param_set_ac_name,
};

static const struct kernel_param_ops param_ops_battery_name = {
	.set = param_set_battery_name,
};

static const struct kernel_param_ops param_ops_battery_model_name = {
	.set = param_set_battery_model_name,
};

static const struct kernel_param_ops param_ops_battery_manufacturer = {
	.set = param_set_battery_manufacturer,
};

static const struct kernel_param_ops param_ops_battery_serial_number = {
	.set = param_set_battery_serial_number,
};

#define param_check_ac_name(name, p) __param_check(name, p, void);
#define param_check_battery_name(name, p) __param_check(name, p, void);
#define param_check_battery_model_name(name, p) __param_check(name, p, void);
#define param_check_battery_manufacturer(name, p) __param_check(name, p, void);
#define param_check_battery_serial_number(name, p) __param_check(name, p, void);

module_param(ac_name, ac_name, 0644);
MODULE_PARM_DESC(ac_name, "AC device name (default: 'DUMMY_AC')");

module_param(battery_name, battery_name, 0644);
MODULE_PARM_DESC(battery_name, "Battery device name (default: 'DUMMY_BAT')");

module_param(battery_model_name, battery_model_name, 0644);
MODULE_PARM_DESC(battery_model_name,
		 "Battery model name (default: 'Dummy battery')");

module_param(battery_manufacturer, battery_manufacturer, 0644);
MODULE_PARM_DESC(battery_manufacturer,
		 "Battery manufacturer (default: 'Linux')");

module_param(battery_serial_number, battery_serial_number, 0644);
MODULE_PARM_DESC(battery_serial_number,
		 "battery serial number (default: UTS_RELEASE)");

// MODULE INFO
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Dummy power supply driver");
MODULE_AUTHOR("Frédéric Pierret (fepitre) <frederic.pierret@qubes-os.org>");
