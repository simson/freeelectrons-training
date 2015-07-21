#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/utsname.h>

static char* message  = "TEST";
module_param(message, charp, S_IRUGO| S_IWUSR);
static int howmany = 1;
module_param(howmany, int, S_IRUGO| S_IWUSR);

static int __init hello_init(void)
{
	int i;
	struct new_utsname * name;
	name =  utsname();
	pr_alert("Version sysname %s, \
	nodename %s, \
	release %s, \
	version %s, \
	machine %s, \
	domainname %s\n",
	name->sysname,
	name->nodename,
	name->release,
	name->version,
	name->machine,
	name->domainname);
	for(i = 0; i < howmany; i++)
	{
		pr_alert("%d/%d Test %s \n", i, howmany, message);
	}

	return 0;
}

static void __exit hello_exit(void)
{
	pr_alert("FIN DU TEST : mon premier module");
}

module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TEST MODULE");
MODULE_AUTHOR("Siméon Marijon");

