#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x9a31bb74, "module_layout" },
	{ 0xe97bdac7, "sock_no_sendpage" },
	{ 0xdae5944f, "sock_no_mmap" },
	{ 0x1c3620dd, "sock_no_shutdown" },
	{ 0x90f502e5, "sock_no_listen" },
	{ 0xfdde3ed1, "can_ioctl" },
	{ 0x1454c71b, "datagram_poll" },
	{ 0x61cfd299, "sock_no_accept" },
	{ 0x9139096d, "sock_no_socketpair" },
	{ 0xfe6a046b, "sock_no_connect" },
	{ 0xe113af5f, "can_proto_unregister" },
	{ 0xd1d3ade5, "can_proto_register" },
	{ 0x27e1a049, "printk" },
	{ 0x4f6b400b, "_copy_from_user" },
	{ 0x69acdf38, "memcpy" },
	{ 0xc87c1f84, "ktime_get" },
	{ 0x28ce48ff, "__alloc_skb" },
	{ 0x4c4fef19, "kernel_stack" },
	{ 0x211c8654, "can_rx_register" },
	{ 0x34af0b52, "sk_free" },
	{ 0x5c3edd59, "_raw_write_unlock_bh" },
	{ 0x32eeaded, "_raw_write_lock_bh" },
	{ 0x82072614, "tasklet_kill" },
	{ 0xedbaf048, "hrtimer_cancel" },
	{ 0xfe769456, "unregister_netdevice_notifier" },
	{ 0x4f8b5ddb, "_copy_to_user" },
	{ 0xb2fd5ceb, "__put_user_4" },
	{ 0x6729d3df, "__get_user_4" },
	{ 0xa1c76e0a, "_cond_resched" },
	{ 0x2d178924, "hrtimer_start" },
	{ 0x915ce441, "can_send" },
	{ 0xcf21d241, "__wake_up" },
	{ 0x11f2fca, "skb_put" },
	{ 0xc502320, "sock_alloc_send_skb" },
	{ 0xde794587, "dev_get_by_index" },
	{ 0x9af89f98, "memcpy_fromiovec" },
	{ 0xfa66f77c, "finish_wait" },
	{ 0x5c8b5ce8, "prepare_to_wait" },
	{ 0x1000e51, "schedule" },
	{ 0x64ce4311, "current_task" },
	{ 0xc8b57c27, "autoremove_wake_function" },
	{ 0xe82d9f69, "__sock_recv_wifi_status" },
	{ 0x8f397731, "skb_free_datagram" },
	{ 0xfeff91ac, "__sock_recv_timestamp" },
	{ 0xd7cbddda, "memcpy_toiovec" },
	{ 0xa030307, "skb_recv_datagram" },
	{ 0xfaef0ed, "__tasklet_schedule" },
	{ 0x1ed94e8b, "kfree_skb" },
	{ 0x665fccc3, "sock_queue_rcv_skb" },
	{ 0x522e98e9, "release_sock" },
	{ 0x52cd0cc9, "can_rx_unregister" },
	{ 0x6fd07228, "lock_sock_nested" },
	{ 0x44c87265, "init_net" },
	{ 0x63ecad53, "register_netdevice_notifier" },
	{ 0xf432dd3d, "__init_waitqueue_head" },
	{ 0x9545af6d, "tasklet_init" },
	{ 0x8125c3b4, "hrtimer_init" },
	{ 0xbdfb6dbb, "__fentry__" },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=can";


MODULE_INFO(srcversion, "71EEBDA85DE15D403C8DE83");
