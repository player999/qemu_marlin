/*
 * STM32F1XX GPIO emulation.
 *
 * Copyright (C) 2015 Jean-Christophe Dubois <jcd@tribudubois.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 or
 * (at your option) version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "qemu/osdep.h"
#include "hw/gpio/stm32f1xx_gpio.h"
#include "hw/irq.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qemu/timer.h"
#include "qapi/qapi-events-misc.h"

static uint32_t controlRegisterRead(STM32F1XXGPIOState *s, uint8_t isLow)
{
    uint8_t offset = 0;
    uint8_t ii;
    uint32_t value = 0;
    if(!isLow) offset = GPIO_PIN_COUNT / 2;
    for(ii = 0; ii < (GPIO_PIN_COUNT / 2); ii++)
    {
        value |= (s->cnf[offset + ii] & 0x3) << (ii * 4 + 0);
        value |= (s->mode[offset + ii] & 0x3) << (ii * 4 + 2);
    }
    return value;
}

static void controlRegisterWrite(STM32F1XXGPIOState *s, uint8_t isLow, uint32_t regValue)
{
    uint8_t offset = 0;
    uint8_t ii;
    if(!isLow) offset = GPIO_PIN_COUNT / 2;

    for(ii = 0; ii < (GPIO_PIN_COUNT / 2); ii++)
    {
        s->cnf[offset + ii] = (regValue >> (ii * 4 + 0)) & 0x3;
        s->mode[offset + ii] = (regValue >> (ii * 4 + 2)) & 0x3;
    }
}

static uint32_t readPortOutputData(STM32F1XXGPIOState *s)
{
    uint32_t value;
    uint8_t ii;
    for(ii = 0; ii < GPIO_PIN_COUNT; ii++)
    {
        value |= s->port[ii]?(1 << ii):0;
    }
    return value;
}

#pragma pack(push,1)
struct STM32F1XX_GPIO_MESSAGE
{
    uint32_t    length;
    int64_t     time;
    uint16_t    message_type;
    uint8_t     port_id;
    uint8_t     pin;
    uint8_t     value;
};
#pragma pack(pop)

static void writePortOutputData(STM32F1XXGPIOState *s, uint32_t val)
{
    uint32_t ii = 0;
    do {
        uint8_t v = val & 1;
        if(v != s->port[ii])
        {
            qapi_event_send_gpio_pin_change(s->port_id, ii, v);
        }
        s->port[ii++] = v;
    } while (val >>= 1);
}

static void writeSetReset(STM32F1XXGPIOState *s, uint16_t reset, uint16_t set)
{
    uint32_t ii = 0;
    for(ii = 0; ii < GPIO_PIN_COUNT; ii++)
    {
        uint8_t isReset = reset & 1;
        uint8_t isSet = set & 1;

        if(isSet)
        {
            if(0 == s->port[ii])
                qapi_event_send_gpio_pin_change(s->port_id, ii, s->port[ii]);
            s->port[ii] = 1;
        }
        else if(isReset)
        {
            if(1 == s->port[ii])
                qapi_event_send_gpio_pin_change(s->port_id, ii, s->port[ii]);
            s->port[ii] = 0;
        }

        reset >>= 1;
        set >>= 1;
    }
}



static uint64_t stm32f1xx_gpio_read(void *opaque, hwaddr offset, unsigned size)
{
    STM32F1XXGPIOState *s = STM32F1XX_GPIO(opaque);
    uint32_t reg_value = 0;

    switch (offset) {
    case GPIO_CRL_ADDR:
        reg_value = controlRegisterRead(s, 1);
        break;
    case GPIO_CRH_ADDR:
        reg_value = controlRegisterRead(s, 0);
        break;
    case GPIO_IDR_ADDR:
        reg_value = s->idr & 0xFFFF;
        break;
    case GPIO_ODR_ADDR:
        reg_value = readPortOutputData(s);
        break;
    case GPIO_BSRR_ADDR:
        reg_value = 0;//Write only
        break;
    case GPIO_BRR_ADDR:
        reg_value = 0;//Write only
        break;
    case GPIO_LCKR_ADDR:
        reg_value = ((s->lckk) & 1) | s->lck;
        break;
    default:
        break;
    }

    return reg_value;
}

static void stm32f1xx_gpio_write(void *opaque, hwaddr offset, uint64_t value,
                           unsigned size)
{
    STM32F1XXGPIOState *s = STM32F1XX_GPIO(opaque);
    uint32_t value32 = value & 0xFFFFFFFF;
 
    switch (offset) {
    case GPIO_CRL_ADDR:
        controlRegisterWrite(s, 1, value32);
        break;
    case GPIO_CRH_ADDR:
        controlRegisterWrite(s, 0, value32);
        break;
    case GPIO_IDR_ADDR:
        /*Read only*/
        break;
    case GPIO_ODR_ADDR:
        writePortOutputData(s, value32);
        break;
    case GPIO_BSRR_ADDR:
        writeSetReset(s, (value32 >> 16) & 0xFFFF, (value32 >> 0) & 0xFFFF);
        break;
    case GPIO_BRR_ADDR:
        writeSetReset(s, (value32 >> 0) & 0xFFFF, 0);
        break;
    case GPIO_LCKR_ADDR:
        s->lckk = (value32 & 0x10000)?1:0;
        s->lck = value32 & 0xFFFF;
        break;
    default:
        break;
    }

    return;
}

