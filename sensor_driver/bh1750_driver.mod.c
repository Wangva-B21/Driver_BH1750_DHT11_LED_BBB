#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xb1ad28e0, "__gnu_mcount_nc" },
	{ 0x828ce6bb, "mutex_lock" },
	{ 0xc358aaf8, "snprintf" },
	{ 0x9618ede0, "mutex_unlock" },
	{ 0x528c709d, "simple_read_from_buffer" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0x14c2ba61, "kthread_stop" },
	{ 0x3d9e0db, "device_destroy" },
	{ 0xb736e912, "class_destroy" },
	{ 0x97fc4079, "cdev_del" },
	{ 0x6091b333, "unregister_chrdev_region" },
	{ 0xfe990052, "gpio_free" },
	{ 0xedc03953, "iounmap" },
	{ 0x92997ed8, "_printk" },
	{ 0x822137e2, "arm_heavy_mb" },
	{ 0x8e865d3c, "arm_delay_ops" },
	{ 0xf9a482f9, "msleep" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0xe97c4103, "ioremap" },
	{ 0x47229b5c, "gpio_request" },
	{ 0xe3ec2f2b, "alloc_chrdev_region" },
	{ 0x2085a943, "cdev_init" },
	{ 0x13cbe78c, "cdev_add" },
	{ 0x5505cbe1, "__class_create" },
	{ 0xf7f249d3, "device_create" },
	{ 0xce0eceb0, "kthread_create_on_node" },
	{ 0x8864572, "wake_up_process" },
	{ 0x2d66370f, "module_layout" },
};

MODULE_INFO(depends, "");

