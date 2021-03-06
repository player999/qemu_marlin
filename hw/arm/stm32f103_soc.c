/*
 * STM32F103 SoC
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
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "hw/arm/stm32f103_soc.h"
#include "hw/qdev-properties.h"
#include "sysemu/sysemu.h"

/* At the moment only Timer 2 to 5 are modelled */
static const uint32_t timer_addr[STM_NUM_TIMERS] = { 0x40000000, 0x40000400, 0x40000800, 0x40000C00 };
static const uint32_t usart_addr[STM_NUM_USARTS] = {0x40013800, 0x40004400, 0x40004800, 0x40004C00, 0x40005000};
static const uint32_t adc_addr[STM_NUM_ADCS] = {0x40012400, 0x40012800, 0x40013C00};
static const uint32_t spi_addr[STM_NUM_SPIS] = {0x40013000, 0x40003800, 0x40003C00};
static const uint32_t gpio_addr[STM_NUM_GPIOS] = {  0x40010800, 0x40010C00, 0x40011000, 0x40011400,
                                                    0x40011800, 0x40011C00, 0x40012000};
static const uint32_t dma_addr[STM_NUM_DMAS] = {0x40020000, 0x40020400};
static const uint8_t dma_channel_num[STM_NUM_DMAS] = {7, 5};

/* RCC module */
static const uint32_t rcc_addr = 0x40021000;

static const int timer_irq[STM_NUM_TIMERS] = {28, 29, 30, 50};
static const int usart_irq[STM_NUM_USARTS] = {37, 38, 39, 52, 53};

#define ADC_IRQ 18
static const int spi_irq[STM_NUM_SPIS] = {35, 36, 51};

/* DMA Request map */
int8_t acd_req_map[] = {0, -1, 4};
int8_t acd_dma_map[] = {0, -1, 1};

static void stm32f103_soc_initfn(Object *obj)
{
    STM32F103State *s = STM32F103_SOC(obj);
    int i;

    sysbus_init_child_obj(obj, "armv7m", &s->armv7m, sizeof(s->armv7m),
                          TYPE_ARMV7M);

    sysbus_init_child_obj(obj, "syscfg", &s->syscfg, sizeof(s->syscfg),
                          TYPE_STM32F2XX_SYSCFG);

    for (i = 0; i < STM_NUM_DMAS; i++) {
        sysbus_init_child_obj(obj, "dma[*]", &s->dma[i],
                              sizeof(s->dma[i]), TYPE_STM32F1XX_DMA);
        object_property_add_const_link(OBJECT(&s->dma[i]), "dma-mr", OBJECT(get_system_memory()), &error_abort);
    }

    for (i = 0; i < STM_NUM_USARTS; i++) {
        sysbus_init_child_obj(obj, "usart[*]", &s->usart[i],
                              sizeof(s->usart[i]), TYPE_STM32F2XX_USART);
    }

    for (i = 0; i < STM_NUM_TIMERS; i++) {
        sysbus_init_child_obj(obj, "timer[*]", &s->timer[i],
                              sizeof(s->timer[i]), TYPE_STM32F2XX_TIMER);
    }

    s->adc_irqs = OR_IRQ(object_new(TYPE_OR_IRQ));

    for (i = 0; i < STM_NUM_ADCS; i++) {
        sysbus_init_child_obj(obj, "adc[*]", &s->adc[i], sizeof(s->adc[i]),
                              TYPE_STM32F2XX_ADC);
    }

    for (i = 0; i < STM_NUM_SPIS; i++) {
        sysbus_init_child_obj(obj, "spi[*]", &s->spi[i], sizeof(s->spi[i]),
                              TYPE_STM32F2XX_SPI);
    }

    for (i = 0; i < STM_NUM_GPIOS; i++) {
        sysbus_init_child_obj(obj, "gpio[*]", &s->gpio[i], sizeof(s->gpio[i]),
                              TYPE_STM32F1XX_GPIO);
    }

    sysbus_init_child_obj(obj, "rcc", &s->rcc, sizeof(s->rcc),
        TYPE_STM32F1XX_RCC);
}

