#include <linux/gpio.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <asm/uaccess.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>

#define BOOTKEYK_MAJOR 64
#define BOOTKEY_MINOR 0
#define BOOTKEY_GPIO 7
#define BUFFER_LENGTH 2

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("BOOT_KEY Driver");
MODULE_AUTHOR("DETMPS");

ssize_t bootkey_read(struct file *filep, char __user *buf, size_t count, loff_t *f_pos);
int bootkey_open(struct inode *inode, struct file *filep);
int bootkey_release(struct inode *inode, struct file *filep);

struct file_operations bootkey_fops = {
  .owner = THIS_MODULE,
  .read = bootkey_read,
  .open = bootkey_open,
  .release = bootkey_release,
};

static DECLARE_WAIT_QUEUE_HEAD(wq);
static struct cdev *cdevStruct;
static int devno;
static int bootkeyRead;
static int flag = 0;

static irqreturn_t bootkey_isr(int irq, void *dev_id)
{
  //Read BOOT_KEy
  bootkeyRead = gpio_get_value(BOOTKEY_GPIO);
  
  //Wake up
  flag = 1;
  wake_up_interruptible(&wq);
  return IRQ_HANDLED;
}

static int __init bootkey_init(void)
{
  printk(KERN_ALERT "BOOT_KEY interrupt driver loaded\n");
  
  //Make device number
  devno = MKDEV(BOOTKEYK_MAJOR, BOOTKEY_MINOR);
  
  //Request GPIO
  int err = 0;
  err = gpio_request(BOOTKEY_GPIO, "BOOT_KEY");
  
  if(err < 0)
  {
    printk(KERN_ALERT "ERROR requesting GPIO: %d\n", err);
    goto err_exit;
  }
  
  //Set GPIO direction (in or out)
  err = gpio_direction_input(BOOTKEY_GPIO);
  
  if(err < 0)
  {
    printk(KERN_ALERT "ERROR changing GPIO direction: %d\n", err);
    goto err_freegpio;
  }
  
  //Register device
  err = register_chrdev_region(devno, 1, "BOOT_KEY");
  
  if(err < 0)
  {
    printk(KERN_ALERT "ERROR register cdev: %d\n", err);
    goto err_freegpio;
  }
  
  //Cdev init
  cdevStruct = cdev_alloc();
  cdev_init(cdevStruct, &bootkey_fops);
  
  //Add Cdev
  err = cdev_add(cdevStruct, devno, 1);
  if(err < 0)
  {
    printk(KERN_ALERT "ERROR adding cdev: %d\n", err);
    goto err_unregister_chardev;
  }
  
  //Succes
  printk(KERN_ALERT "BOOT_KEY interrupt driver loaded successfully\n");
  return 0;
  
  //Goto erros
  err_unregister_chardev:
    unregister_chrdev_region(devno, 1);
  err_freegpio:
    gpio_free(BOOTKEY_GPIO); 
  err_exit:
    return err;
  
}

static void __exit bootkey_exit(void)
{ 
  //Delete Cdev
  cdev_del(cdevStruct);

  //Unregister device
  unregister_chrdev_region(devno, 1);
  
  //Free GPIO
  gpio_free(BOOTKEY_GPIO);
  
  //Succes
  printk(KERN_ALERT "BOOT_KEY interrupt driver unloaded\n");
}

module_init(bootkey_init);
module_exit(bootkey_exit);

int bootkey_open(struct inode *inode, struct file *filep)
{
  //Reading Major and Minor
  int major, minor;
  major = MAJOR(inode->i_rdev);
  minor = MINOR(inode->i_rdev);
  
  //Printing
  printk("Opening MyGpio Device:\n- Major: %i\n- Minor: %i\n\n", major, minor);
  
  //Init interrupt
  unsigned int IRQ = gpio_to_irq(BOOTKEY_GPIO);
  printk("Request IRQ %d for BOOT_KEY.\n", IRQ);
  int err = request_irq(IRQ, bootkey_isr, IRQF_TRIGGER_RISING|IRQF_TRIGGER_FALLING, "BOOT_KEY IRQ", NULL); //NULL fordi den ikke er delt
  if(err < 0)
  {
    printk(KERN_ALERT "ERROR could not request interrupt: %d.\n", err);
  }
  
  return 0;
}

int bootkey_release(struct inode *inode, struct file *filep)
{
  //Reading Major and Minor
  int minor, major;
  major = MAJOR(inode->i_rdev);
  minor = MINOR(inode->i_rdev);
  
  //Printing
  printk("Closing/Releasing MyGpio Device: \n- Major: %i\n- Minor: %i\n\n", major, minor);
  
  //Release interrupt
  unsigned int IRQ = gpio_to_irq(BOOTKEY_GPIO);
  free_irq(IRQ, NULL);
  
  return 0;
}

ssize_t bootkey_read(struct file *filep, char __user *buf, size_t count, loff_t *f_pos)
{
  wait_event_interruptible(wq, flag == 1);
  flag = 0;
  
  char buffer[BUFFER_LENGTH];
  int bufferLength = sizeof(buffer);
  
  bufferLength = sprintf(buffer, "%i\n", bootkeyRead);
  
  if(copy_to_user(buf, buffer, bufferLength) != 0)
  {
    printk(KERN_ALERT "Could not copy to user space..\n");
  }
  
  //Flytter fil position
  *f_pos += bufferLength;
  
  return bufferLength;
}