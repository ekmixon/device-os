/*
 * Copyright (c) 2018 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "core_hal.h"
#include "service_debug.h"
#include "rtl8721d.h"
#include "hal_event.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include "hw_config.h"
#include "syshealth_hal.h"
#include "button_hal.h"
#include "hal_platform.h"
#include "dct.h"
#include "rng_hal.h"
#include "interrupts_hal.h"
#include "ota_module.h"
#include "bootloader.h"
#include <stdlib.h>
#include <malloc.h>
#include "rtc_hal.h"
#include "timer_hal.h"
#include "pinmap_impl.h"
#include "system_error.h"
#include "usb_hal.h"
#include "usart_hal.h"
#include "system_error.h"
#include "gpio_hal.h"
#include "exflash_hal.h"
#include "flash_common.h"
#include "concurrent_hal.h"
#include "user_hal.h"

#define BACKUP_REGISTER_NUM        10
static int32_t backup_register[BACKUP_REGISTER_NUM] __attribute__((section(".backup_registers")));
static volatile uint8_t rtos_started = 0;

static struct Last_Reset_Info {
    int reason;
    uint32_t data;
} last_reset_info = { RESET_REASON_NONE, 0 };

void HardFault_Handler( void ) __attribute__( ( naked ) );

void HardFault_Handler(void);
void MemManage_Handler(void);
void BusFault_Handler(void);
void UsageFault_Handler(void);

void SysTick_Handler(void);
void SVC_Handler(void);
void PendSV_Handler(void);
void SysTickOverride(void);

void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info);
void app_error_handler_bare(uint32_t err);

extern char link_heap_location, link_heap_location_end;
extern uintptr_t link_interrupt_vectors_location[];
extern uintptr_t link_ram_interrupt_vectors_location[];
extern uintptr_t link_ram_interrupt_vectors_location_end;
extern uintptr_t link_user_part_flash_end[];
extern uintptr_t link_module_info_crc_end[];

static void* new_heap_end = &link_heap_location_end;

extern void malloc_enable(uint8_t);
extern void malloc_set_heap_end(void*);
extern void* malloc_heap_end();
#if defined(MODULAR_FIRMWARE)
void* module_user_pre_init();
#endif

extern void* dynalib_table_location; // user part dynalib location
extern module_bounds_t module_user;
extern module_bounds_t module_ota;

__attribute__((externally_visible)) void prvGetRegistersFromStack( uint32_t *pulFaultStackAddress ) {
    /* These are volatile to try and prevent the compiler/linker optimising them
    away as the variables never actually get used.  If the debugger won't show the
    values of the variables, make them global my moving their declaration outside
    of this function. */
    volatile uint32_t r0;
    volatile uint32_t r1;
    volatile uint32_t r2;
    volatile uint32_t r3;
    volatile uint32_t r12;
    volatile uint32_t lr; /* Link register. */
    volatile uint32_t pc; /* Program counter. */
    volatile uint32_t psr;/* Program status register. */

    r0 = pulFaultStackAddress[ 0 ];
    r1 = pulFaultStackAddress[ 1 ];
    r2 = pulFaultStackAddress[ 2 ];
    r3 = pulFaultStackAddress[ 3 ];

    r12 = pulFaultStackAddress[ 4 ];
    lr = pulFaultStackAddress[ 5 ];
    pc = pulFaultStackAddress[ 6 ];
    psr = pulFaultStackAddress[ 7 ];

    /* Silence "variable set but not used" error */
    if (false) {
        (void)r0; (void)r1; (void)r2; (void)r3; (void)r12; (void)lr; (void)pc; (void)psr;
    }

    if (SCB->CFSR & (1<<25) /* DIVBYZERO */) {
        // stay consistent with the core and cause 5 flashes
        UsageFault_Handler();
    } else {
        PANIC(HardFault,"HardFault");

        /* Go to infinite loop when Hard Fault exception occurs */
        while (1) {
            ;
        }
    }
}


void HardFault_Handler(void) {
    __asm volatile
    (
        " tst lr, #4                                                \n"
        " ite eq                                                    \n"
        " mrseq r0, msp                                             \n"
        " mrsne r0, psp                                             \n"
        " ldr r1, [r0, #24]                                         \n"
        " ldr r2, =handler2_address_const                           \n"
        " bx r2                                                     \n"
        " handler2_address_const: .word prvGetRegistersFromStack    \n"
    );
}

void app_error_fault_handler(uint32_t _id, uint32_t _pc, uint32_t _info) {
    volatile uint32_t id = _id;
    volatile uint32_t pc = _pc;
    volatile uint32_t info = _info;
    (void)id; (void)pc; (void)info;
    PANIC(HardFault,"HardFault");
    while(1) {
        ;
    }
}

