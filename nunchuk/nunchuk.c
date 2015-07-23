#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/input.h>
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

struct nunchuk_dev {
	struct input_polled_dev * polled_input;
	struct i2c_client *i2c_client;
	int idx;
	struct nunchuk_state state;
};

static int handshake(struct nunchuk_dev *dev)
{
	int ret;
	char uncrypted_msg[] = { 0xf0 , 0x55 };
	char init_msg[] = { 0xfb , 0x00 };
	/* Send the uncrypted communication message */
	ret = i2c_master_send(dev->i2c_client, uncrypted_msg, 2);
	if (ret != 2) {
		dev_err(&dev->i2c_client->dev,"I2c send failed (%d)\n",ret);
		return ret < 0 ? ret : -EIO;
	}
	udelay(1000);
	/*Complete the init */
	ret = i2c_master_send(dev->i2c_client, init_msg, 2);
	if (ret != 2) {
		dev_err(&dev->i2c_client->dev,"I2c send failed (%d)\n",ret);
		return ret < 0 ? ret : -EIO;
	}
	return 0;
}

static int nunchuk_read_registers(struct nunchuk_dev *dev, struct nunchuk_state* n_state)
{
	char read_msg[] = { 0x00 };
	char state[6] = { 0x00 };
	int ret;
	mdelay(10);
	ret = i2c_master_send(dev->i2c_client, read_msg, 1);
	if (ret != 1) {
		dev_err(&dev->i2c_client->dev,"I2c send failed (%d)\n",ret);
		return ret < 0 ? ret : -EIO;
	}
	mdelay(10);
	ret = i2c_master_recv(dev->i2c_client, state, 6);
	if (ret != 6) {
		dev_err(&dev->i2c_client->dev,"I2c send failed (%d)\n",ret);
		return ret < 0 ? ret : -EIO;
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
	pr_info("X = %hhu\nY = %hhu\nAcc_X = %hu\nAcc_Y = %hu\nAcc_Z = %hu\nC = %hhu\nZ = %hhu\n",
		n_state->x_pos,
		n_state->y_pos,
		n_state->acc_x,
		n_state->acc_y,
		n_state->acc_z,
		n_state->c_pressed,
		n_state->z_pressed);
	return 0;
}

static void nunchuk_poll(struct input_polled_dev *dev)
{
	struct nunchuk_dev *nunchuk = dev->private;
	struct i2c_client *client = nunchuk->i2c_client;
	struct nunchuk_state new_state;
	int ret;

	if( (ret = nunchuk_read_registers(nunchuk, &(new_state))) < 0)
	{
		dev_err(&client->dev, "Reading Nunchuk register failed\n");
	}
	if( (ret = nunchuk_read_registers(nunchuk, &(new_state))) < 0)
	{
		dev_err(&client->dev, "Reading Nunchuk register failed\n");
	}
	if(memcmp(&(nunchuk->state),
		  &new_state, sizeof(struct nunchuk_state) ))
	{
		memcpy(&(nunchuk->state), &new_state,
		       sizeof(struct nunchuk_state));
		input_event(dev->input, EV_KEY, BTN_C,
			    nunchuk->state.c_pressed);
		input_event(dev->input, EV_KEY, BTN_Z,
			    nunchuk->state.z_pressed);
		input_sync(dev->input);
	}



}
static int nunchuk_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct input_polled_dev* polled_input;
	struct input_dev * input;
	struct nunchuk_dev* nunchuk;
	struct nunchuk_state new_state;
	int ret;

	nunchuk = devm_kzalloc(&client->dev, sizeof(struct nunchuk_dev), GFP_KERNEL);
	if(!nunchuk)
	{
		dev_err(&client->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}
	polled_input = input_allocate_polled_device();
	if (!polled_input)
	{
		dev_err(&client->dev, "Failed to allocate memory\n");
		goto out_nofree;
	}

	nunchuk->i2c_client = client;
	nunchuk->polled_input = polled_input;
	polled_input->private = nunchuk;
	/* register to a kernel framework */
	i2c_set_clientdata(client, nunchuk);
	input = polled_input->input;
	input->dev.parent = &client->dev;
	
	input->name = "Wii nunchuk";
	input->id.bustype = BUS_I2C;
	set_bit(EV_KEY, input->evbit);
	set_bit(BTN_C, input->keybit);
	set_bit(BTN_Z, input->keybit);
	polled_input->poll = nunchuk_poll;
	polled_input->poll_interval = 50;

	nunchuk->idx = ++last_idx;
	pr_info("Nunchuk detected id : %d/%d\n",nunchuk->idx,last_idx);

	if( input_register_polled_device(polled_input) < 0)
		goto out_input_polled_device;

	/* initialize device */
	handshake(nunchuk);
	/*
	   while(1){

	   if( (ret = nunchuk_read_registers(nunchuk, &(new_state))) < 0)
	   {
	   dev_err(&client->dev, "Reading Nunchuk register failed\n");
	   while( (ret = handshake(nunchuk)) < 0)
	   {
	   mdelay(10);
	   pr_info("Retrying\n");
	   }

	   }
	   if( (ret = nunchuk_read_registers(nunchuk, &(new_state))) < 0)
	   {
	   dev_err(&client->dev, "Reading Nunchuk register failed\n");
	   while( (ret = handshake(nunchuk)) < 0)
	   {
	   mdelay(10);
	   pr_info("Retrying\n");
	   }

	   }
	   if(memcmp(&(nunchuk->state),
	   &new_state, sizeof(struct nunchuk_state) ))
	   {
	   memcpy(&(nunchuk->state), &new_state,
	   sizeof(struct nunchuk_state));
	   nunchuk_display_state(&(nunchuk->state));
	   }


	   mdelay(1);
	   }
	   */
	return 0;

out_input_polled_device:
	dev_err(&client->dev, "Free the polled_input strcuture\n");
	input_free_polled_device(polled_input);
out_nofree:
	return -ENOMEM;
}

static int nunchuk_remove(struct i2c_client *client)
{
	struct nunchuk_dev* nunchuk = i2c_get_clientdata(client);
	if(!nunchuk)
	{
		dev_err(&client->dev, "Failed to retrieve the device structure\n");
		return -ENOMEM;
	}

	pr_info("Nunchuk will be removed id : %d/%d\n",nunchuk->idx,last_idx);
	last_idx--;
	pr_info("It remains %d nunchuk connected\n", last_idx);
	/* unregister device from kernel framework */
	if(!nunchuk->polled_input)
	{
		dev_err(&client->dev, "Failed to retrieve the polled device structure\n");
		return -ENOMEM;
	}

	pr_info("A = polled_input %p\n",nunchuk->polled_input->input);
	input_unregister_polled_device(nunchuk->polled_input);
	input_free_polled_device(nunchuk->polled_input);
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

