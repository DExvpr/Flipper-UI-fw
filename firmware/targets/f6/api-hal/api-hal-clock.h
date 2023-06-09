#pragma once

/** Initialize clocks */
void api_hal_clock_init();

/** Switch to HSI clock */
void api_hal_clock_switch_to_hsi();

/** Switch to PLL clock */
void api_hal_clock_switch_to_pll();