void app_error_handler_bare(uint32_t error_code) {
    PANIC(HardFault,"HardFault");
    while(1) {
        ;
    }
}

void app_error_handler(uint32_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    PANIC(HardFault,"HardFault");
    while(1) {
    }
}

void MemManage_Handler(void) {
    /* Go to infinite loop when Memory Manage exception occurs */
    PANIC(MemManage,"MemManage");
    while (1) {
        ;
    }
}

void BusFault_Handler(void) {
    /* Go to infinite loop when Bus Fault exception occurs */
    PANIC(BusFault,"BusFault");
    while (1) {
        ;
    }
}

void UsageFault_Handler(void) {
    /* Go to infinite loop when Usage Fault exception occurs */
    PANIC(UsageFault,"UsageFault");
    while (1) {
        ;
    }
}


extern void INT_HardFault_C(uint32_t mstack[], uint32_t pstack[], uint32_t lr_value, uint32_t fault_id);

void INT_HardFault_Patch_C(uint32_t mstack[], uint32_t pstack[], uint32_t lr_value, uint32_t fault_id)
{
    u8 IsPstack = 0;

    DBG_8195A("\r\nHard Fault Patch (Non-secure)\r\n");

    /* EXC_RETURN.S, 1: original is Secure, 0: original is Non-secure */
    if (lr_value & BIT(6)) {                    //Taken from S
        DBG_8195A("\nException taken from Secure to Non-secure.\nSecure stack is used to store context." 
            "It can not be dumped from non-secure side for security reason!!!\n");

        while(1);
    } else {                                    //Taken from NS
        if (lr_value & BIT(3)) {                //Thread mode
            if (lr_value & BIT(2)) {            //PSP
                IsPstack = 1;
            }
        }
    }

#if defined(CONFIG_EXAMPLE_CM_BACKTRACE) && CONFIG_EXAMPLE_CM_BACKTRACE
    cm_backtrace_fault(IsPstack ? pstack : mstack, lr_value);
    while(1);
#else

    if(IsPstack)
        mstack = pstack;

    INT_HardFault_C(mstack, pstack, lr_value, fault_id);
#endif    
    
}

void INT_HardFault_Patch(void)
{
    __ASM volatile(
        "MRS R0, MSP\n\t"
        "MRS R1, PSP\n\t"
        "MOV R2, LR\n\t" /* second parameter is LR current value */
        "MOV R3, #0\n\t"   
        "SUB.W    R4, R0, #128\n\t"
        "MSR MSP, R4\n\t"   // Move MSP to upper to for we can dump current stack contents without chage contents
        "LDR R4,=INT_HardFault_Patch_C\n\t"
        "BX R4\n\t"
    );
}

void INT_UsageFault_Patch(void)
{
    __ASM volatile(
        "MRS R0, MSP\n\t"
        "MRS R1, PSP\n\t"
        "MOV R2, LR\n\t" /* second parameter is LR current value */
        "MOV R3, #1\n\t"   
        "SUB.W    R4, R0, #128\n\t"
        "MSR MSP, R4\n\t"   // Move MSP to upper to for we can dump current stack contents without chage contents
        "LDR R4,=INT_HardFault_Patch_C\n\t"
        "BX R4\n\t"
    );
}

void INT_BusFault_Patch(void)
{
    __ASM volatile(
        "MRS R0, MSP\n\t"
        "MRS R1, PSP\n\t"
        "MOV R2, LR\n\t" /* second parameter is LR current value */
        "MOV R3, #2\n\t"   
        "SUB.W    R4, R0, #128\n\t"
        "MSR MSP, R4\n\t"   // Move MSP to upper to for we can dump current stack contents without chage contents
        "LDR R4,=INT_HardFault_Patch_C\n\t"
        "BX R4\n\t"
    );
}

void INT_MemFault_Patch(void)
{
    __ASM volatile(
        "MRS R0, MSP\n\t"
        "MRS R1, PSP\n\t"
        "MOV R2, LR\n\t" /* second parameter is LR current value */
        "MOV R3, #3\n\t"   
        "SUB.W    R4, R0, #128\n\t"
        "MSR MSP, R4\n\t"   // Move MSP to upper to for we can dump current stack contents without chage contents
        "LDR R4,=INT_HardFault_Patch_C\n\t"
        "BX R4\n\t"
    );
}

void SysTickOverride(void) {
    HAL_SysTick_Handler();
}

void SysTickChain() {
    SysTick_Handler();
    SysTickOverride();
}

/**
 * Called by HAL_Core_Init() to pre-initialize any low level hardware before
 * the main loop runs.
 */
