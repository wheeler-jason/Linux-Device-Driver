#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/fs.h>            //does file system stuff
#include <linux/init.h>
#include <asm/uaccess.h>

#define DEVICE_NAME “mod1_device”
#define BUFFER_SIZE 2048         // 2 KB buffer

MODULE_LICENSE("GPL");            
MODULE_AUTHOR("Chad Armstrong, Jason Wheeler, Connor Tibedo");   
MODULE_DESCRIPTION("A simple Linux character mode device driver"); 

static int deviceNumber;
static int isOpen = 0;
static char message[BUFFER_SIZE] = {0};
static int message_size = 0; // number of characters currently in our message buffer

static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops =
{
   .open = dev_open,
   .read = dev_read,
   .write = dev_write,
   .release = dev_release,
};

/*
	First function that is called.
	Registers the device's major number.
	
	@return 0 if everything is ok, else return the error number 
*/
static int __init mod1_init(void) {
    printk(KERN_INFO “INIT: Initializing the device: %s\n”, DEVICE_NAME);

    deviceNumber = register_chrdev(0, DEVICE_NAME, &fops);
    if(deviceNumber < 0) {
        printk(KERN_ALERT “INIT: Error registering device: %s\n”, DEVICE_NAME);
		return deviceNumber;
    }

    printk(KERN_INFO “INIT: Successfully registered device ‘%s’ with major number ‘%d’\n”, DEVICE_NAME, deviceNumber);
    printk(KERN_INFO “INIT: Run 'mknod /dev/%s c %d 0' to create the device file.\n”, DEVICE_NAME, deviceNumber);

    return 0;
}

/*
	Last function that is called.
	Unregisters the device's major number.
*/ 
static void __exit mod1_exit(void) {
    unregister_chrdev(deviceNumber, DEVICE_NAME);

	printk(KERN_INFO “EXIT: Device unregistered: %s.\n”, DEVICE_NAME);
}

/*
	Attempts to open the device for read/write operations.
	
	@return 0 if successfully opened, else return busy signal
*/
static int dev_open(struct inode *inodep, struct file *filep) {
    // it’s already opened!
    if(isOpen)
        return -EBUSY;

     isOpen = 1;
     printk(KERN_INFO “DEV-OPEN: Device has been opened: %s\n”, DEVICE_NAME);

     return 0;
}

/*
	Read message from device.
	
	@return 0 if whole message was read, else return the number of characters read
*/
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int error_count = 0;
    int i, availBufferSpace, numCharsInBuffer = 0;

    for (i = 0; i < len; i++) {
        if (buffer[i] != ‘\0’) 
            numCharsInBuffer++;
        else 
            break;
    }
    
    availBufferSpace = len - numCharsInBuffer;
    // overflow condition met
    if (message_size > availBufferSpace) {
        error_count = copy_to_user(buffer, message, availBufferSpace);
        

        printk(KERN_INFO “DEV-READ: Overflow resulted in failure to send %d characters to the user\n", message_size-availBufferSpace);

        // shift elements in message array over to the left to remove used data 
        for (i = 0; i < availBufferSpace; i++) {
            message[i] = message[i+availBufferSpace];
        }
        message[i] = ‘\0’;
		message_size = strlen(message);    
        return message_size;  
    }
    // no overflow
    else {
        error_count = copy_to_user(buffer, message, message_size);
        memset(message, ‘\0’, sizeof message);
        message_size = 0;
        printk(KERN_INFO "DEV-READ: Successfully sent the full message to the user.\n");
    }
    
    return 0;
}

/*
	Send message from the user to the device
	
	@return the number of characters written to the device
*/
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    int availMessageSpace = BUFFER_SIZE - message_size; 
    int i, numCharsInBuffer = 0;
    
    for (i = 0; i < len; i++) {
        if (buffer[i] != ‘\0’) 
            numCharsInBuffer++;
        else 
            break;
    }

    // Overflow condition met
	if(message_size + numCharsInBuffer > BUFFER_SIZE) {
        
		// stores availMessageSpace # of characters into message from user’s buffer
		strncat(message, buffer, availMessageSpace);
        message_size = strlen(message); 
        printk(KERN_INFO "DEV-WRITE: Not enough buffer space so partial writing %d characters from the user\n", availMessageSpace);

		return availMessageSpace;
	} else {

		strncat(message, buffer, numCharsInBuffer);
        message_size = strlen(message);
        printk(KERN_INFO "DEV-WRITE: Successfully wrote full message from user.\n");
    }

    return numCharsInBuffer;
}

/*
	Closes the device when finished reading/writing.
*/
static int dev_release(struct inode *inodep, struct file *filep){
    isOpen = 0;
    printk(KERN_INFO “Device has been released: %s\n”, DEVICE_NAME);
   
    return 0;
}

// First and last functions that are called
module_init(mod1_init);
module_exit(mod1_exit);
