#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/utsname.h>

static char *message  = "TEST";
module_param(message, charp, S_IRUGO | S_IWUSR);
static int howmany = 1;
module_param(howmany, int, S_IRUGO | S_IWUSR);
struct timeval tv_init, tv_exit, tv_res  = { 0 };

static int __init hello_init(void)
{
	int i;
	struct new_utsname *name;

	name =  utsname();
	do_gettimeofday(&tv_init);
	pr_alert("Version sysname %s, "
		 "nodename %s, "
		 "release %s, "
		 "version %s, "
		 "machine %s, "
		 "domainname %s\n",
		 name->sysname,
		 name->nodename,
		 name->release,
		 name->version,
		 name->machine,
		 name->domainname);
	for (i = 0; i < howmany; i++)
		pr_alert("%d/%d Test %s\n", i, howmany, message);

	return 0;
}

static void timersub(const struct timeval *tvp, const struct timeval *uvp,
		     struct timeval *vvp)
{
	vvp->tv_sec = tvp->tv_sec - uvp->tv_sec;
	vvp->tv_usec = tvp->tv_usec - uvp->tv_usec;
	if (vvp->tv_usec < 0) {
		--vvp->tv_sec;
		vvp->tv_usec += 1000000;
	}
}
static void __exit hello_exit(void)
{
	int days, hours, minutes, seconds, microseconds;

	do_gettimeofday(&tv_exit);
	timersub(&tv_exit, &tv_init, &tv_res);
	days = tv_res.tv_sec / 86400;
	hours = (tv_res.tv_sec % 86400) / 3600;
	minutes = (tv_res.tv_sec % 3600) / 60;
	seconds = (tv_res.tv_sec % 60);
	microseconds = (tv_res.tv_usec);
	pr_alert("FIN DU TEST : mon premier module");
	pr_alert("Il a vécu %d jours %d heure %d minutes %d secondes et %d \
		 microsecondes\n", days, hours, minutes, seconds, microseconds);
}



module_init(hello_init);
module_exit(hello_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TEST MODULE");
MODULE_AUTHOR("Siméon Marijon");

