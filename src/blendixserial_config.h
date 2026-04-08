/**
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTIBILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
*/


/*
  If you discover any bugs or have suggestions, please let me know!

  Author: Usman
  Maintainer: Usman https://github.com/ELECTRONICSTREE/BlendixSerial-Arduino
  Email: help@electronicstree.com
  Date: 8-April-2026
*/



/**
 * @file blendixserial_config.h
 * @brief Compile-time configuration for the BlendixSerial library.
 *
 * Adjust these values to match your project's actual usage before compiling.
 * Use the official BlendixSerial Configurator to calculate safe values:
 *   https://electronicstree.com/arduino-blendixserial-library-configurator/
 */

#ifndef BLENDIXSERIAL_CONFIG_H
#define BLENDIXSERIAL_CONFIG_H



#define BLENDIXSERIAL_MAX_OBJECTS  3
#define BLENDIXSERIAL_MAX_PAYLOAD  128
#define BLENDIXSERIAL_MAX_TEXT     24
// Optional: enable RX text (adds 8B, only if Blender sends text to Arduino)
// #define BLENDIXSERIAL_ENABLE_TEXT_RX



#endif // BLENDIXSERIAL_CONFIG_H