void HAL_Core_Init_finalize(void) {
    // uint32_t* isrs = (uint32_t*)&link_ram_interrupt_vectors_location;
    // isrs[IRQN_TO_IDX(SysTick_IRQn)] = (uint32_t)SysTickChain;
    __NVIC_SetVector(SysTick_IRQn, (u32)(void*)SysTickChain);
}

void HAL_Core_Init(void) {
    HAL_Core_Init_finalize();
}

void HAL_Core_Config_systick_configuration(void) {
    // SysTick_Configuration();
    // sd_nvic_EnableIRQ(SysTick_IRQn);
    // dcd_migrate_data();
}

/**
 * Called by HAL_Core_Config() to allow the HAL implementation to override
 * the interrupt table if required.
 */
void HAL_Core_Setup_override_interrupts(void) {
    __NVIC_SetVector(SysTick_IRQn, (u32)(void*)SysTick_Handler);
    __NVIC_SetVector(SVCall_IRQn, (u32)(void*)SVC_Handler);
    __NVIC_SetVector(PendSV_IRQn, (u32)(void*)PendSV_Handler);

    __NVIC_SetVector(HardFault_IRQn, (u32)(void*)INT_HardFault_Patch);
    __NVIC_SetVector(MemoryManagement_IRQn, (u32)(void*)INT_MemFault_Patch);
    __NVIC_SetVector(BusFault_IRQn, (u32)(void*)INT_BusFault_Patch);
    __NVIC_SetVector(UsageFault_IRQn, (u32)(void*)INT_UsageFault_Patch);
}

void HAL_Core_Restore_Interrupt(IRQn_Type irqn) {
    // uint32_t handler = ((const uint32_t*)&link_interrupt_vectors_location)[IRQN_TO_IDX(irqn)];

    // // Special chain handler
    // if (irqn == SysTick_IRQn) {
    //     handler = (uint32_t)SysTickChain;
    // }

    // volatile uint32_t* isrs = (volatile uint32_t*)&link_ram_interrupt_vectors_location;
    // isrs[IRQN_TO_IDX(irqn)] = handler;
}

#if HAL_PLATFORM_PROHIBIT_XIP
static void prohibit_xip(void) {
    const uint8_t regions = (MPU->TYPE & MPU_TYPE_DREGION_Msk) >> MPU_TYPE_DREGION_Pos;
    // Disable MPU
    MPU->CTRL = 0;
    // Clear all regions
    for (uint8_t i = 0; i < regions; i++) {
        MPU->RNR    = i;
        MPU->RASR   = 0;
        MPU->RBAR   = 0;
    }

    uint8_t st = 0;
    sd_nvic_critical_region_enter(&st);

    // Forbid access to XIP
    MPU->RNR = 0;
    MPU->RBAR = EXTERNAL_FLASH_XIP_BASE;
    // XN: 1, instruction fetches disabled
    // AP: 0b000, all accesses generate a permission fault
    // TEX: 0b000, S: 0, C: 1, B: 0, as suggested by core manual for flash memory
    const uint32_t attr = 0x10050000;
    MPU->RASR = attr | (1 << MPU_RASR_ENABLE_Pos) | ((31 - __CLZ(EXTERNAL_FLASH_XIP_LENGTH) - 1) << MPU_RASR_SIZE_Pos);
    // Enable MPU
    MPU->CTRL = 0x00000005;

    sd_nvic_critical_region_exit(st);

    // Use a DSB followed by an ISB instruction to ensure that the new MPU configuration is used by subsequent instructions. 
    __DSB();
    __ISB();

    // Uncomment the code bellow will trigger hardfault
    // uint8_t data = *((uint8_t*)(EXTERNAL_FLASH_XIP_BASE + EXTERNAL_FLASH_OTA_ADDRESS));
    // LOG(ERROR, "%d", data);
}
#endif // HAL_PLATFORM_PROHIBIT_XIP

/*******************************************************************************
 * Function Name  : HAL_Core_Config.
 * Description    : Called in startup routine, before calling C++ constructors.
 * Input          : None.
 * Output         : None.
 * Return         : None.
 *******************************************************************************/
