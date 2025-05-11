#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/mutex.h>

#define DEVICE_NAME "ads1115"
#define CLASS_NAME "ads"

#define ADS1115_REG_CONVERT   0x00
#define ADS1115_REG_CONFIG    0x01

//Single-shot, PGA=±4.096V, 128SPS
#define ADS1115_CONFIG_MSB_AIN0 0xC3
#define ADS1115_CONFIG_MSB_AIN1 0xD3
#define ADS1115_CONFIG_MSB_AIN2 0xE3
#define ADS1115_CONFIG_MSB_AIN3 0xF3
#define ADS1115_CONFIG_LSB      0x83

#define ADS1115_IOCTL_READ_AIN0 _IOR('a', 1, int)
#define ADS1115_IOCTL_READ_AIN1 _IOR('a', 2, int)
#define ADS1115_IOCTL_READ_AIN2 _IOR('a', 3, int)
#define ADS1115_IOCTL_READ_AIN3 _IOR('a', 4, int)

static struct i2c_client *ads1115_client;
static struct class *ads1115_class;
static dev_t dev_number;
static struct cdev ads1115_cdev;
static DEFINE_MUTEX(ads1115_mutex);

static int ads1115_read_channel(int channel)
{
	int ret;
	u8 config[2];

	switch (channel) {
	case 0: config[0] = ADS1115_CONFIG_MSB_AIN0; break;
	case 1: config[0] = ADS1115_CONFIG_MSB_AIN1; break;
	case 2: config[0] = ADS1115_CONFIG_MSB_AIN2; break;
	case 3: config[0] = ADS1115_CONFIG_MSB_AIN3; break;
	default: return -EINVAL;
	}

	config[1] = ADS1115_CONFIG_LSB;

	ret = i2c_smbus_write_i2c_block_data(ads1115_client, ADS1115_REG_CONFIG, 2, config);
	if (ret < 0)
		return ret;

	msleep(10);

	ret = i2c_smbus_read_word_data(ads1115_client, ADS1115_REG_CONVERT);
	if (ret < 0)
		return ret;

	short raw_value = (short)swab16(ret);
	return raw_value < 0 ? 0 : raw_value;
}

static long ads1115_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int value;

	mutex_lock(&ads1115_mutex);
	switch (cmd) {
	case ADS1115_IOCTL_READ_AIN0: value = ads1115_read_channel(0); break;
	case ADS1115_IOCTL_READ_AIN1: value = ads1115_read_channel(1); break;
	case ADS1115_IOCTL_READ_AIN2: value = ads1115_read_channel(2); break;
	case ADS1115_IOCTL_READ_AIN3: value = ads1115_read_channel(3); break;
	default:
		mutex_unlock(&ads1115_mutex);
		return -ENOTTY;
	}
	mutex_unlock(&ads1115_mutex);

	if (value < 0)
		return value;

	if (copy_to_user((int __user *)arg, &value, sizeof(int)))
		return -EFAULT;

	return 0;
}

static int ads1115_open(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "ADS1115 opened\n");
	return 0;
}

static int ads1115_release(struct inode *inode, struct file *file)
{
	printk(KERN_INFO "ADS1115 closed\n");
	return 0;
}

static struct file_operations fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ads1115_ioctl,
	.open = ads1115_open,
	.release = ads1115_release,
};

static int ads1115_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;

	ads1115_client = client;

	ret = alloc_chrdev_region(&dev_number, 0, 1, DEVICE_NAME);
	if (ret < 0) return ret;

	cdev_init(&ads1115_cdev, &fops);
	ret = cdev_add(&ads1115_cdev, dev_number, 1);
	if (ret < 0) goto unregister;

	ads1115_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(ads1115_class)) {
		ret = PTR_ERR(ads1115_class);
		goto del_cdev;
	}

	device_create(ads1115_class, NULL, dev_number, NULL, DEVICE_NAME);
	mutex_init(&ads1115_mutex);
	printk(KERN_INFO "ADS1115 driver installed\n");
	return 0;

del_cdev:
	cdev_del(&ads1115_cdev);
unregister:
	unregister_chrdev_region(dev_number, 1);
	return ret;
}

static void ads1115_remove(struct i2c_client *client)
{
	device_destroy(ads1115_class, dev_number);
	class_destroy(ads1115_class);
	cdev_del(&ads1115_cdev);
	unregister_chrdev_region(dev_number, 1);
	printk(KERN_INFO "ADS1115 driver removed\n");
}

static const struct i2c_device_id ads1115_id[] = {
	{ "ads1115", 0 },
	{}
};
MODULE_DEVICE_TABLE(i2c, ads1115_id);

static struct i2c_driver ads1115_driver = {
	.driver = {
		.name = "ads1115",
	},
	.probe = ads1115_probe,
	.remove = ads1115_remove,
	.id_table = ads1115_id,
};

module_i2c_driver(ads1115_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Dylan");
MODULE_DESCRIPTION("ADS1115 driver with 4 channels AIN0-AIN3");
