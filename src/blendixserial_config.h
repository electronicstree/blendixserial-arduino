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
  Date: 9-Mar-2026
*/



/**
 * @file blendixserial_config.h
 * @brief Configuration file for the BlendixSerial library
 *
 * This file contains compile-time configuration parameters that control
 * memory usage and feature limits of the BlendixSerial protocol implementation.
 * Modify these values according to your project's needs and available RAM.
 *
 * Recommended procedure:
 * 1. Use the official BlendixSerial Configurator calculator
 * 2. Set values that match your actual usage pattern
 * 3. Recompile and upload the sketch
 * 
 * BlendixSerial Configurator: https://electronicstree.com/arduino-blendixserial-library-configurator/
 */

#ifndef BLENDIXSERIAL_CONFIG_H
#define BLENDIXSERIAL_CONFIG_H

/**
 * @def BLENDIXSERIAL_MAX_OBJECTS
 * @brief Maximum number of simultaneously tracked 3D objects
 *
 * Each object can have location, rotation and scale (up to 9 float values).
 * Every object consumes ~36 bytes depending on alignment.
 */
#define BLENDIXSERIAL_MAX_OBJECTS  3

/**
 * @def BLENDIXSERIAL_MAX_PAYLOAD
 * @brief Maximum allowed payload size in bytes (excluding protocol overhead)
 *
 * Payload contains object data + optional text.
 * 128 bytes allows ~3 fully updated objects or shorter text + several objects.
 */
#define BLENDIXSERIAL_MAX_PAYLOAD  128

/**
 * @def BLENDIXSERIAL_MAX_TEXT
 * @brief Maximum length of text message (excluding null terminator)
 *
 * Text is sent/received as raw bytes (no length prefix in type=2 messages).
 * Buffer always contains null terminator.
 */
#define BLENDIXSERIAL_MAX_TEXT     24

#endif