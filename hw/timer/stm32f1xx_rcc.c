/*
 * STM32F2XX RCC
 *
 * Copyright (c) 2020 Taras Zakharchenko <taras.zakharchenko@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "qemu/osdep.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "hw/timer/stm32f1xx_rcc.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"


static void stm32f1xx_rcc_reset(DeviceState *dev)
{
    STM32F1XXRCCState *s = STM32F1XXRCC(dev);

    s->rcc_cr.value = 0;
    s->rcc_cfgr.value = 0;
    s->rcc_cir = 0;
    s->rcc_abp2rstr = 0;
    s->rcc_abp1rstr = 0;
    s->rcc_ahbenr = 0;
    s->rcc_abp2enr = 0;
    s->rcc_abp1enr = 0;
    s->rcc_bdcr = 0;
    s->rcc_csr = 0;
}

static uint64_t stm32f1xx_rcc_read(void *opaque, hwaddr offset,
                           unsigned size)
{
    STM32F1XXRCCState *s = opaque;

    switch (offset) {
    case RCC_CR:
        return s->rcc_cr.value;
    case RCC_CFGR:
        return s->rcc_cfgr.value;
    case RCC_CIR:
        return s->rcc_cir;
    case RCC_APB2RSTR:
        return s->rcc_abp2rstr;
    case RCC_APB1RSTR:
        return s->rcc_abp1rstr;
    case RCC_AHBENR:
        return s->rcc_ahbenr;
    case RCC_APB2ENR:
        return s->rcc_abp2enr;
    case RCC_APB1ENR:
        return s->rcc_abp1enr;
    case RCC_BDCR:
        return s->rcc_bdcr;
    case RCC_CSR:
        return s->rcc_csr;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, offset);
    }

    return 0;
}

static void stm32f1xx_rcc_write(void *opaque, hwaddr offset,
                        uint64_t val64, unsigned size)
{
    STM32F1XXRCCState *s = opaque;
    uint32_t value = val64 & 0xFFFFFFFF;

    switch (offset) {
    case RCC_CR:
        s->rcc_cr.value = value;
        s->rcc_cr.fields.hsi_rdy = s->rcc_cr.fields.hsi_on;
        s->rcc_cr.fields.hse_rdy = s->rcc_cr.fields.hse_on;
        s->rcc_cr.fields.pll_rdy = s->rcc_cr.fields.pll_on;
        return;
    case RCC_CFGR:
        s->rcc_cfgr.value = value;
        s->rcc_cfgr.fields.sws = s->rcc_cfgr.fields.sw;
        return;
    case RCC_CIR:
        s->rcc_cir = value;
        return;
    case RCC_APB2RSTR:
        s->rcc_abp2rstr = value;
        return;
    case RCC_APB1RSTR:
        s->rcc_abp1rstr = value;
        return;
    case RCC_AHBENR:
        s->rcc_ahbenr = value;
        return;
    case RCC_APB2ENR:
        s->rcc_abp2enr = value;
        return;
    case RCC_APB1ENR:
        s->rcc_abp1enr = value;
        return;
    case RCC_BDCR:
        s->rcc_bdcr = value;
        return;
    case RCC_CSR:
        s->rcc_csr = value;
        break;
    default:
        qemu_log_mask(LOG_GUEST_ERROR,
                      "%s: Bad offset 0x%"HWADDR_PRIx"\n", __func__, offset);
        return;
    }
}

static const MemoryRegionOps stm32f1xx_rcc_ops = {
    .read = stm32f1xx_rcc_read,
    .write = stm32f1xx_rcc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_stm32f1xx_rcc = {
    .name = TYPE_STM32F1XX_RCC,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {    
        VMSTATE_UINT32(rcc_cr.value, STM32F1XXRCCState),
        VMSTATE_UINT32(rcc_cfgr.value, STM32F1XXRCCState),
        VMSTATE_UINT32(rcc_cir, STM32F1XXRCCState),
        VMSTATE_UINT32(rcc_abp2rstr, STM32F1XXRCCState),
        VMSTATE_UINT32(rcc_abp1rstr, STM32F1XXRCCState),
        VMSTATE_UINT32(rcc_ahbenr, STM32F1XXRCCState),
        VMSTATE_UINT32(rcc_abp2enr, STM32F1XXRCCState),
        VMSTATE_UINT32(rcc_abp1enr, STM32F1XXRCCState),
        VMSTATE_UINT32(rcc_bdcr, STM32F1XXRCCState),
        VMSTATE_UINT32(rcc_csr, STM32F1XXRCCState),
        VMSTATE_END_OF_LIST()
    }
};

static Property stm32f1xx_rcc_properties[] = {
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32f1xx_rcc_init(Object *obj)
{
    STM32F1XXRCCState *s = STM32F1XXRCC(obj);
    memory_region_init_io(&s->iomem, obj, &stm32f1xx_rcc_ops, s,
                          "stm32f1xx_rcc", 0x28);
    sysbus_init_mmio(SYS_BUS_DEVICE(obj), &s->iomem);
}

static void stm32f1xx_rcc_realize(DeviceState *dev, Error **errp)
{
    
}

static void stm32f1xx_rcc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->reset = stm32f1xx_rcc_reset;
    device_class_set_props(dc, stm32f1xx_rcc_properties);
    dc->vmsd = &vmstate_stm32f1xx_rcc;
    dc->realize = stm32f1xx_rcc_realize;
}

static const TypeInfo stm32f1xx_rcc_info = {
    .name          = TYPE_STM32F1XX_RCC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F1XXRCCState),
    .instance_init = stm32f1xx_rcc_init,
    .class_init    = stm32f1xx_rcc_class_init,
};

static void stm32f1xx_rcc_register_types(void)
{
    type_register_static(&stm32f1xx_rcc_info);
}

type_init(stm32f1xx_rcc_register_types)