void HAL_Core_Config(void) {

    irq_table_init(MSP_RAM_HP_NS);

    DECLARE_SYS_HEALTH(ENTERED_SparkCoreConfig);

#ifdef DFU_BUILD_ENABLE
    USE_SYSTEM_FLAGS = 1;
#endif

    /* Forward interrupts */
    // memcpy(&link_ram_interrupt_vectors_location, &link_interrupt_vectors_location, (uintptr_t)&link_ram_interrupt_vectors_location_end-(uintptr_t)&link_ram_interrupt_vectors_location);
    // uint32_t* isrs = (uint32_t*)&link_ram_interrupt_vectors_location;
    // SCB->VTOR = (uint32_t)isrs;

#if HAL_PLATFORM_PROHIBIT_XIP
    prohibit_xip();
#endif
    
    Set_System();

    hal_timer_init(NULL);

    HAL_Core_Setup_override_interrupts();

    HAL_RNG_Configuration();

#if defined(MODULAR_FIRMWARE)
    const module_info_dynamic_location_ext_t* dyn = NULL;
    const uint8_t* user_end = (const uint8_t*)module_user.end_address - sizeof(uint32_t);
    const uint16_t* suffix_size = (const uint16_t*)(user_end - sizeof(uint16_t)) /* module_info_suffix_t::size */;
    for (const uint8_t* addr = user_end - *suffix_size; addr < user_end - MODULE_INFO_SUFFIX_NONEXT_DATA_SIZE;) {
        const module_info_extension_t* ext = (const module_info_extension_t*)(addr);
        if (ext->type == MODULE_INFO_EXTENSION_DYNAMIC_LOCATION) {
            if (ext->length >= sizeof(module_info_dynamic_location_ext_t)) {
                dyn = (const module_info_dynamic_location_ext_t*)ext;
                break;
            }
        } else if (ext->type == MODULE_INFO_EXTENSION_INVALID || ext->type == MODULE_INFO_EXTENSION_END) {
            break;
        }
    }

    // End of current system-part1 aligned to 4KB
    module_ota.start_address = (((uint32_t)&link_module_info_crc_end) & 0xFFFFF000) + 0x1000; // 4K aligned, erasure

    uint32_t ota_end_address = 0;
    if (dyn) {
        ota_end_address = (uint32_t)dyn->module_start_address;
        if (ota_end_address >= module_ota.start_address + 0x200000) {
            // Should have 2M for the OTA region at the least
            module_user.start_address = (uint32_t)dyn->module_start_address;
        } else {
            // Invalid user module
            dyn = NULL;
        }
    }

    if (!dyn) {
        module_user.start_address = module_user.end_address;
    }

    if (dyn) {
        // This will also perform CRC checks etc
        hal_user_module_descriptor user_desc = {};
        if (!hal_user_module_get_descriptor(&user_desc) && 0) {
            dynalib_table_location = (void*)dyn->dynalib_load_address; // dynalib table in flash
            new_heap_end = user_desc.pre_init();
            if (new_heap_end < malloc_heap_end()) {
                malloc_set_heap_end(new_heap_end);
            }
            dynalib_table_location = (void*)dyn->dynalib_start_address; // dynalib in PSRAM
        }
        module_ota.end_address = ota_end_address;
    }

    // Enable malloc before littlefs initialization.
    malloc_enable(1);
#endif

#ifdef DFU_BUILD_ENABLE
    Load_SystemFlags();
#endif

    // TODO: Use current LED theme
    if (HAL_Feature_Get(FEATURE_LED_OVERRIDDEN)) {
        // Just in case
        LED_Off(PARTICLE_LED_RGB);
    } else {
        LED_SetRGBColor(RGB_COLOR_WHITE);
        LED_On(PARTICLE_LED_RGB);
    }
}

void HAL_Core_Setup(void) {
    /* DOES NOT DO ANYTHING
     * SysTick is enabled within FreeRTOS
     */
    HAL_Core_Config_systick_configuration();

    if (bootloader_update_if_needed()) {
        HAL_Core_System_Reset();
    }

    // Initialize stdlib PRNG with a seed from hardware RNG
    srand(HAL_RNG_GetRandomNumber());

    hal_rtc_init();

#if !defined(MODULAR_FIRMWARE) || !MODULAR_FIRMWARE
    module_user_init_hook();
#endif
}

#if defined(MODULAR_FIRMWARE) && MODULAR_FIRMWARE
bool HAL_Core_Validate_Modules(uint32_t flags, void* reserved)
{
    const module_bounds_t* bounds = NULL;
    hal_module_t mod;
    bool module_fetched = false;
    bool valid = false;

    // First verify bootloader module
    // The bootloader image starts with XIP header!!! Skip it for now,
#if 0
    bounds = find_module_bounds(MODULE_FUNCTION_BOOTLOADER, 0, HAL_PLATFORM_MCU_DEFAULT);
    module_fetched = fetch_module(&mod, bounds, false, MODULE_VALIDATION_INTEGRITY);

    valid = module_fetched && (mod.validity_checked == mod.validity_result);

    if (!valid) {
        return valid;
    }
#endif

    // Now check system-parts
    int i = 0;
    if (flags & 1) {
        // Validate only that system-part that depends on bootloader passes dependency check
        i = 1;
    }
    do {
        bounds = find_module_bounds(MODULE_FUNCTION_SYSTEM_PART, i++, HAL_PLATFORM_MCU_DEFAULT);
        if (bounds) {
            module_fetched = fetch_module(&mod, bounds, false, MODULE_VALIDATION_INTEGRITY);
            valid = module_fetched && (mod.validity_checked == mod.validity_result);
        }
        if (flags & 1) {
            bounds = NULL;
        }
    } while(bounds != NULL && valid);

    return valid;
}
#endif

