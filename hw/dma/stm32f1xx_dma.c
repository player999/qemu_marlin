/*
 * STM32F1XX DMA emulation (c) 2020 Taras Zakharchenko
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
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "migration/vmstate.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "hw/irq.h"
#include "hw/dma/stm32f1xx_dma.h"

#define DMA_ISR (0x00000000)
#define DMA_IFCR (0x00000004)

#define DMA_CCR (0x00000000)
#define DMA_CNDTR (0x00000004)
#define DMA_CPAR (0x00000008)
#define DMA_CMAR (0x0000000C)

#define DMA_CCR_EN_MASK (0x1)
#define DMA_CCR_EN_SHIFT (0x0)
#define DMA_CCR_EN (DMA_CCR_EN_MASK << DMA_CCR_EN_SHIFT)

#define DMA_CCR_TCIE_MASK (0x1)
#define DMA_CCR_TCIE_SHIFT (0x1)
#define DMA_CCR_TCIE (DMA_CCR_TCIE_MASK << DMA_CCR_TCIE_SHIFT)

#define DMA_CCR_HTIE_MASK (0x1)
#define DMA_CCR_HTIE_SHIFT (0x2)
#define DMA_CCR_HTIE (DMA_CCR_HTIE_MASK << DMA_CCR_HTIE_SHIFT)

#define DMA_CCR_TEIE_MASK (0x1)
#define DMA_CCR_TEIE_SHIFT (0x3)
#define DMA_CCR_TEIE (DMA_CCR_TEIE_MASK << DMA_CCR_TEIE_SHIFT)

#define DMA_CCR_DIR_MASK (0x1)
#define DMA_CCR_DIR_SHIFT (0x4)
#define DMA_CCR_DIR (DMA_CCR_DIR_MASK << DMA_CCR_DIR_SHIFT)

#define DMA_CCR_CIRC_MASK (0x1)
#define DMA_CCR_CIRC_SHIFT (0x5)
#define DMA_CCR_CIRC (DMA_CCR_CIRC_MASK << DMA_CCR_CIRC_SHIFT)

#define DMA_CCR_PINC_MASK (0x1)
#define DMA_CCR_PINC_SHIFT (0x6)
#define DMA_CCR_PINC (DMA_CCR_PINC_MASK << DMA_CCR_PINC_SHIFT)

#define DMA_CCR_MINC_MASK (0x1)
#define DMA_CCR_MINC_SHIFT (0x7)
#define DMA_CCR_MINC (DMA_CCR_MINC_MASK << DMA_CCR_MINC_SHIFT)

#define DMA_CCR_PSIZE_MASK (0x3)
#define DMA_CCR_PSIZE_SHIFT (0x8)
#define DMA_CCR_PSIZE (DMA_CCR_PSIZE_MASK << DMA_CCR_PSIZE_SHIFT)

#define DMA_CCR_MSIZE_MASK (0x3)
#define DMA_CCR_MSIZE_SHIFT (0xA)
#define DMA_CCR_MSIZE (DMA_CCR_MSIZE_MASK << DMA_CCR_MSIZE_SHIFT)

#define DMA_CCR_PL_MASK (0x3)
#define DMA_CCR_PL_SHIFT (0xC)
#define DMA_CCR_PL (DMA_CCR_PL_MASK << DMA_CCR_PL_SHIFT)

#define DMA_CCR_MEM2MEM_MASK (0x1)
#define DMA_CCR_MEM2MEM_SHIFT (0xE)
#define DMA_CCR_MEM2MEM (DMA_CCR_MEM2MEM_MASK << DMA_CCR_MEM2MEM_SHIFT)

#define DMA_DATASIZE_8 (0x0)
#define DMA_DATASIZE_16 (0x1)
#define DMA_DATASIZE_32 (0x2)

static uint64_t stm32f1xx_dma_read(void *opaque, hwaddr addr, unsigned int size)
{
    STM32F1XXDMAState *s = opaque;
    if(DMA_ISR == addr) {
        return s->isr;
    }
    else if(DMA_IFCR == addr) {
        return s->ifcr;
    }
    else {
        uint8_t chan_idx = (addr - 0x4*2) / 20;
        uint8_t reg_offset = addr - 0x4*2 - 20 * chan_idx;
        if(chan_idx >= s->channel_count) return 0;

        switch (reg_offset)
        {
            case DMA_CCR:
                return s->chan_dma[chan_idx].ccr;
            case DMA_CNDTR:
                return s->chan_dma[chan_idx].cndtr;
            case DMA_CPAR:
                return s->chan_dma[chan_idx].cpar;
            case DMA_CMAR:
                return s->chan_dma[chan_idx].cmar;
            default:
                break;
        }
    }
    return 0;
}


static void stm32f1xx_dma_write(void *opaque, hwaddr addr, uint64_t val64, unsigned int size)
{
    uint32_t val32 = val64 & 0xFFFFFFFF;
    STM32F1XXDMAState *s = opaque;

    if(DMA_ISR == addr) {
        s->isr = val32;
    }
    else if(DMA_IFCR == addr) {
        s->ifcr = val32;
    }
    else {
        uint8_t chan_idx = (addr - 0x4*2) / 20;
        uint8_t reg_offset = addr - 0x4*2 - 20 * chan_idx;
        if(chan_idx >= s->channel_count) return;

        switch (reg_offset)
        {
            case DMA_CCR:
                s->chan_dma[chan_idx].ccr = val32;
                break;
            case DMA_CNDTR:
                s->chan_dma[chan_idx].cndtr = val32;
                s->chan_dma[chan_idx].reload_cndtr = val32;
                break;
            case DMA_CPAR:
                s->chan_dma[chan_idx].cpar = val32;
                break;
            case DMA_CMAR:
                s->chan_dma[chan_idx].cmar = val32;
                break;
            default:
                break;
        }
    }
}

static void handle_dma_request(void *opaque, int channel, int level)
{
    STM32F1XXDMAState *s = opaque;
    STM32F1XXDMAChan *chan;
    uint8_t dir;
    uint8_t msize;
    uint8_t psize;
    uint8_t src_size;
    uint8_t dst_size;
    uint32_t mmask, pmask;
    uint32_t data;
    uint32_t dst_data;

    chan = &s->chan_dma[channel];

    //Invalid channel index
    if((channel < 0) || (channel >= s->channel_count)) return;
    if(0 == (chan->ccr & DMA_CCR_EN)) return;
    if(0 == chan->cndtr) return;

    dir = (chan->ccr & DMA_CCR_DIR);
    psize = (chan->ccr & DMA_CCR_PSIZE) >> DMA_CCR_PSIZE_SHIFT;
    msize = (chan->ccr & DMA_CCR_MSIZE) >> DMA_CCR_MSIZE_SHIFT;

    if (psize == DMA_DATASIZE_8)         pmask = 0xFFFFFFFF;
    else if (psize == DMA_DATASIZE_16)   pmask = 0xFFFFFFFE;
    else if (psize == DMA_DATASIZE_32)   pmask = 0xFFFFFFFC;

    if (msize == DMA_DATASIZE_8)         mmask = 0xFFFFFFFF;
    else if (msize == DMA_DATASIZE_16)   mmask = 0xFFFFFFFE;
    else if (msize == DMA_DATASIZE_32)   mmask = 0xFFFFFFFC;

    if(dir)
    {
        data = ldl_le_phys(&s->dma_as, chan->cmar & mmask);
        dst_data = ldl_le_phys(&s->dma_as, chan->cpar & pmask);
        src_size = msize; dst_size = psize;

    }
    else
    {
        data = ldl_le_phys(&s->dma_as, chan->cpar & pmask);
        dst_data = ldl_le_phys(&s->dma_as, chan->cmar & mmask);
        src_size = psize; dst_size = msize;
    }

    if (dst_size == DMA_DATASIZE_8)         dst_data &= 0xFFFFFF00;
    else if (dst_size == DMA_DATASIZE_16)   dst_data &= 0xFFFF0000;
    else if (dst_size == DMA_DATASIZE_32)   dst_data &= 0x00000000;

    if (src_size == DMA_DATASIZE_8)         dst_data |= data & 0x000000FF;
    else if (src_size == DMA_DATASIZE_16)   dst_data |= data & 0x0000FFFF;
    else if (src_size == DMA_DATASIZE_32)   dst_data |= data & 0xFFFFFFFF;

    if(dir)
    {
        stl_le_phys(&s->dma_as, chan->cpar & pmask, dst_data);
    }
    else
    {
        stl_le_phys(&s->dma_as, chan->cmar & mmask, dst_data);
    }

    if(chan->ccr & DMA_CCR_MINC) chan->cmar++;
    if(chan->ccr & DMA_CCR_PINC) chan->cpar++;

    chan->cndtr--;
    if(chan->cndtr == 0)
    {
        if(chan->ccr & DMA_CCR_CIRC)
        {
            chan->cndtr = chan->reload_cndtr;
        }
    }
}

static const MemoryRegionOps stm32f1xx_dma_ops = {
    .read = stm32f1xx_dma_read,
    .write = stm32f1xx_dma_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .valid.min_access_size = 4,
    .valid.max_access_size = 4,
};

static const VMStateDescription vmstate_stm32f1xx_dma_chan = {
    .name = TYPE_STM32F1XX_DMA "-chan",
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(ccr, STM32F1XXDMAChan),
        VMSTATE_UINT32(cndtr, STM32F1XXDMAChan),
        VMSTATE_UINT32(reload_cndtr, STM32F1XXDMAChan),
        VMSTATE_UINT32(cpar, STM32F1XXDMAChan),
        VMSTATE_UINT32(cmar, STM32F1XXDMAChan),
        VMSTATE_END_OF_LIST()
    }
};

static const VMStateDescription vmstate_stm32f1xx_dma = {
    .name = TYPE_STM32F1XX_DMA,
    .version_id = 1,
    .minimum_version_id = 1,
    .fields = (VMStateField[]) {
        VMSTATE_STRUCT_ARRAY(chan_dma, STM32F1XXDMAState, STM32F1XX_DMA_MAXCHANS, 1, vmstate_stm32f1xx_dma_chan, STM32F1XXDMAChan),
        VMSTATE_UINT32(isr, STM32F1XXDMAState),
        VMSTATE_UINT32(ifcr, STM32F1XXDMAState),
        VMSTATE_UINT8(channel_count, STM32F1XXDMAState),
        VMSTATE_END_OF_LIST()
    }
};

static void stm32f1xx_dma_init(Object *obj)
{
    STM32F1XXDMAState *s = STM32F1XX_DMA(obj);

    memory_region_init_io(&s->mmio_dma, OBJECT(s), &stm32f1xx_dma_ops, s,
                          TYPE_STM32F1XX_DMA, 0x400);
    sysbus_init_mmio(SYS_BUS_DEVICE(s), &s->mmio_dma);

    qdev_init_gpio_in_named(DEVICE(s), handle_dma_request, STM32F1XX_DMA_REQUEST_SLOTS, STM32F1XX_DMA_MAXCHANS);
}

static void stm32f1xx_dma_reset(DeviceState *dev)
{
    STM32F1XXDMAState *s = STM32F1XX_DMA(dev);
    s->isr = 0;
    s->ifcr = 0;
    memset((void*)s->chan_dma, 0, sizeof(s->chan_dma));
}

static void stm32f1xx_dma_realize(DeviceState *dev, Error **errp)
{
    STM32F1XXDMAState *s = STM32F1XX_DMA(dev);
    Object *obj;
    Error *err = NULL;

    obj = object_property_get_link(OBJECT(dev), "dma-mr", &err);
    if (obj == NULL) return;
    s->dma_mr = MEMORY_REGION(obj);
    address_space_init(&s->dma_as, s->dma_mr, TYPE_STM32F1XX_DMA "-memory");

    stm32f1xx_dma_reset(dev);
}

static Property stm32f1xx_dma_properties[] = {
    DEFINE_PROP_UINT8("channel-count", STM32F1XXDMAState, channel_count, STM32F1XX_DMA_MAXCHANS),
    DEFINE_PROP_END_OF_LIST(),
};

static void stm32f1xx_dma_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = stm32f1xx_dma_realize;
    dc->reset = stm32f1xx_dma_reset;
    dc->vmsd = &vmstate_stm32f1xx_dma;
    device_class_set_props(dc, stm32f1xx_dma_properties);
}

static const TypeInfo stm32f1xx_dma_info = {
    .name          = TYPE_STM32F1XX_DMA,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(STM32F1XXDMAState),
    .instance_init = stm32f1xx_dma_init,
    .class_init    = stm32f1xx_dma_class_init,
};

static void stm32f1xx_dma_register_types(void)
{
    type_register_static(&stm32f1xx_dma_info);
}

type_init(stm32f1xx_dma_register_types)