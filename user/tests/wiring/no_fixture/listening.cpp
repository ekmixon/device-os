/*
 * Copyright (c) 2020 Particle Industries, Inc.  All rights reserved.
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

#include "application.h"
#include "unit-test/unit-test.h"

// TODO: Should we add a test case for the default case? For users that don't touch the flag

// Dot not enter listening mode based on the flag
test(LISTENING_00_DISABLE_LISTENING_MODE) {
    System.enableFeature(FEATURE_DISABLE_LISTENING_MODE);
    Network.listen();
    HAL_Delay_Milliseconds(1000); // Time for system thread to enter listening mode, if any
    assertFalse(Network.listening());
}

// Enter listening mode based on the flag
test(LISTENING_01_ENABLE_LISTENING_MODE) {
    System.disableFeature(FEATURE_DISABLE_LISTENING_MODE);
    Network.listen();
    HAL_Delay_Milliseconds(1000); // Time for system thread to enter listening mode
    assertTrue(Network.listening());
    Network.listen(false);
    HAL_Delay_Milliseconds(1000); // Time for system thread to exit listening mode
    assertFalse(Network.listening());
}

// If the flag is enabled while device is in listening mode,
// device-os should exit listening mode
test(LISTENING_02_DISABLE_FLAG_WHILE_IN_LISTENING_MODE) {
    Network.listen();
    HAL_Delay_Milliseconds(1000); // Time for system thread to enter listening mode
    assertTrue(Network.listening());
    System.enableFeature(FEATURE_DISABLE_LISTENING_MODE);
    HAL_Delay_Milliseconds(1500); // Time for system thread to process the flag
    assertFalse(Network.listening());
}