bool HAL_Core_Mode_Button_Pressed(uint16_t pressedMillisDuration) {
    bool pressedState = false;

    if(hal_button_get_debounce_time(HAL_BUTTON1) >= pressedMillisDuration) {
        pressedState = true;
    }
    if(hal_button_get_debounce_time(HAL_BUTTON1_MIRROR) >= pressedMillisDuration) {
        pressedState = true;
    }

    return pressedState;
}

void HAL_Core_Mode_Button_Reset(uint16_t button)
{
}

void HAL_Core_System_Reset_Ex(int reason, uint32_t data, void *reserved) {
    if (HAL_Feature_Get(FEATURE_RESET_INFO)) {
        // Save reset info to backup registers
        HAL_Core_Write_Backup_Register(BKP_DR_02, reason);
        HAL_Core_Write_Backup_Register(BKP_DR_03, data);
    }

    HAL_Core_System_Reset();
}

void HAL_Core_Factory_Reset(void) {
    system_flags.Factory_Reset_SysFlag = 0xAAAA;
    Save_SystemFlags();
    HAL_Core_System_Reset_Ex(RESET_REASON_FACTORY_RESET, 0, NULL);
}

void HAL_Core_Enter_Safe_Mode(void* reserved) {
    HAL_Core_Write_Backup_Register(BKP_DR_01, ENTER_SAFE_MODE_APP_REQUEST);
    HAL_Core_System_Reset_Ex(RESET_REASON_SAFE_MODE, 0, NULL);
}

bool HAL_Core_Enter_Safe_Mode_Requested(void)
{
    Load_SystemFlags();
    uint8_t flags = SYSTEM_FLAG(StartupMode_SysFlag);

    return (flags & 1);
}

void HAL_Core_Enter_Bootloader(bool persist) {
    if (persist) {
        HAL_Core_Write_Backup_Register(BKP_DR_10, 0xFFFF);
        system_flags.FLASH_OTA_Update_SysFlag = 0xFFFF;
        Save_SystemFlags();
    } else {
        HAL_Core_Write_Backup_Register(BKP_DR_01, ENTER_DFU_APP_REQUEST);
    }

    HAL_Core_System_Reset_Ex(RESET_REASON_DFU_MODE, 0, NULL);
}

void HAL_Core_Enter_Stop_Mode(uint16_t wakeUpPin, uint16_t edgeTriggerMode, long seconds) {
    // Deprecated. See: hal/src/nRF52840/sleep_hal.cpp
}

int HAL_Core_Enter_Stop_Mode_Ext(const uint16_t* pins, size_t pins_count, const InterruptMode* mode, size_t mode_count, long seconds, void* reserved) {
    // Deprecated. See: hal/src/nRF52840/sleep_hal.cpp
    return SYSTEM_ERROR_DEPRECATED;
}

void HAL_Core_Execute_Stop_Mode(void) {
    // Deprecated. See: hal/src/nRF52840/sleep_hal.cpp
}

int HAL_Core_Enter_Standby_Mode(uint32_t seconds, uint32_t flags) {
    // Deprecated. See: hal/src/nRF52840/sleep_hal.cpp
    return SYSTEM_ERROR_DEPRECATED;
}

int HAL_Core_Execute_Standby_Mode_Ext(uint32_t flags, void* reserved) {
    // Deprecated. See: hal/src/nRF52840/sleep_hal.cpp
    return SYSTEM_ERROR_DEPRECATED;
}

void HAL_Core_Execute_Standby_Mode(void) {
    // Deprecated. See: hal/src/nRF52840/sleep_hal.cpp
}

