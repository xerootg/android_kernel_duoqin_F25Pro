// SPDX-License-Identifier: GPL-2.0
/*
 * board_id_gpio_state - DuoQin/Agenew board-ID strap reader.
 *
 * Clean-room recreation of the stock Qin F25 Pro board_id_gpio_state.ko
 * (author chenjiqian@agenewtech.com), reconstructed from the unstripped
 * blob's symbol table, relocations and string constants.
 *
 * At probe it reads the three board-ID strap GPIOs named in the
 * "agenew,board_id_gpio_state" device-tree node and caches their levels;
 * the values are exposed read-only via
 *   /sys/class/agn_gpio/gpio_dev/board_id_{0,1,2}
 * exactly like the stock module. The GPIOs are sampled once and freed.
 */

#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

static u8 state_gpio[3];
static struct class *agn_gpio_class;
static struct device *gpio_dev;

static const char *const board_id_prop[3] = {
	"agenew-gpio-board_id_0",
	"agenew-gpio-board_id_1",
	"agenew-gpio-board_id_2",
};

#define BOARD_ID_ATTR(n)						\
static ssize_t board_id_##n##_show(struct device *dev,			\
				   struct device_attribute *attr,	\
				   char *buf)				\
{									\
	pr_info("state_gpio[" #n "] = %d\n", state_gpio[n]);		\
	return sprintf(buf, "%d\n", state_gpio[n]);			\
}									\
static ssize_t board_id_##n##_store(struct device *dev,			\
				    struct device_attribute *attr,	\
				    const char *buf, size_t count)	\
{									\
	pr_info("board_id_" #n "_gpio_store\n");			\
	return count;							\
}									\
static DEVICE_ATTR(board_id_##n, 0644, board_id_##n##_show,		\
		   board_id_##n##_store)

BOARD_ID_ATTR(0);
BOARD_ID_ATTR(1);
BOARD_ID_ATTR(2);

static int board_id_gpio_platform_probe(struct platform_device *pdev)
{
	struct device_node *node;
	int i, num, ret;

	pr_info("%s: &pdev=%p\n", __func__, pdev);

	node = of_find_compatible_node(NULL, NULL,
				       "agenew,board_id_gpio_state");
	if (!node)
		return 0;

	for (i = 0; i < 3; i++) {
		num = of_get_named_gpio_flags(node, board_id_prop[i], 0, NULL);
		pr_info("of_get_named_gpio board_id_gpio_num[%d] %d\n", i, num);
		if (num < 0)
			continue;
		if (gpio_request(num, board_id_prop[i]))
			continue;
		ret = gpiod_direction_input(gpio_to_desc(num));
		pr_info("Setting GPIO input mode succeeded  ret = %d\n", ret);
		state_gpio[i] = gpiod_get_raw_value(gpio_to_desc(num));
		pr_info("state_gpio[%d] = %d\n", i, state_gpio[i]);
		gpio_free(num);
		pr_info("Setting GPIO free mode succeeded\n");
	}
	return 0;
}

static int board_id_gpio_platform_remove(struct platform_device *pdev)
{
	pr_info("board_id_gpio_platform_remove\n");
	return 0;
}

static const struct of_device_id board_id_gpio_of_match[] = {
	{ .compatible = "agenew,board_id_gpio_state" },
	{ }
};

static struct platform_driver gpio_platform_driver = {
	.probe = board_id_gpio_platform_probe,
	.remove = board_id_gpio_platform_remove,
	.driver = {
		.name = "board_id_gpio",
		.of_match_table = board_id_gpio_of_match,
	},
};

static int __init board_id_gpio_init(void)
{
	pr_info("board_id_gpio_init\n");

	agn_gpio_class = class_create(THIS_MODULE, "agn_gpio");
	if (IS_ERR(agn_gpio_class)) {
		pr_info("Failed to create agn_gpio!\n");
		return PTR_ERR(agn_gpio_class);
	}
	gpio_dev = device_create(agn_gpio_class, NULL, 0, NULL, "gpio_dev");
	if (IS_ERR(gpio_dev)) {
		pr_info("Failed to create gpio_dev!\n");
		class_destroy(agn_gpio_class);
		return PTR_ERR(gpio_dev);
	}
	if (device_create_file(gpio_dev, &dev_attr_board_id_0) ||
	    device_create_file(gpio_dev, &dev_attr_board_id_1) ||
	    device_create_file(gpio_dev, &dev_attr_board_id_2))
		pr_info("Failed to create file!\n");

	platform_driver_register(&gpio_platform_driver);
	pr_info(" board_id_gpio_init success\n");
	return 0;
}

static void __exit board_id_gpio_exit(void)
{
	pr_info("board_id_gpio_exit\n");
	platform_driver_unregister(&gpio_platform_driver);
	device_remove_file(gpio_dev, &dev_attr_board_id_0);
	device_remove_file(gpio_dev, &dev_attr_board_id_1);
	device_remove_file(gpio_dev, &dev_attr_board_id_2);
	device_destroy(agn_gpio_class, 0);
	class_destroy(agn_gpio_class);
}

module_init(board_id_gpio_init);
module_exit(board_id_gpio_exit);

MODULE_AUTHOR("chenjiqian@agenewtech.com");
MODULE_DESCRIPTION("board_id_gpio driver");
MODULE_LICENSE("GPL");
