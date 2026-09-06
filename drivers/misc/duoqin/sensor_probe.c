// SPDX-License-Identifier: GPL-2.0
/*
 * sensor_probe - MediaTek sensorhub driver-registration kicker.
 *
 * Clean-room recreation of the stock Qin F25 Pro sensor_probe.ko
 * (author "Mediatek"). The stock module simply calls the *_probe()
 * registration entry points exported by the eight MTK sensorhub class
 * modules (accel_common, gyro_common, mag_common, alsps_common,
 * baro_common, step_counter, situation, fusion) at init, and the
 * matching *_remove() at exit, in this exact order:
 *   acc, gyro, mag, alsps, baro, step_c, situation, fusion.
 *
 * Divergence from stock (deliberate): the stock blob links directly
 * against those exports, which requires their symbol CRCs at build
 * time - the exporters are still prebuilt vendor blobs, so their
 * symvers are not available to this build. We resolve the entry
 * points at runtime with __symbol_get() instead, which behaves
 * identically when the sensorhub modules are loaded first (as
 * modules.load orders them) and degrades to a logged failure rather
 * than an insmod error when one is absent.
 */

#include <linux/kernel.h>
#include <linux/module.h>

struct sensor_probe_ent {
	const char *probe_sym;
	const char *remove_sym;
	const char *tag;
	int (*probe)(void);
	int (*remove)(void);
};

static struct sensor_probe_ent ents[] = {
	{ "acc_probe",       "acc_remove",       "acc" },
	{ "gyro_probe",      "gyro_remove",      "gyro" },
	{ "mag_probe",       "mag_remove",       "mag" },
	{ "alsps_probe",     "alsps_remove",     "alsps" },
	{ "baro_probe",      "baro_remove",      "baro" },
	{ "step_c_probe",    "step_c_remove",    "step_c" },
	{ "situation_probe", "situation_remove", "situ" },
	{ "fusion_probe",    "fusion_remove",    "fusion" },
};

static int __init sensor_probe_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ents); i++) {
		ents[i].probe = __symbol_get(ents[i].probe_sym);
		if (!ents[i].probe || ents[i].probe()) {
			pr_err("failed to register %s driver\n", ents[i].tag);
			if (ents[i].probe) {
				__symbol_put(ents[i].probe_sym);
				ents[i].probe = NULL;
			}
		}
	}
	return 0;
}

static void __exit sensor_probe_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(ents) - 1; i >= 0; i--) {
		if (!ents[i].probe)
			continue;
		ents[i].remove = __symbol_get(ents[i].remove_sym);
		if (ents[i].remove) {
			ents[i].remove();
			__symbol_put(ents[i].remove_sym);
		}
		__symbol_put(ents[i].probe_sym);
	}
}

module_init(sensor_probe_init);
module_exit(sensor_probe_exit);

MODULE_AUTHOR("Mediatek");
MODULE_DESCRIPTION("SensorProbe driver");
MODULE_LICENSE("GPL");