bool HAL_Core_System_Reset_FlagSet(RESET_TypeDef resetType) {
    uint32_t reset_reason = SYSTEM_FLAG(RCC_CSR_SysFlag);
    if (reset_reason == 0xffffffff) {
        // sd_power_reset_reason_get(&reset_reason);
    }
    switch(resetType) {
        case PIN_RESET: {
            // return reset_reason == NRF_POWER_RESETREAS_RESETPIN_MASK;
        }
        case SOFTWARE_RESET: {
            // return reset_reason == NRF_POWER_RESETREAS_SREQ_MASK;
        }
        case WATCHDOG_RESET: {
            // return reset_reason == NRF_POWER_RESETREAS_DOG_MASK;
        }
        case POWER_MANAGEMENT_RESET: {
            // SYSTEM OFF Mode
            // return (reset_reason == NRF_POWER_RESETREAS_OFF_MASK) || (reset_reason == NRF_POWER_RESETREAS_LPCOMP_MASK);
        }
        // If none of the reset sources are flagged, this indicates that
        // the chip was reset from the on-chip reset generator,
        // which will indicate a power-on-reset or a brownout reset.
        case POWER_DOWN_RESET:
        case POWER_BROWNOUT_RESET: {
            return reset_reason == 0;
        }
        default:
            return false;
    }
    return false;
}

static void Init_Last_Reset_Info()
{
    if (HAL_Core_System_Reset_FlagSet(SOFTWARE_RESET))
    {
        // Load reset info from backup registers
        last_reset_info.reason = HAL_Core_Read_Backup_Register(BKP_DR_02);
        last_reset_info.data = HAL_Core_Read_Backup_Register(BKP_DR_03);
        // Clear backup registers
        HAL_Core_Write_Backup_Register(BKP_DR_02, 0);
        HAL_Core_Write_Backup_Register(BKP_DR_03, 0);
    }
    else // Hardware reset
    {
        if (HAL_Core_System_Reset_FlagSet(WATCHDOG_RESET))
        {
            last_reset_info.reason = RESET_REASON_WATCHDOG;
        }
        else if (HAL_Core_System_Reset_FlagSet(POWER_MANAGEMENT_RESET))
        {
            last_reset_info.reason = RESET_REASON_POWER_MANAGEMENT; // Reset generated when entering standby mode (nRST_STDBY: 0)
        }
        else if (HAL_Core_System_Reset_FlagSet(POWER_DOWN_RESET))
        {
            last_reset_info.reason = RESET_REASON_POWER_DOWN;
        }
        else if (HAL_Core_System_Reset_FlagSet(POWER_BROWNOUT_RESET))
        {
            last_reset_info.reason = RESET_REASON_POWER_BROWNOUT;
        }
        else if (HAL_Core_System_Reset_FlagSet(PIN_RESET)) // Pin reset flag should be checked in the last place
        {
            last_reset_info.reason = RESET_REASON_PIN_RESET;
        }
        // TODO: Reset from USB, NFC, LPCOMP...
        else
        {
            last_reset_info.reason = RESET_REASON_UNKNOWN;
        }
        last_reset_info.data = 0; // Not used
    }

    // Clear Reset info register
    // sd_power_reset_reason_clr(0xFFFFFFFF);
}

int HAL_Core_Get_Last_Reset_Info(int *reason, uint32_t *data, void *reserved) {
    if (HAL_Feature_Get(FEATURE_RESET_INFO)) {
        if (reason) {
            *reason = last_reset_info.reason;
        }
        if (data) {
            *data = last_reset_info.data;
        }
        return 0;
    }
    return -1;
}

/**
 * @brief  Computes the 32-bit CRC of a given buffer of byte data.
 * @param  pBuffer: pointer to the buffer containing the data to be computed
 * @param  BufferSize: Size of the buffer to be computed
 * @retval 32-bit CRC
 */
uint32_t HAL_Core_Compute_CRC32(const uint8_t *pBuffer, uint32_t bufferSize) {
    // TODO: Use the peripheral lock?
    int is = 0;
    bool enableRsip = enable_rsip_if_disabled((uint32_t)pBuffer, &is);
    uint32_t crc32 = Compute_CRC32(pBuffer, bufferSize, NULL);
    disable_rsip_if_enabled(enableRsip, is);
    return crc32;
}

uint16_t HAL_Core_Mode_Button_Pressed_Time() {
    return 0;
}

void HAL_Bootloader_Lock(bool lock) {

}

unsigned HAL_Core_System_Clock(HAL_SystemClock clock, void* reserved) {
    return SystemCoreClock;
}


static TaskHandle_t  app_thread_handle;
#define APPLICATION_STACK_SIZE 6144
#define MALLOC_LOCK_TIMEOUT_MS (60000) // This is defined in "ticks" and each tick is 1ms

/**
 * The mutex to ensure only one thread manipulates the heap at a given time.
 */
xSemaphoreHandle malloc_mutex = 0;

static void init_malloc_mutex(void) {
    malloc_mutex = xSemaphoreCreateRecursiveMutex();
}

