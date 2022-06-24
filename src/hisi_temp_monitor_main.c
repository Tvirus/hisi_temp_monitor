#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/reboot.h>
#include "hisi_temp_api.h"




#define VERSION "1.0"



#define DEBUG(fmt, arg...)  do{if(debug)printk("--HITEMP-- " fmt "\n", ##arg);}while(0)
#define ERROR(fmt, arg...)  printk(KERN_ERR "--HITEMP-- " fmt "\n", ##arg)




static dev_t  temp_ndev;
static struct cdev temp_cdev;
static struct class *temp_class = NULL;
static struct device *temp_device = NULL;
struct task_struct *monitor_task = NULL;

static u32 uplimit = 105;
static u32 debug = 0;




static ssize_t temp_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    s32 temp = 0;

    if (hisi_get_temp(&temp))
    {
        ERROR("read temp failed !");
        return -EPERM;
    }
    return snprintf(buf, PAGE_SIZE, "%d\n", temp);
}
static ssize_t temp_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    return -EPERM;
}

static ssize_t uplimit_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%u\n", uplimit);
}
static ssize_t uplimit_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long v = 0;

    if (kstrtoul(buf, 0, &v))
        return -EINVAL;
    if ((0 == debug) && (v < 80))
    {
        ERROR("uplimit cannot be lower than 80'C");
        return -EINVAL;
    }
    uplimit = v;
    DEBUG("set uplimit to %u'C", uplimit);

    return size;
}

static ssize_t debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
    return snprintf(buf, PAGE_SIZE, "%u\n", debug);
}
static ssize_t debug_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
    unsigned long v;

    if (kstrtoul(buf, 0, &v))
        return -EINVAL;

    debug = !!v;

    return size;
}

struct device_attribute temp_attrs[] =
{
    __ATTR(temp,     0660,  temp_show,     temp_store),
    __ATTR(uplimit,  0660,  uplimit_show,  uplimit_store),
    __ATTR(debug,    0660,  debug_show,    debug_store  )
};
int temp_attrs_size = sizeof(temp_attrs)/sizeof(temp_attrs[0]);

static int add_sysfs_interfaces(struct device *dev, struct device_attribute *a, int size)
{
    int i;

    for (i = 0; i < size; i++)
        if (device_create_file(dev, a + i))
            goto undo;
    return 0;

undo:
    for (i--; i >= 0; i--)
        device_remove_file(dev, a + i);
    return -ENODEV;
}
static void del_sysfs_interfaces(struct device *dev, struct device_attribute *a, int size)
{
    int i;

    for (i = 0; i < size; i++)
        device_remove_file(dev, a + i);
}




static int temp_fop_open(struct inode* inode, struct file* filp)
{
    return 0;
}
static int temp_fop_release(struct inode* inode, struct file* filp)
{
    return 0;
}

int monitor_thread(void *data)
{
    s32 temp = 0;
    u32 count = 0;

    while(1)
    {
        if (kthread_should_stop())
            break;

        if (hisi_get_temp(&temp))
        {
            DEBUG("read temp failed !");
            msleep(1000);
            continue;
        }
        DEBUG("%d'C", temp);
        if (uplimit < temp)
        {
            count++;
            ERROR("current temp(%d) exceeds uplimit(%d) !", temp, uplimit);
            if (3 < count)
                kernel_restart(NULL);
        }
        else
        {
            count = 0;
        }
        msleep(1000);
    }

    return 0;
}

int start_monitor(void)
{
    monitor_task = kthread_create(monitor_thread, NULL, "temp_monitor");
    if(IS_ERR(monitor_task))
    {
        ERROR("create monitor thread failed(%ld) !\n", PTR_ERR(monitor_task));
        return -1;
    }
    wake_up_process(monitor_task);

    return 0;
}
int stop_monitor(void)
{
    if (monitor_task)
        kthread_stop(monitor_task);
    return 0;
}


static int __init temp_init(void)
{
    struct file_operations temp_fops;
    int ret = 0;

    ERROR("Driver Version: %s\n", VERSION);

    ret = hitemp_init();
    if (ret)
    {
        ERROR("init hisi temp reg failed !");
        goto ERR_0;
    }

    ret = alloc_chrdev_region(&temp_ndev, 0, 1, "hisi_temp_monitor");
    if (ret)
    {
        ERROR("alloc chrdev region failed !");
        goto ERR_1;
    }
    temp_fops.owner   = THIS_MODULE;
    temp_fops.open    = temp_fop_open;
    temp_fops.release = temp_fop_release;
    cdev_init(&temp_cdev, &temp_fops);
    ret = cdev_add(&temp_cdev, temp_ndev, 1);
    if (ret)
    {
        ERROR("failed to add char dev !");
        goto ERR_2;
    }

    temp_class = class_create(THIS_MODULE, "temperature");
    if (IS_ERR_OR_NULL(temp_class))
    {
        ERROR("failed to create class \"temperature\" !");
        ret = PTR_ERR(temp_class);
        goto ERR_3;
    }
    temp_device = device_create(temp_class, NULL, temp_ndev, NULL, "hisi_temp_monitor");
    if (IS_ERR_OR_NULL(temp_device))
    {
        ERROR("failed to create class device !");
        ret = PTR_ERR(temp_device);
        goto ERR_4;
    }

    ret = add_sysfs_interfaces(temp_device, temp_attrs, temp_attrs_size);
    if (ret)
    {
        ERROR("create sysfs interface failed !");
        goto ERR_5;
    }

    ret = start_monitor();
    if (ret)
    {
        ERROR("start monitor failed !");
        goto ERR_6;
    }

    return 0;


ERR_6:
    del_sysfs_interfaces(temp_device, temp_attrs, temp_attrs_size);
ERR_5:
    device_destroy(temp_class, temp_ndev);
ERR_4:
    class_destroy(temp_class);
ERR_3:
    cdev_del(&temp_cdev);
ERR_2:
    unregister_chrdev_region(temp_ndev, 1);
ERR_1:
    hitemp_deinit();
ERR_0:
    return ret;
}
static void __exit temp_exit(void)
{
    ERROR("exit\n");

    stop_monitor();
    del_sysfs_interfaces(temp_device, temp_attrs, temp_attrs_size);
    device_destroy(temp_class, temp_ndev);
    class_destroy(temp_class);
    cdev_del(&temp_cdev);
    unregister_chrdev_region(temp_ndev, 1);
    hitemp_deinit();
}


late_initcall(temp_init);
module_exit(temp_exit);


MODULE_AUTHOR("LLL");
MODULE_DESCRIPTION("hisi temperature monitor driver");
MODULE_LICENSE("GPL");
