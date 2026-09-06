// SPDX-License-Identifier: GPL-2.0
/*
 * ata_test - DuoQin/Agenew factory GPIO pin-test helper ("ATA test").
 *
 * Clean-room recreation of the stock Qin F25 Pro ata_test.ko (built by
 * Agenew from an EmbeTronicX sysfs example, original path
 * drivers/misc/mediatek/agenew_dev/gpio_test/ata_test.c), reconstructed
 * from the unstripped blob's symbols, relocations and strings.
 *
 * Userspace interface (identical to stock):
 *  - misc chardev /dev/pin_test:
 *      write "1" / "2"  -> no-op paths
 *      write "0"        -> run the pin test: select pinctrl state
 *                          "gpio_pin1_test", read the strap GPIO, select
 *                          "gpio_pin2_test", read again; result string
 *                          becomes "OK" with "<pin>|" appended for each
 *                          reading that came back high.
 *      read             -> returns the result string buffer.
 *  - platform driver "pin_test" (DT compatible "mediatek,pin_test") with
 *    two vestigial sysfs driver attrs (pin_test_show / pin_test_store,
 *    no-op in stock too).
 *
 * Known cosmetic divergences from the blob: udelay constant approximated
 * (stock used a compiler-folded __const_udelay value ~10us), and the
 * no-op write paths keep only their observable printks.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/uaccess.h>

#define ATA_TAG "<ATA test>[%s:%d]"

struct ata_gpio_state {
	const char *name;
	bool prepared;
	struct pinctrl_state *state;
};

static struct pinctrl *pinctrl_ata;
static struct ata_gpio_state gpios[] = {
	{ .name = "gpio_pin1_test" },
	{ .name = "gpio_pin2_test" },
};

static char result_list[32];
static int test_pin_type[2];
static int gpio_offset;
static int pin1_pass;

int GPIO_Select(int type)
{
	if (type < 0 || type >= ARRAY_SIZE(gpios)) {
		pr_err(ATA_TAG "invaild gpio type %d\n\n", __func__, __LINE__,
		       type);
		return -EINVAL;
	}
	if (!gpios[type].prepared) {
		pr_err(ATA_TAG "gpio type %d not prepared\n", __func__,
		       __LINE__, type);
		return -EINVAL;
	}
	return pinctrl_select_state(pinctrl_ata, gpios[type].state);
}

int gpio_init(struct platform_device *pdev)
{
	int i;

	pinctrl_ata = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl_ata)) {
		pr_info("gpio_init 158\n");
		return PTR_ERR(pinctrl_ata);
	}
	for (i = 0; i < ARRAY_SIZE(gpios); i++) {
		gpios[i].state = pinctrl_lookup_state(pinctrl_ata,
						      gpios[i].name);
		if (IS_ERR(gpios[i].state)) {
			pr_info("\n");
			pr_err(ATA_TAG "pinctrl_lookup_state %s fail %ld\n",
			       "gpio_init", __LINE__, gpios[i].name,
			       PTR_ERR(gpios[i].state));
			pr_info("gpio_init 172\n");
		} else {
			gpios[i].prepared = true;
		}
	}
	return 0;
}

static int pin_test_dev_open(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t pin_test_dev_read(struct file *filp, char __user *buf,
				 size_t len, loff_t *off)
{
	pr_info("6Read function\n");
	if (copy_to_user(buf, result_list,
			 min(len, sizeof(result_list))))
		pr_err("%s ioctl  copy error\n", "pin_test_dev_read");
	return 0;
}

static ssize_t pin_test_dev_write(struct file *filp, const char __user *buf,
				  size_t len, loff_t *off)
{
	char kbuf[32];
	int val = 0, v0, v1, gpio;

	pr_info("Write Function\n");
	result_list[0] = '\0';

	if (len >= sizeof(kbuf)) {
		pr_err("copy_from_user fail!");
		return len;
	}
	if (copy_from_user(kbuf, buf, len)) {
		pr_err("copy_from_user fail!");
		return len;
	}
	kbuf[len] = '\0';
	kstrtoint(kbuf, 10, &val);
	pr_info("Write Function2222222222\n");

	if (val == 1)
		return len;
	if (val == 2) {
		pr_info("55555555555\n");
		return len;
	}

	pr_info("%s = %d\n", "pin1_pass", pin1_pass);

	GPIO_Select(0);
	udelay(10);
	gpio = gpio_offset + test_pin_type[0];
	v0 = gpiod_get_raw_value(gpio_to_desc(gpio));
	pr_info("%s ioctl [%d] set 9 bit gpio [%d] 0x%x\n", "pin_test", 111,
		gpio, v0);

	GPIO_Select(1);
	udelay(10);
	gpio = gpio_offset + test_pin_type[1];
	v1 = gpiod_get_raw_value(gpio_to_desc(gpio));
	pr_info("%s ioctl [%d] set 9 bit gpio [%d] 0x%x\n", "pin_test", 116,
		gpio, v1);

	test_pin_type[0] |= 0x100;
	pr_info("%s ioctl [%d] clean after gpio [%d] 0x%x\n", "pin_test", 128,
		0, test_pin_type[0]);
	test_pin_type[1] |= 0x100;
	pr_info("%s ioctl [%d] clean after gpio [%d] 0x%x\n", "pin_test", 128,
		1, test_pin_type[1]);

	strscpy(result_list, "OK", sizeof(result_list));
	if (v0)
		snprintf(result_list, sizeof(result_list), "%s%d|",
			 result_list, test_pin_type[0]);
	if (v1)
		snprintf(result_list, sizeof(result_list), "%s%d|",
			 result_list, test_pin_type[1]);
	pr_info("\n");
	pr_info("result_list = %s\n", result_list);
	return len;
}

static long pin_test_dev_ioctl(struct file *file, unsigned int cmd,
			       unsigned long arg)
{
	return 0;
}

static const struct file_operations pin_test_fops = {
	.owner = THIS_MODULE,
	.open = pin_test_dev_open,
	.read = pin_test_dev_read,
	.write = pin_test_dev_write,
	.unlocked_ioctl = pin_test_dev_ioctl,
	.llseek = no_llseek,
};

static struct miscdevice pin_test_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "pin_test",
	.fops = &pin_test_fops,
};

static ssize_t pin_test_show_status(struct device_driver *driver, char *buf)
{
	return 0;
}

static ssize_t pin_test_store_ctrol(struct device_driver *driver,
				    const char *buf, size_t count)
{
	return 0;
}

static struct driver_attribute driver_attr_pin_test_show =
	__ATTR(pin_test_show, 0444, pin_test_show_status, NULL);
static struct driver_attribute driver_attr_pin_test_store =
	__ATTR(pin_test_store, 0200, NULL, pin_test_store_ctrol);

static struct platform_driver pin_test_platform_driver;

static int pin_test_platform_probe(struct platform_device *pdev)
{
	struct device_node *node;
	int ret;

	pr_info("%s  [%d] \n", "pin_test_platform_probe", __LINE__);

	ret = gpio_init(pdev);
	if (ret)
		return -EINVAL;

	ret = misc_register(&pin_test_device);
	pr_info("6ret of misc_register:%d\n", ret);
	if (ret)
		pr_err("%s : misc_register failed", "pin_test");

	ret = driver_create_file(&pin_test_platform_driver.driver,
				 &driver_attr_pin_test_show);
	pr_info("3driver_create_file (%s) = %d\n", "pin_test_show", ret);
	ret = driver_create_file(&pin_test_platform_driver.driver,
				 &driver_attr_pin_test_store);
	pr_info("3driver_create_file (%s) = %d\n", "pin_test_store", ret);

	node = of_find_compatible_node(NULL, NULL, "mediatek,pin_test");
	if (node) {
		gpio_offset = of_get_named_gpio_flags(node,
						      "agenew-gpio-offset", 0,
						      NULL);
		pr_info("of_get_named_gpio gpio_offset %d!!\n", gpio_offset);
		if (gpio_offset < 0)
			gpio_offset = 0;
	}
	return 0;
}

static int pin_test_platform_remove(struct platform_device *pdev)
{
	pr_err(ATA_TAG "pin_test_platform_remove\n\n", __func__, __LINE__);
	misc_deregister(&pin_test_device);
	return 0;
}

static const struct of_device_id pin_test_of_match[] = {
	{ .compatible = "mediatek,pin_test" },
	{ }
};

static struct platform_driver pin_test_platform_driver = {
	.probe = pin_test_platform_probe,
	.remove = pin_test_platform_remove,
	.driver = {
		.name = "pin_test",
		.of_match_table = pin_test_of_match,
	},
};

static int __init etx_driver_init(void)
{
	int ret;

	ret = platform_driver_register(&pin_test_platform_driver);
	if (ret) {
		pr_err(ATA_TAG "failed to register ata driver\n", __func__,
		       __LINE__);
		return ret;
	}
	pr_err(ATA_TAG "success to register ata driver\n\n", __func__,
	       __LINE__);
	pr_info("6Device Driver Insert...Done!!!\n");
	return 0;
}

static void __exit etx_driver_exit(void)
{
	platform_driver_unregister(&pin_test_platform_driver);
	pr_info("6Device Driver Remove...Done!!!\n");
}

module_init(etx_driver_init);
module_exit(etx_driver_exit);

MODULE_AUTHOR("EmbeTronicX <embetronicx@gmail.com>");
MODULE_DESCRIPTION("Simple Linux device driver (sysfs)");
MODULE_LICENSE("GPL");