void __malloc_lock(struct _reent *ptr) {
    if (malloc_mutex) {
        if (!xSemaphoreTakeRecursive(malloc_mutex, MALLOC_LOCK_TIMEOUT_MS)) {
            PANIC(HeapError, "Semaphore Lock Timeout");
            while (1);
        }
    }
}

void __malloc_unlock(struct _reent *ptr) {
    if (malloc_mutex) {
        xSemaphoreGiveRecursive(malloc_mutex);
    }
}

/**
 * The entrypoint to our application.
 * This should be called from the RTOS main thread once initialization has been
 * completed, constructors invoked and and HAL_Core_Config() has been called.
 */
void application_start() {
    rtos_started = 1;
    
    // one the key is sent to the cloud, this can be removed, since the key is fetched in
    // Spark_Protocol_init(). This is just a temporary measure while the key still needs
    // to be fetched via DFU.

    HAL_Core_Setup();

    // TODO:
    // generate_key();

    if (HAL_Feature_Get(FEATURE_RESET_INFO))
    {
        // Load last reset info from RCC / backup registers
        Init_Last_Reset_Info();
    }

    app_setup_and_loop();
}

void application_task_start(void* arg) {
    application_start();
}

extern void Reset_Handler();
extern uintptr_t link_stack_end;

typedef struct entry {
    uint32_t stack;
    void*    entry;
} entry;

__attribute__((section(".system.entry"), used))
entry systemPartEntry = {
    (uint32_t)&link_stack_end,
    Reset_Handler
};

/**
 * Called from startup_stm32f2xx.s at boot, main entry point.
 */
int main(void) {
    init_malloc_mutex();
    xTaskCreate( application_task_start, "app_thread", APPLICATION_STACK_SIZE/sizeof( portSTACK_TYPE ), NULL, 2, &app_thread_handle);

    if (HAL_Feature_Get(FEATURE_LED_OVERRIDDEN)) {
        LED_Signaling_Start();
    }

    vTaskStartScheduler();

    /* we should never get here */
    while (1);

    return 0;
}

static int Write_Feature_Flag(uint32_t flag, bool value, bool *prev_value) {
    if (hal_interrupt_is_isr()) {
        return -1; // DCT cannot be accessed from an ISR
    }
    uint32_t flags = 0;
    int result = dct_read_app_data_copy(DCT_FEATURE_FLAGS_OFFSET, &flags, sizeof(flags));
    if (result != 0) {
        return result;
    }
    // NOTE: inverted logic!
    const bool cur_value = !(flags & flag);
    if (prev_value) {
        *prev_value = cur_value;
    }
    if (cur_value != value) {
        if (value) {
            flags &= ~flag;
        } else {
            flags |= flag;
        }
        result = dct_write_app_data(&flags, DCT_FEATURE_FLAGS_OFFSET, 4);
        if (result != 0) {
            return result;
        }
    }
    return 0;
}

static int Read_Feature_Flag(uint32_t flag, bool* value) {
    // if (hal_interrupt_is_isr()) {
    //     return -1; // DCT cannot be accessed from an ISR
    // }
    // uint32_t flags = 0;
    // const int result = dct_read_app_data_copy(DCT_FEATURE_FLAGS_OFFSET, &flags, sizeof(flags));
    // if (result != 0) {
    //     return result;
    // }
    // *value = !(flags & flag);
    return 0;
}

int HAL_Feature_Set(HAL_Feature feature, bool enabled) {
   switch (feature) {
        case FEATURE_RETAINED_MEMORY: {
            // TODO: Switch on backup SRAM clock
            // Switch on backup power regulator, so that it survives the deep sleep mode,
            // software and hardware reset. Power must be supplied to VIN or VBAT to retain SRAM values.
            return -1;
        }
        case FEATURE_RESET_INFO: {
            return Write_Feature_Flag(FEATURE_FLAG_RESET_INFO, enabled, NULL);
        }
#if HAL_PLATFORM_CLOUD_UDP
        case FEATURE_CLOUD_UDP: {
            const uint8_t data = (enabled ? 0xff : 0x00);
            return dct_write_app_data(&data, DCT_CLOUD_TRANSPORT_OFFSET, sizeof(data));
        }
#endif // HAL_PLATFORM_CLOUD_UDP
        case FEATURE_ETHERNET_DETECTION: {
            return Write_Feature_Flag(FEATURE_FLAG_ETHERNET_DETECTION, enabled, NULL);
        }
        case FEATURE_LED_OVERRIDDEN: {
            return Write_Feature_Flag(FEATURE_FLAG_LED_OVERRIDDEN, enabled, NULL);
        }
    }

    return -1;
}