static const MemoryRegionOps stm32f1xx_gpio_ops = {
    .read = stm32f1xx_gpio_read,
    .write = stm32f1xx_gpio_write,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static const VMStateDescription vmstate_stm32f1xx_gpio = {
    .name = TYPE_STM32F1XX_GPIO,
    .version_id = 1,
    .minimum_version_id = 1,
    .minimum_version_id_old = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT8_ARRAY(    cnf,        STM32F1XXGPIOState, GPIO_PIN_COUNT),
        VMSTATE_UINT8_ARRAY(    mode,       STM32F1XXGPIOState, GPIO_PIN_COUNT),
        VMSTATE_UINT8_ARRAY(    port,       STM32F1XXGPIOState, GPIO_PIN_COUNT),
        VMSTATE_UINT16(         idr,        STM32F1XXGPIOState),
        VMSTATE_UINT16(         lck,        STM32F1XXGPIOState),
        VMSTATE_UINT8(          lckk,       STM32F1XXGPIOState),
        VMSTATE_UINT8(          port_id,    STM32F1XXGPIOState),
        VMSTATE_END_OF_LIST()
    }
};

static Property stm32f1xx_gpio_properties[] = {
    DEFINE_PROP_UINT8("port-id", STM32F1XXGPIOState, port_id, 'A'),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32f1xx_gpio_reset(DeviceState *dev)
{
    STM32F1XXGPIOState *s = STM32F1XX_GPIO(dev);

    memset(s->cnf,  IMODE_FLOATING_INPUT,   GPIO_PIN_COUNT);
    memset(s->mode, OMODE_INPUT,            GPIO_PIN_COUNT);
    memset(s->port, 0,                      GPIO_PIN_COUNT);
    s->idr          = 0;
    s->lck          = 0;
    s->lckk         = 0;
}

static void stm32f1xx_gpio_realize(DeviceState *dev, Error **errp)
{
    STM32F1XXGPIOState *s = STM32F1XX_GPIO(dev);

    memory_region_init_io(&s->iomem, OBJECT(s), &stm32f1xx_gpio_ops, s,
                          TYPE_STM32F1XX_GPIO, GPIO_MEM_SIZE);

    sysbus_init_mmio(SYS_BUS_DEVICE(dev), &s->iomem);
}

static void stm32f1xx_gpio_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32f1xx_gpio_realize;
    dc->reset = stm32f1xx_gpio_reset;
    device_class_set_props(dc, stm32f1xx_gpio_properties);
    dc->vmsd = &vmstate_stm32f1xx_gpio;
    dc->desc = "STM32F1XX GPIO controller";
}

static const TypeInfo stm32f1xx_gpio_info = {
    .name = TYPE_STM32F1XX_GPIO,
    .parent = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F1XXGPIOState),
    .class_init = stm32f1xx_gpio_class_init,
};

static void stm32f1xx_gpio_register_types(void)
{
    type_register_static(&stm32f1xx_gpio_info);
}

type_init(stm32f1xx_gpio_register_types)
