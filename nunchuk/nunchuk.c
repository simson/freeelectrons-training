#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/input-polldev.h>

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
struct nunchuk_state {
		char x_pos;
		char y_pos;
		short acc_x;
		short acc_y;
		short acc_z;
		char c_pressed;
		char z_pressed;
	};

struct nunchuk_info {
	int idx;
	struct nunchuk_state state;
};

static int handshake(struct i2c_client *client)
{
	int ret;
	char uncrypted_msg[] = { 0xf0 , 0x55 };
	char init_msg[] = { 0xfb , 0x00 };
		/* Send the uncrypted communication message */
	ret = i2c_master_send(client, uncrypted_msg, 2);
	if (ret != 2) {
		pr_alert("I2c transfer failed\n");
		return -EIO;
	}
	udelay(1000);
	/*Complete the init */
	ret = i2c_master_send(client, init_msg, 2);
	if (ret != 2) {
		pr_alert("I2c transfer failed\n");
		return -EIO;
	}
	return 0;
}

static int nunchuk_read_registers(struct i2c_client *client, struct nunchuk_state* n_state)
{
	char read_msg[] = { 0x00 };
	char state[6] = { 0x00 };
	int ret;
	mdelay(10);
	ret = i2c_master_send(client, read_msg, 1);
	if (ret != 1) {
		pr_alert("I2c transfer failed\n");
		return -EIO;
	}
	mdelay(10);
	ret = i2c_master_recv(client, state, 6);
	if (ret != 6) {
		pr_alert("I2c transfer failed\n");
		return -EIO;
	}
	n_state->x_pos = state[0] ;
	n_state->y_pos = state[1];
	n_state->acc_x = (state[2] << 2) | ( ( state[5] & 0x0C ) >> 2 );
	n_state->acc_y = (state[3] << 2) | ( ( state[5] & 0x30 ) >> 4 );
	n_state->acc_z = (state[4] << 2) | ( ( state[5] & 0xC0 ) >> 6 );
	n_state->c_pressed = ! ( (state[5] & 0x2 ) >> 1);
	n_state->z_pressed = ! (state[5] & 0x1 );
	return 0;
}

static int nunchuk_display_state(struct nunchuk_state *n_state)
{
	pr_alert("X = %hhu\nY = %hhu\nAcc_X = %hu\nAcc_Y = %hu\nAcc_Z = %hu\nC = %hhu\nZ = %hhu\n",
	 n_state->x_pos,
	 n_state->y_pos,
	 n_state->acc_x,
	 n_state->acc_y,
	 n_state->acc_z,
	 n_state->c_pressed,
	 n_state->z_pressed);
	return 0;
}

static int nunchuk_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct input_polled_dev* polled_input;
	struct input_dev * input;
	struct nunchuk_info* pdata;

	polled_input = input_allocate_polled_device();
	input_register_polled_device(polled_input);

	/* initialize device */
	handshake(client);
	pdata = kmalloc(sizeof(struct nunchuk_info), GFP_KERNEL);
	pdata->idx = ++last_idx;
	/* register to a kernel framework */
	i2c_set_clientdata(client, pdata);
	pr_alert("Nunchuk detected id : %d/%d\n",pdata->idx,last_idx);
	struct nunchuk_state new_state;
	while(1){
		nunchuk_read_registers(client, &(new_state));
		nunchuk_read_registers(client, &(new_state));
		if(memcmp(&(pdata->state),
			  &new_state, sizeof(struct nunchuk_state) ))
		{
			memcpy(&(pdata->state), &new_state,
			       sizeof(struct nunchuk_state));
			nunchuk_display_state(&(pdata->state));
		}

		mdelay(1);
	}

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

