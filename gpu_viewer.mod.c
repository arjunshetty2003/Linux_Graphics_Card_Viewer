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

#ifdef CONFIG_MITIGATION_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif



static const char ____versions[]
__used __section("__versions") =
	"\x14\x00\x00\x00\xb7\x6f\x70\xe6"
	"single_open\0"
	"\x14\x00\x00\x00\x65\xc9\xf2\x1d"
	"proc_remove\0"
	"\x14\x00\x00\x00\x8b\x3c\x3a\x37"
	"seq_write\0\0\0"
	"\x18\x00\x00\x00\x1c\xcf\x21\xb2"
	"pci_get_device\0\0"
	"\x14\x00\x00\x00\x69\xb6\x98\xa4"
	"seq_printf\0\0"
	"\x20\x00\x00\x00\x66\xef\xf7\xcd"
	"pci_read_config_dword\0\0\0"
	"\x20\x00\x00\x00\x10\xb5\xda\x64"
	"pci_write_config_dword\0\0"
	"\x1c\x00\x00\x00\xd9\xc8\x50\x22"
	"pci_find_capability\0"
	"\x20\x00\x00\x00\xa9\x75\x1d\xbb"
	"pci_read_config_word\0\0\0\0"
	"\x1c\x00\x00\x00\xcb\xf6\xfd\xf0"
	"__stack_chk_fail\0\0\0\0"
	"\x14\x00\x00\x00\x0e\xd8\xd5\xd9"
	"seq_read\0\0\0\0"
	"\x14\x00\x00\x00\xe5\x41\x2a\xae"
	"seq_lseek\0\0\0"
	"\x18\x00\x00\x00\x0a\x8d\x36\x7f"
	"single_release\0\0"
	"\x14\x00\x00\x00\xbb\x6d\xfb\xbd"
	"__fentry__\0\0"
	"\x14\x00\x00\x00\x2b\x3a\x21\x7f"
	"proc_create\0"
	"\x10\x00\x00\x00\x7e\x3a\x2c\x12"
	"_printk\0"
	"\x1c\x00\x00\x00\xca\x39\x82\x5b"
	"__x86_return_thunk\0\0"
	"\x18\x00\x00\x00\xde\x9f\x8a\x25"
	"module_layout\0\0\0"
	"\x00\x00\x00\x00\x00\x00\x00\x00";

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "29347CBCA0B2DA77F02CE49");
