/*
 * STM32F2XX RCC
 *
 * Copyright (c) 2020 Taras Zakharchenko <alistair@alistair23.me>
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

#ifndef HW_STM32F1XX_RCC_H
#define HW_STM32F1XX_RCC_H

#include "hw/sysbus.h"
#include "qemu/timer.h"

#define RCC_CR          0x00
#define RCC_CFGR        0x04
#define RCC_CIR         0x08
#define RCC_APB2RSTR    0x0C
#define RCC_APB1RSTR    0x10
#define RCC_AHBENR      0x14
#define RCC_APB2ENR     0x18
#define RCC_APB1ENR     0x1C
#define RCC_BDCR        0x20
#define RCC_CSR         0x24   

#define RCC_CR_HSEON        (1 << 16)
#define RCC_CR_HSERDY       (1 << 17)
#define RCC_CR_PLLON        (1 << 24)
#define RCC_CR_PLLRDY       (1 << 25)

#define RCC_CFGR_SWS        (0x3 << 2)
#define RCC_CFGR_SW         (0x3 << 0)

#define TYPE_STM32F1XX_RCC "stm32f1xx-rcc"
#define STM32F1XXRCC(obj) OBJECT_CHECK(STM32F1XXRCCState, \
                            (obj), TYPE_STM32F1XX_RCC)

typedef struct STM32F1XXRCCState {
    /* <private> */
    SysBusDevice parent_obj;

    /* <public> */
    MemoryRegion iomem;

    /* <registers> */
    union {
        uint32_t value;
        struct
        {
            uint32_t hsi_on: 1;//0
            uint32_t hsi_rdy: 1;//1
            uint32_t reserved0:14;//2-15
            uint32_t hse_on: 1;//16
            uint32_t hse_rdy: 1;//17
            uint32_t reserved1: 6;//18-23
            uint32_t pll_on: 1;//24
            uint32_t pll_rdy: 1;//25
            uint32_t reserved2: 5;//26-31
        } fields;
    } rcc_cr;
    union {
        uint32_t value;
        struct
        {
            uint32_t sw: 2;//0-1
            uint32_t sws: 2;//2-3
            uint32_t reserved0: 28;//4-31
        } fields;
    } rcc_cfgr;
    uint32_t rcc_cir;
    uint32_t rcc_abp2rstr;
    uint32_t rcc_abp1rstr;
    uint32_t rcc_ahbenr;
    uint32_t rcc_abp2enr;
    uint32_t rcc_abp1enr;
    uint32_t rcc_bdcr;
    uint32_t rcc_csr;
} STM32F1XXRCCState;


#endif /* HW_STM32F1XX_RCC_H */
