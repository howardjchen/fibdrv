#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"

/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 500

static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);

static ktime_t kt;

static char *add_two_string(char *num1, char *num2)
{
    int idx1 = strlen(num1) - 1;
    int idx2 = strlen(num2) - 1;

    int n = idx1 > idx2 ? strlen(num1) : strlen(num2);
    char *ret = kzalloc((n + 2) * sizeof(char), GFP_KERNEL);
    if (!ret) {
        printk(KERN_INFO "[%s] Cannot get memory with size %d\n", __func__,
               n + 2);
        return NULL;
    }

    int idxRet = n;
    int carry = 0;

    while (idx1 >= 0 || idx2 >= 0) {
        int sum = 0;

        if (idx1 >= 0) {
            sum += num1[idx1] - '0';
            idx1--;
        }
        if (idx2 >= 0) {
            sum += num2[idx2] - '0';
            idx2--;
        }
        sum += carry;
        carry = sum / 10;
        ret[idxRet] = sum % 10 + '0';
        idxRet--;
    }

    if (carry) {
        ret[idxRet] = '1';
        idxRet--;
    }

    return &(ret[idxRet + 1]);
}

static int fib_sequence(char **f, long long k)
{
    f[0] = kzalloc(sizeof(char) + 1, GFP_KERNEL);
    f[1] = kzalloc(sizeof(char) + 1, GFP_KERNEL);
    strncpy(f[0], "0", 2);
    strncpy(f[1], "1", 2);

    for (int i = 2; i <= k; i++) {
        f[i] = add_two_string(f[i - 1], f[i - 2]);
        if (f[i] == NULL)
            return -ENOMEM;
    }

    return 0;
}

static int fib_time_proxy(char **f, long long k)
{
    kt = ktime_get();
    int ret = fib_sequence(f, k);
    kt = ktime_sub(ktime_get(), kt);

    return ret;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    char **f;

    if (*offset > 1)
        f = kmalloc((*offset + 1) * sizeof(f), GFP_KERNEL);
    else
        f = kmalloc(3 * sizeof(f), GFP_KERNEL);

    if (!f) {
        printk(KERN_INFO "[%s] Cannot get memory with size %lld\n", __func__,
               (*offset + 1));
        return -ENOMEM;
    }

    int ret = fib_time_proxy(f, *offset);
    if (ret < 0)
        return ret;

    /*
     * Copy at most size bytes to user space.
     * Return ''0'' on success and some other value on error.
     */
    if (copy_to_user(buf, f[*offset], strlen(f[*offset]) + 1))
        return -EFAULT;
    else
        return 0;

    return (strlen(f[*offset]) + 1);
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return ktime_to_ns(kt);
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH;  // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