static void stm32f103_soc_realize(DeviceState *dev_soc, Error **errp)
{
    STM32F103State *s = STM32F103_SOC(dev_soc);
    DeviceState *dev, *armv7m;
    SysBusDevice *busdev;
    Error *err = NULL;
    int i;

    MemoryRegion *system_memory = get_system_memory();
    MemoryRegion *sram = g_new(MemoryRegion, 1);
    MemoryRegion *flash = g_new(MemoryRegion, 1);
    MemoryRegion *flash_alias = g_new(MemoryRegion, 1);

    memory_region_init_rom(flash, OBJECT(dev_soc), "STM32F103.flash",
                           FLASH_SIZE, &error_fatal);
    memory_region_init_alias(flash_alias, OBJECT(dev_soc),
                             "STM32F103.flash.alias", flash, 0, FLASH_SIZE);

    memory_region_add_subregion(system_memory, FLASH_BASE_ADDRESS, flash);
    memory_region_add_subregion(system_memory, 0, flash_alias);

    memory_region_init_ram(sram, NULL, "STM32F103.sram", SRAM_SIZE,
                           &error_fatal);
    memory_region_add_subregion(system_memory, SRAM_BASE_ADDRESS, sram);

    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", 96);
    qdev_prop_set_string(armv7m, "cpu-type", s->cpu_type);
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    object_property_set_link(OBJECT(&s->armv7m), OBJECT(get_system_memory()),
                                     "memory", &error_abort);

    object_property_set_bool(OBJECT(&s->armv7m), true, "realized", &err);
    if (err != NULL) {
        error_propagate(errp, err);
        return;
    }
    //Load firmware
    armv7m_load_kernel(s->armv7m.cpu, s->firmware, FLASH_SIZE);

    /* System configuration controller */
    dev = DEVICE(&s->syscfg);
    object_property_set_bool(OBJECT(&s->syscfg), true, "realized", &err);
    if (err != NULL) {
        error_propagate(errp, err);
        return;
    }
    busdev = SYS_BUS_DEVICE(dev);
    sysbus_mmio_map(busdev, 0, 0x40013800);
    sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, 71));

    /*DMA1 and DMA2*/
    for (i = 0; i < STM_NUM_DMAS; i++)
    {
        dev = DEVICE(&s->dma[i]);

        object_property_set_uint(OBJECT(&s->dma[i]), dma_channel_num[i], "channel-count", &err);
        if (err != NULL) {
            error_propagate(errp, err);
            return;
        }

        object_property_set_bool(OBJECT(&s->dma[i]), true, "realized", &err);
        if (err != NULL) {
            error_propagate(errp, err);
            return;
        }

        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, dma_addr[i]);
    }

    /* Attach UART (uses USART registers) and USART controllers */
    for (i = 0; i < STM_NUM_USARTS; i++) {
        dev = DEVICE(&(s->usart[i]));
        qdev_prop_set_chr(dev, "chardev", serial_hd(i));
        object_property_set_bool(OBJECT(&s->usart[i]), true, "realized", &err);
        if (err != NULL) {
            error_propagate(errp, err);
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, usart_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, usart_irq[i]));
    }

    /* Timer 2 to 5 */
    for (i = 0; i < STM_NUM_TIMERS; i++) {
        dev = DEVICE(&(s->timer[i]));
        qdev_prop_set_uint64(dev, "clock-frequency", 1000000000);
        object_property_set_bool(OBJECT(&s->timer[i]), true, "realized", &err);
        if (err != NULL) {
            error_propagate(errp, err);
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, timer_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, timer_irq[i]));
    }

    /* ADC 1 to 3 */
    object_property_set_int(OBJECT(s->adc_irqs), STM_NUM_ADCS,
                            "num-lines", &err);
    object_property_set_bool(OBJECT(s->adc_irqs), true, "realized", &err);
    if (err != NULL) {
        error_propagate(errp, err);
        return;
    }
    qdev_connect_gpio_out(DEVICE(s->adc_irqs), 0,
                          qdev_get_gpio_in(armv7m, ADC_IRQ));

    for (i = 0; i < STM_NUM_ADCS; i++) {
        dev = DEVICE(&(s->adc[i]));
        object_property_set_bool(OBJECT(&s->adc[i]), true, "stm32f1xx-mode", &err);
        if (err != NULL) {
            error_propagate(errp, err);
            return;
        }

        object_property_set_bool(OBJECT(&s->adc[i]), true, "realized", &err);
        if (err != NULL) {
            error_propagate(errp, err);
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, adc_addr[i]);
        sysbus_connect_irq(busdev, 0,
                           qdev_get_gpio_in(DEVICE(s->adc_irqs), i));

        if(acd_dma_map[i] != -1)
            qdev_connect_gpio_out_named(DEVICE(busdev), STM32F2XX_ADC_DMA_REQUEST, 0,
                qdev_get_gpio_in_named(DEVICE(&(s->dma[acd_dma_map[i]])), STM32F1XX_DMA_REQUEST_SLOTS, acd_req_map[i]));
    }

    /* SPI 1 and 2 */
    for (i = 0; i < STM_NUM_SPIS; i++) {
        dev = DEVICE(&(s->spi[i]));
        object_property_set_bool(OBJECT(&s->spi[i]), true, "realized", &err);
        if (err != NULL) {
            error_propagate(errp, err);
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, spi_addr[i]);
        sysbus_connect_irq(busdev, 0, qdev_get_gpio_in(armv7m, spi_irq[i]));
    }

    /* GPIOS */
    for (i = 0; i < STM_NUM_GPIOS; i++) {
        dev = DEVICE(&(s->gpio[i]));
        object_property_set_uint(OBJECT(&s->gpio[i]), i, "port-id", &err);
        if (err != NULL) {
            error_propagate(errp, err);
            return;
        }
        object_property_set_bool(OBJECT(&s->gpio[i]), true, "realized", &err);
        if (err != NULL) {
            error_propagate(errp, err);
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, gpio_addr[i]);
    }

    {
        dev = DEVICE(&s->rcc);
        object_property_set_bool(OBJECT(&s->rcc), true, "realized", &err);
        if (err != NULL) {
            error_propagate(errp, err);
            return;
        }
        busdev = SYS_BUS_DEVICE(dev);
        sysbus_mmio_map(busdev, 0, rcc_addr);
    }
}

static Property stm32f103_soc_properties[] = {
    DEFINE_PROP_STRING("cpu-type", STM32F103State, cpu_type),
    DEFINE_PROP_STRING("firmware", STM32F103State, firmware),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32f103_soc_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32f103_soc_realize;
    device_class_set_props(dc, stm32f103_soc_properties);
}

static const TypeInfo stm32f103_soc_info = {
    .name          = TYPE_STM32F103_SOC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F103State),
    .instance_init = stm32f103_soc_initfn,
    .class_init    = stm32f103_soc_class_init,
};

static void stm32f103_soc_types(void)
{
    type_register_static(&stm32f103_soc_info);
}

type_init(stm32f103_soc_types)
