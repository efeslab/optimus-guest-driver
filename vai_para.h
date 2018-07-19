#ifndef _VAI_PARA_H_
#define _VAI_PARA_H_

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <asm/kvm_para.h>

#define KVM_HC_VAI_OPEN 11
#define KVM_HC_VAI_CLOSE 12
#define KVM_HC_VAI_MAP 13
#define KVM_HC_VAI_UNMAP 14
#define KVM_HC_VAI_SUBMIT 15
#define KVM_HC_VAI_PULL 16

long vai_para_open(void);
long vai_para_close(void);

#endif /* _VAI_PARA_H_ */
