/*
 * STM32F1XX GPIO registers definition.
 *
 * Copyright (C) 2020 Taras Zakharchenko <taras.zakharchenko@gmail.com>
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

#ifndef STM32F1XX_GPIO_H
#define STM32F1XX_GPIO_H

#include "hw/sysbus.h"
#include <mqueue.h>

#define TYPE_STM32F1XX_GPIO "stm32f1xx.gpio"

#define STM32F1XX_GPIO(obj) OBJECT_CHECK(STM32F1XXGPIOState, (obj), TYPE_STM32F1XX_GPIO)

#define GPIO_MEM_SIZE    0x400

#define GPIO_CRL_ADDR    0x00
#define GPIO_CRH_ADDR    0x04
#define GPIO_IDR_ADDR    0x08
#define GPIO_ODR_ADDR    0x0C
#define GPIO_BSRR_ADDR   0x10
#define GPIO_BRR_ADDR    0x14
#define GPIO_LCKR_ADDR   0x18

#define GPIO_PIN_COUNT   0x10


enum InPinMode {
    /* Input Mode */
    IMODE_GPO_PP             = 0,
    IMODE_GPO_OD             = 1,
    IMODE_AF_PP              = 2,   
    IMODE_AF_OD              = 3,
    /* Output Mode */
    IMODE_ANALOG_MODE        = 0,
    IMODE_FLOATING_INPUT     = 1,
    IMODE_INPUT_WITH_PULL    = 2,
    IMODE_RESERVED           = 3,
};

enum OutPinMode {
    OMODE_INPUT     = 0,
    OMODE_10MHZ     = 1,
    OMODE_2MHZ      = 2,
    OMODE_50MHZ     = 3,
};

typedef struct STM32F1XXGPIOState {
    /*< private >*/
    SysBusDevice parent_obj;

    uint8_t port_id;

    /*< public >*/
    MemoryRegion iomem;

    uint8_t  cnf[GPIO_PIN_COUNT];
    uint8_t  mode[GPIO_PIN_COUNT];
    uint8_t  port[GPIO_PIN_COUNT];
    uint16_t idr;
    uint16_t lck;
    uint8_t  lckk;
} STM32F1XXGPIOState;

#endif /* STM32F1XX */
