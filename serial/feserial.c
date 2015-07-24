#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/serial_reg.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define SERIAL_BUFSIZE 32
static const struct i2c_device_id fe_serial_id[] = {
	{ "free-electrons,serial", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, fe_serial_id);

#ifdef CONFIG_OF
static const struct of_device_id fe_serial_dt_ids[] = {
	{ .compatible = "free-electrons,serial", },
	{ }
};

MODULE_DEVICE_TABLE(of, fe_serial_dt_ids);
#endif
ssize_t feserial_read(struct file *f, char __user *buf, size_t sz, loff_t *off);
ssize_t feserial_write(struct file *f, const char __user *buf, size_t sz, loff_t *off);

static const struct file_operations feserial_ops = {
	.owner		= THIS_MODULE,
	.write		= feserial_write,
	.read		= feserial_read,
};

struct feserial_dev {
	void __iomem *regs;
	struct platform_device *dev;
	struct miscdevice miscdev;
	int irq;
	char serial_buf[SERIAL_BUFSIZE];
	int serial_buf_rd;
	int serial_buf_wr;
	wait_queue_head_t serial_wait;
};

static unsigned int feserial_reg_read(struct feserial_dev *feserial_dev, int offset)
{
	return readl(feserial_dev->regs + (offset << 2)) ;
};

static void feserial_reg_write(struct feserial_dev *feserial_dev, int val, int offset)
{
	writel(val, feserial_dev->regs + (offset << 2)) ;

};


static void feserial_write_char(struct feserial_dev *feserial, char c)
{
	while( ( feserial_reg_read(feserial, UART_LSR) & UART_LSR_THRE ) == 0 )
	{
		cpu_relax();
	}
	feserial_reg_write(feserial,c, UART_TX);
}

static void feserial_write_string(struct feserial_dev *feserial, const char* str, int n)
{
	int i;
	for(i=0; i < n ; i++)
	{
		feserial_write_char(feserial, str[i]);
	}
};

ssize_t feserial_read(struct file *f, char __user *buf, size_t sz, loff_t *ppos)
{
	int ret;
	struct feserial_dev *feserial = container_of(f->private_data, struct
						feserial_dev, miscdev);
	ret = wait_event_interruptible(
			feserial->serial_wait,
			(feserial->serial_buf_wr != feserial->serial_buf_rd));

	ret = put_user(feserial->serial_buf[feserial->serial_buf_rd],buf);
	feserial->serial_buf_rd++;
	feserial->serial_buf_rd%=SERIAL_BUFSIZE;
	*ppos += 1;
	return 1;
}

ssize_t feserial_write(struct file *f, const char __user *buf, size_t sz, loff_t *ppos)
{
	int i;
	struct feserial_dev *dev = container_of(f->private_data, struct
						feserial_dev, miscdev);
	for(i=0; i < sz; i++)
	{
		unsigned char c;
		if(get_user(c,buf+i))
			return EFAULT;

		feserial_write_char(dev, c);

		if (c == '\n')
		{
			feserial_write_char(dev, c);
		}
	}
	*ppos += sz;
	return sz;
}

irqreturn_t feserial_irqhandler(int irq, void *dev_id)
{
	struct feserial_dev *feserial = dev_id;
	char c = feserial_reg_read(feserial, UART_RX);
	feserial->serial_buf[feserial->serial_buf_wr] = c;
	feserial->serial_buf_wr++;
	feserial->serial_buf_wr%=SERIAL_BUFSIZE;
	wake_up(&feserial->serial_wait);
	return IRQ_HANDLED;
}

static int feserial_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct feserial_dev *feserial;
	int uartclk, baud_divisor;

	feserial = devm_kzalloc(&pdev->dev, sizeof(struct feserial_dev), GFP_KERNEL);
	platform_set_drvdata(pdev, feserial);
	pr_alert("Called feserial_probe\n");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	if(!res)
	{
		dev_err(&pdev->dev, "Failed to get the resource\n");
		return -EIO;
	}
	feserial->regs = devm_ioremap_resource(&pdev->dev,res);
	pr_info("Start = 0x%x : 0x%p\n",res->start, feserial->regs);

	feserial->irq = platform_get_irq(pdev, 0);
	if(feserial->irq < 0)
	{
		dev_err(&pdev->dev, "Failed to get the IRQ\n");
		return feserial->irq;
	}
	pr_info("IRQ %d will be handled\n",feserial->irq);
	devm_request_irq(
			       &pdev->dev, 
			       feserial->irq, 
			       feserial_irqhandler,
			       0, 
			       "freeelc_uart_irq", 
			       feserial);


	pm_runtime_enable(&pdev->dev);
	pm_runtime_get_sync(&pdev->dev);

	/* Configure the baud rate */
	of_property_read_u32(pdev->dev.of_node, "clock-frequency", &uartclk);
	baud_divisor = uartclk / 16 / 115200;
	feserial_reg_write(feserial, 0x07, UART_OMAP_MDR1);
	feserial_reg_write(feserial, 0x00, UART_LCR);
	feserial_reg_write(feserial, UART_LCR_DLAB, UART_LCR);
	feserial_reg_write(feserial, baud_divisor & 0xff, UART_DLL);
	feserial_reg_write(feserial, (baud_divisor >> 8) & 0xff, UART_DLM);
	feserial_reg_write(feserial, UART_LCR_WLEN8, UART_LCR);
	feserial_reg_write(feserial, UART_IER_RDI, UART_IER);

	/* Soft reset */
	feserial_reg_write(feserial, UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT, UART_FCR);
	feserial_reg_write(feserial, 0x00, UART_OMAP_MDR1);

	feserial_write_string(feserial, "deadbeef", 8);

	/* Init workqueue */
	init_waitqueue_head(&feserial->serial_wait);

	/* Init miscdevice */
	feserial->miscdev.minor = MISC_DYNAMIC_MINOR;
	feserial->miscdev.fops	= &feserial_ops;
	feserial->miscdev.name = kasprintf(GFP_KERNEL,"feserial-%x", res->start);

	misc_register(&feserial->miscdev);

	return 0;
}

static int feserial_remove(struct platform_device *pdev)
{
	struct feserial_dev *feserial = platform_get_drvdata(pdev);
	pr_info("Called feserial_remove\n");
	feserial_write_string(feserial, "beefdead", 8);

	/*Free and unregister */
	kfree(feserial->miscdev.name);
	misc_deregister(&feserial->miscdev);
	pm_runtime_disable(&pdev->dev);
	return 0;
}



static struct platform_driver feserial_driver = {
	.driver = {
		.name = "FE_SERIAL",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(fe_serial_dt_ids),
	},
	.probe = feserial_probe,
	.remove = feserial_remove,
};


module_platform_driver(feserial_driver);

MODULE_LICENSE("GPL");
