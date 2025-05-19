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
	{ 0x92997ed8, "_printk" },
	{ 0xefd6cf06, "__aeabi_unwind_cpp_pr0" },
	{ 0x3c3ff9fd, "sprintf" },
	{ 0x88db9f48, "__check_object_size" },
	{ 0x51a910c0, "arm_copy_to_user" },
	{ 0x2cfde9a2, "warn_slowpath_fmt" },
	{ 0xb3f7646e, "kthread_should_stop" },
	{ 0x822137e2, "arm_heavy_mb" },
	{ 0xf9a482f9, "msleep" },
	{ 0xae353d77, "arm_copy_from_user" },
	{ 0xc358aaf8, "snprintf" },
	{ 0xd9ce8f0c, "strnlen" },
	{ 0xcbd4898c, "fortify_panic" },
	{ 0x3ea1b6e4, "__stack_chk_fail" },
	{ 0xedc03953, "iounmap" },
	{ 0x14c2ba61, "kthread_stop" },
	{ 0xecf743f7, "sysfs_remove_file_ns" },
	{ 0x3d9e0db, "device_destroy" },
	{ 0xb736e912, "class_destroy" },
	{ 0x6bc3fbc0, "__unregister_chrdev" },
	{ 0xfe990052, "gpio_free" },
	{ 0xba814bea, "__register_chrdev" },
	{ 0x5505cbe1, "__class_create" },
	{ 0xf7f249d3, "device_create" },
	{ 0x5f754e5a, "memset" },
	{ 0xe97c4103, "ioremap" },
	{ 0x100fe9bd, "sysfs_create_file_ns" },
	{ 0x47229b5c, "gpio_request" },
	{ 0xb4fd8b29, "gpio_to_desc" },
	{ 0x8e0d6f5d, "gpiod_direction_output_raw" },
	{ 0xce0eceb0, "kthread_create_on_node" },
	{ 0x8864572, "wake_up_process" },
	{ 0x2d66370f, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "7B839460A3E9613D381F348");
