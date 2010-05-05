/* Hello world test module from LDD vol 3 */

#include <linux/init.h>
#include <linux/module.h>

MODULE_LICENSE("GPL");

static int test_init(void) {
    printk(KERN_ALERT "Hello from a symbiotic module!!\n");
    return 0;
}

static void test_exit(void) {
    printk(KERN_ALERT "Symbiotic test module unloading\n");
}

module_init(test_init);
module_exit(test_exit);
