#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>

static const struct i2c_device_id nunchuk_id[] = {
	{ "nunchuk", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, nunchuk_id);

#ifdef CONFIG_OF
static const struct of_device_id nunchuk_dt_ids[] = {
	{ .compatible = "nintendo,nunchuk", },
	{ }
};

MODULE_DEVICE_TABLE(of, nunchuk_dt_ids);
#endif

static int last_idx = 0;
struct nunchuk_info {
	int idx;
};

static int nunchuk_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	/* initialize device */
	struct nunchuk_info* pdata = kmalloc(sizeof(struct nunchuk_info), GFP_KERNEL);
	pdata->idx = ++last_idx;
	/* register to a kernel framework */
	i2c_set_clientdata(client, pdata);
	pr_alert("Nunchuk detected id : %d/%d\n",pdata->idx,last_idx);
	return 0;
}

static int nunchuk_remove(struct i2c_client *client)
{
	struct nunchuk_info* pdata = i2c_get_clientdata(client);
	pr_alert("Nunchuk will be removed id : %d/%d\n",pdata->idx,last_idx);
	kfree(pdata);
	last_idx--;
	pr_alert("It remains %d nunchunk connected\n", last_idx);
	/* unregister device from kernel framework */
	/* shut down the device */
	return 0;
}

static struct i2c_driver nunchuk_driver = {
	.probe	= nunchuk_probe,
	.remove	= nunchuk_remove,
	.id_table = nunchuk_id,
	.driver = {
		.name = "Nunchuk Driver",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(nunchuk_dt_ids),
	},
};

module_i2c_driver(nunchuk_driver);

MODULE_LICENSE("GPL");