bool HAL_Feature_Get(HAL_Feature feature) {
    switch (feature) {
        case FEATURE_CLOUD_UDP: {
            return true; // Mesh platforms are UDP-only
        }
        case FEATURE_RESET_INFO: {
            return true;
        }
        case FEATURE_ETHERNET_DETECTION: {
            bool value = false;
            return (Read_Feature_Flag(FEATURE_FLAG_ETHERNET_DETECTION, &value) == 0) ? value : false;
        }
        case FEATURE_LED_OVERRIDDEN: {
            bool value = false;
            return (Read_Feature_Flag(FEATURE_FLAG_LED_OVERRIDDEN, &value) == 0) ? value : false;
        }
    }
    return false;
}

int32_t HAL_Core_Backup_Register(uint32_t BKP_DR) {
    if ((BKP_DR == 0) || (BKP_DR > BACKUP_REGISTER_NUM)) {
        return -1;
    }

    return BKP_DR - 1;
}

void HAL_Core_Write_Backup_Register(uint32_t BKP_DR, uint32_t Data) {
    int32_t BKP_DR_Index = HAL_Core_Backup_Register(BKP_DR);
    if (BKP_DR_Index != -1) {
        backup_register[BKP_DR_Index] = Data;
    }
}

uint32_t HAL_Core_Read_Backup_Register(uint32_t BKP_DR) {
    int32_t BKP_DR_Index = HAL_Core_Backup_Register(BKP_DR);
    if (BKP_DR_Index != -1) {
        return backup_register[BKP_DR_Index];
    }
    return 0xFFFFFFFF;
}

extern size_t pvPortLargestFreeBlock();

uint32_t HAL_Core_Runtime_Info(runtime_info_t* info, void* reserved)
{
    struct mallinfo heapinfo = mallinfo();
    // fordblks  The total number of bytes in free blocks.
    info->freeheap = heapinfo.fordblks;
    if (offsetof(runtime_info_t, total_init_heap) + sizeof(info->total_init_heap) <= info->size) {
        info->total_init_heap = (uintptr_t)new_heap_end - (uintptr_t)&link_heap_location;
    }

    if (offsetof(runtime_info_t, total_heap) + sizeof(info->total_heap) <= info->size) {
        info->total_heap = heapinfo.arena;
    }

    if (offsetof(runtime_info_t, max_used_heap) + sizeof(info->max_used_heap) <= info->size) {
        info->max_used_heap = heapinfo.usmblks;
    }

    if (offsetof(runtime_info_t, user_static_ram) + sizeof(info->user_static_ram) <= info->size) {
        // info->user_static_ram = (uintptr_t)&_Stack_Init - (uintptr_t)new_heap_end;
    }

    if (offsetof(runtime_info_t, largest_free_block_heap) + sizeof(info->largest_free_block_heap) <= info->size) {
            info->largest_free_block_heap = pvPortLargestFreeBlock();
    }

    return 0;
}

uint16_t HAL_Bootloader_Get_Flag(BootloaderFlag flag)
{
    switch (flag)
    {
        case BOOTLOADER_FLAG_VERSION:
            return SYSTEM_FLAG(Bootloader_Version_SysFlag);
        case BOOTLOADER_FLAG_STARTUP_MODE:
            return SYSTEM_FLAG(StartupMode_SysFlag);
    }
    return 0;
}

int HAL_Core_Enter_Panic_Mode(void* reserved)
{
    __disable_irq();
    return 0;
}

#if HAL_PLATFORM_CLOUD_UDP

#include "dtls_session_persist.h"
#include <string.h>

SessionPersistDataOpaque session __attribute__((section(".backup_system")));

int HAL_System_Backup_Save(size_t offset, const void* buffer, size_t length, void* reserved)
{
    if (offset==0 && length==sizeof(SessionPersistDataOpaque))
    {
        memcpy(&session, buffer, length);
        return 0;
    }
    return -1;
}

int HAL_System_Backup_Restore(size_t offset, void* buffer, size_t max_length, size_t* length, void* reserved)
{
    if (offset==0 && max_length>=sizeof(SessionPersistDataOpaque) && session.size==sizeof(SessionPersistDataOpaque))
    {
        *length = sizeof(SessionPersistDataOpaque);
        memcpy(buffer, &session, sizeof(session));
        return 0;
    }
    return -1;
}


#else

int HAL_System_Backup_Save(size_t offset, const void* buffer, size_t length, void* reserved)
{
    return -1;
}

int HAL_System_Backup_Restore(size_t offset, void* buffer, size_t max_length, size_t* length, void* reserved)
{
    return -1;
}

#endif
