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

#ifndef STM32F1XX_DMA_H
#define STM32F1XX_DMA_H

#include "hw/sysbus.h"

#define TYPE_STM32F1XX_DMA "stm32f1xx-dma"
#define STM32F1XX_DMA(obj) \
        OBJECT_CHECK(STM32F1XXDMAState, (obj), TYPE_STM32F1XX_DMA)

#define STM32F1XX_DMA_MAXCHANS 7
#define STM32F1XX_DMA_REQUEST_SLOTS "stm32f1xx-dma-req-slots"

typedef struct {
    uint32_t ccr;
    uint32_t cndtr;
    uint32_t reload_cndtr;
    uint32_t cpar;
    uint32_t cmar;
} STM32F1XXDMAChan;

typedef struct {
    SysBusDevice busdev;

    MemoryRegion *dma_mr;
    AddressSpace dma_as;
    MemoryRegion mmio_dma;
    uint8_t channel_count;

    STM32F1XXDMAChan chan_dma[STM32F1XX_DMA_MAXCHANS];

    uint32_t isr;
    uint32_t ifcr;
} STM32F1XXDMAState;

#endif /*STM32F1XX_DMA_H*/
