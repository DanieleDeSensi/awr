/**
 * Copyright (C) 2019 - ETH Zurich
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 **/
#include "gni_pub.h"

/**
 * @brief awr_change_routing Sets a specific routing strategy.
 * @param routing The routing to be set. Values are specified in gni_pub.h
 */
void awr_change_routing(uint16_t routing);

/**
 * @brief awr_enable Enables the automatic mode. Enabled by default.
 */
void awr_enable();

/**
 * @brief awr_disable Disables the automatic mode.
 */
void awr_disable();
