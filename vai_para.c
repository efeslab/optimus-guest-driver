#include "vai_para.h"

long vai_para_open(void)
{
    return kvm_hypercall0(KVM_HC_VAI_OPEN);
}

long vai_para_close(void)
{
    return kvm_hypercall0(KVM_HC_VAI_CLOSE);
}
