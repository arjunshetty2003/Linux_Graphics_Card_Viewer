#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

#ifdef CONFIG_UNWINDER_ORC
#include <asm/orc_header.h>
ORC_HEADER;
#endif

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



static const char ____versions[]
__used __section("__versions") =
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x14\x00\x00\x00\x33\x5b\x98\xf1"
	"init_net\0\0\0\0"
	"\x20\x00\x00\x00\xa1\xa5\xaa\x0c"
	"__netlink_kernel_create\0"
	"\x1c\x00\x00\x00\xcb\xf6\xfd\xf0"
	"__stack_chk_fail\0\0\0\0"
	"\x20\x00\x00\x00\xdd\xa6\x72\xb7"
	"netlink_kernel_release\0\0"
	"\x14\x00\x00\x00\x77\x7d\x40\xe8"
	"__alloc_skb\0"
	"\x14\x00\x00\x00\x53\xb3\xd6\x1b"
	"__nlmsg_put\0"
	"\x18\x00\x00\x00\x32\x8f\x67\x19"
	"pci_get_device\0\0"
	"\x18\x00\x00\x00\xa6\xe1\xdc\xd3"
	"netlink_unicast\0"
	"\x1c\x00\x00\x00\x4d\x70\x1a\x65"
	"kfree_skb_reason\0\0\0\0"
	"\x28\x00\x00\x00\xb3\x1c\xa2\x87"
	"__ubsan_handle_out_of_bounds\0\0\0\0"
	"\x18\x00\x00\x00\x3d\xea\xc3\x7f"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "DBBAE457883AD962BB6DF41");
