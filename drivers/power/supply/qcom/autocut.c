/*
 * Auto-cut charging module for X00TD
 * Sysfs: /sys/module/autocut/parameters/max_soc
 *        /sys/module/autocut/parameters/min_soc
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/workqueue.h>
#include <linux/power_supply.h>

static int max_soc = 100;
static int min_soc = 90;
module_param(max_soc, int, 0644);
MODULE_PARM_DESC(max_soc, "Max SOC to stop charging (default 100)");
module_param(min_soc, int, 0644);
MODULE_PARM_DESC(min_soc, "Min SOC to resume charging (default 90)");

static struct delayed_work autocut_work;

static void autocut_work_fn(struct work_struct *work)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret, capacity, charging;

	psy = power_supply_get_by_name("battery");
	if (!psy)
		goto reschedule;

	/* Read capacity */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	if (ret)
		goto put_psy;
	capacity = val.intval;

	/* Read charging_enabled state */
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
	if (ret)
		goto put_psy;
	charging = val.intval;

	/* Logic: max=100 stop, min=90 resume */
	if (capacity >= max_soc && charging) {
		val.intval = 0;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
		if (!ret)
			pr_info("autocut: charging STOPPED at %d%% (max=%d)\n", capacity, max_soc);
	} else if (capacity <= min_soc && !charging) {
		val.intval = 1;
		ret = power_supply_set_property(psy, POWER_SUPPLY_PROP_CHARGING_ENABLED, &val);
		if (!ret)
			pr_info("autocut: charging RESUMED at %d%% (min=%d)\n", capacity, min_soc);
	}

put_psy:
	power_supply_put(psy);

reschedule:
	schedule_delayed_work(&autocut_work, msecs_to_jiffies(10000));
}

static int __init autocut_init(void)
{
	INIT_DELAYED_WORK(&autocut_work, autocut_work_fn);
	schedule_delayed_work(&autocut_work, msecs_to_jiffies(10000));
	pr_info("autocut: loaded (max_soc=%d, min_soc=%d)\n", max_soc, min_soc);
	return 0;
}

static void __exit autocut_exit(void)
{
	cancel_delayed_work_sync(&autocut_work);
	pr_info("autocut: unloaded\n");
}

module_init(autocut_init);
module_exit(autocut_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Auto-cut charging with configurable SOC thresholds");
MODULE_AUTHOR("X00TD Porter");
