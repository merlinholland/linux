/*
 *
 * Copyright (c) 2012-2021 Shenshu Technologies Co., Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include "sockioctl.h"
#include "pm.h"

/* debug code */
static int set_suspend(int eth_n)
{
	return 0;
}

/* debug code */
static int set_resume(int eth_n)
{
	return 0;
}

static int hw_states_read(struct seq_file *m, void *v)
{
	return 0;
}

static struct proc_dir_entry *gmac_proc_root;

static int proc_open_hw_states_read(struct inode *inode, struct file *file)
{
	return single_open(file, hw_states_read, PDE_DATA(inode));
}

static struct proc_file {
	char *name;
	const struct file_operations ops;

} proc_file[] = {
	{
		.name = "hw_stats",
		.ops = {
			.open           = proc_open_hw_states_read,
			.read           = seq_read,
			.llseek         = seq_lseek,
			.release        = single_release,
		},
	}
};

/*
 * /proc/gmac/
 *	|---hw_stats
 *	|---skb_pools
 */
void gmac_proc_create(void)
{
	int i;

	gmac_proc_root = proc_mkdir("gmac", NULL);
	if (gmac_proc_root == NULL)
		return;

	for (i = 0; i < ARRAY_SIZE(proc_file); i++) {
		struct proc_dir_entry *entry;

		entry = proc_create(proc_file[i].name, 0, gmac_proc_root,
				    &proc_file[i].ops);
		if (entry == NULL)
			pr_err("failed to create %s\n", proc_file[i].name);
	}
}

void gmac_proc_destroy(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(proc_file); i++)
		remove_proc_entry(proc_file[i].name, gmac_proc_root);

	remove_proc_entry("gmac", NULL);
}

int gmac_ioctl(struct net_device *ndev, struct ifreq *rq, int cmd)
{
	struct gmac_netdev_local *priv = NULL;
	struct pm_config config;
	int val = 0;
	if (ndev == NULL || rq == NULL)
		return -EINVAL;
	priv = netdev_priv(ndev);
	switch (cmd) {
	case SIOCSETPM:
		if (rq->ifr_data == NULL ||
				copy_from_user(&config, rq->ifr_data, sizeof(config)))
			return -EFAULT;
		return pmt_config(ndev, &config);

	case SIOCSETSUSPEND:
		if (rq->ifr_data == NULL || copy_from_user(&val, rq->ifr_data, sizeof(val)))
			return -EFAULT;
		return set_suspend(val);

	case SIOCSETRESUME:
		if (rq->ifr_data == NULL || copy_from_user(&val, rq->ifr_data, sizeof(val)))
			return -EFAULT;
		return set_resume(val);

	default:
		if (!netif_running(ndev))
			return -EINVAL;

		if (priv->phy == NULL)
			return -EINVAL;

		return phy_mii_ioctl(priv->phy, rq, cmd);
	}
	return 0;
}
