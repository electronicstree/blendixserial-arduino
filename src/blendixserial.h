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
 * @file blendixserial.h
 * @brief Bidirectional compact binary protocol for real-time 3D transform & text synchronization
 *        between Blender and microcontrollers.
 *
 * Protocol characteristics:
 * - Framed binary format with STX / ETX markers
 * - 8-bit XOR checksum covering header + payload
 * - Delta compression: only changed object properties are transmitted
 * - Up to 9 float values per object (location/rotation/scale × 3 axes)
 * - Optional short ASCII/UTF-8 text field in the same packet
 * - Big-endian float representation
 * - Designed for low-bandwidth, real-time applications 
 *
 * Typical packet structure (big-endian fields):
 *   STX [0x02]
 *   msgType       (1 byte)    1=objects only, 2=text only, 3=objects+text
 *   objCount      (1 byte)
 *   payloadLen    (2 bytes)
 *   payload       (0–MAX_PAYLOAD bytes)
 *   checksum      (1 byte)    XOR of bytes from msgType to end of payload
 *   ETX           [0x03]
 */


#pragma once
#include <Arduino.h>
#include "blendixserial_config.h"

//  Compile-time limits (overridable via config)
#ifndef BLENDIXSERIAL_MAX_OBJECTS
#define BLENDIXSERIAL_MAX_OBJECTS 3
#endif

#ifndef BLENDIXSERIAL_MAX_PAYLOAD
#define BLENDIXSERIAL_MAX_PAYLOAD 128
#endif

#ifndef BLENDIXSERIAL_MAX_TEXT
#define BLENDIXSERIAL_MAX_TEXT 24
#endif

#define BLENDIXSERIAL_MAX_PACKET_SIZE (BLENDIXSERIAL_MAX_PAYLOAD + 7)


/**
 * @name Protocol framing constants
 * @{
 */
#define BLENDIXSERIAL_STX 0x02 ///< Start-of-frame marker (ASCII STX)
#define BLENDIXSERIAL_ETX 0x03 ///< End-of-frame marker   (ASCII ETX)
/** @} */



/**
 * @enum Property
 * @brief Transform property groups supported by the protocol
 */
enum Property
{
    Location = 0,   ///< Position / translation             (bits 0–2)
    Rotation = 1,   ///< Rotation (XYZ extrinsic)           (bits 3–5)
    Scale    = 2    ///< Non-uniform scale                  (bits 6–8)
};



/**
 * @enum Axis
 * @brief Individual component within a Property
 */
enum Axis
{
    X = 0,
    Y = 1,
    Z = 2
};



/**
 * @class blendixserial
 * @brief Core stateful handler for the BlendixSerial protocol (send + receive)
 *
 * Thread safety: None |  designed for single-threaded Arduino loop() usage.
 *                 Not safe to call member functions from ISR or multiple tasks
 *                 without external synchronization.
 */
class blendixserial
{
public:

    /**
     * @brief Default constructor – initializes clean state
     *
     * Postconditions:
     *  - All object states are cleared (bitmask=0, dirty=false)
     *  - Text buffers/flags are cleared
     *  - Receive FSM is in WAIT_STX state
     *
     * Complexity: O(1) — fixed size arrays
     */
    blendixserial();


    /**
     * @brief Set value of one specific transform component
     * @param obj   Object index [0 .. BLENDIXSERIAL_MAX_OBJECTS-1]
     * @param prop  Which transform property (Location / Rotation / Scale)
     * @param axis  Which axis (X/Y/Z)
     * @param value New value (IEEE 754 single precision float)
     *
     * Behavior:
     *  - Marks the corresponding bit in the object's change mask
     *  - Sets the dirty flag → object will be included in next bodBuild()
     *  - Overwrites previous value (even if it was already set)
     *
     * Preconditions:
     *  - obj < BLENDIXSERIAL_MAX_OBJECTS  (otherwise call is ignored)
     *
     * Complexity: O(1)
    */
    void setValue(uint8_t obj, Property prop, Axis axis, float value);


    /**
     * @brief Convenience function – set all three location components at once
     * @param obj   Object index [0 .. BLENDIXSERIAL_MAX_OBJECTS-1]
     * @param x,y,z New position coordinates
     *
     * Equivalent to three consecutive setValue() calls for Location axis X/Y/Z.
     * Marks object dirty if any component changes (actually always, because
     * previous values are not compared).
     */
    void setLocation(uint8_t obj, float x, float y, float z);


    /// @copydoc setLocation()
    void setRotation(uint8_t obj, float x, float y, float z);


    /// @copydoc setLocation()
    void setScale(uint8_t obj, float x, float y, float z);


    /**
     * @brief Queue a short text message to be sent with the next packet
     * @param text  Null-terminated C-string (UTF-8 compatible)
     *
     * Constraints:
     *  - Maximum storable length = BLENDIXSERIAL_MAX_TEXT - 1
     *  - Longer strings are silently truncated
     *  - Empty string ("") is allowed and will be sent
     *
     * Behavior:
     *  - Copies string into internal buffer + adds null terminator
     *  - Sets textDirty flag → text will be included in next bodBuild()
     *  - Previous text (if unsent) is overwritten
     *
     * Complexity: O(n) where n = strlen(text), bounded by MAX_TEXT
     */
    void setText(const char *text);


    /**
     * @brief Build one complete protocol frame if there is new data to send
     * @param buffer  Output buffer — **must** be at least BLENDIXSERIAL_MAX_PACKET_SIZE bytes
     * @return        Number of bytes written to buffer [7 .. MAX_PACKET_SIZE]
     *                or 0 if nothing to send (no dirty objects and no pending text)
     *
     * Confidence:
     *  - When return > 0 → buffer[0] = STX, buffer[return-1] = ETX
     *  - Checksum is correct (verified on receiving side)
     *  - All dirty flags and textDirty flag are cleared after successful build
     *  - Payload never exceeds BLENDIXSERIAL_MAX_PAYLOAD
     *
     * Edge cases handled:
     *  - No data → returns 0
     *  - Text only → msgType=2
     *  - Objects only → msgType=1
     *  - Both → msgType=3
     *  - Payload would overflow → silently drops oldest objects until it fits
     *
     * Complexity: O(k) where k = number of changed axes across all objects
     */
    uint16_t bodBuild(uint8_t *buffer);

    
    /**
     * @brief Feed one incoming byte into the incremental parser
     * @param byte  Single byte received from UART / stream
     * @return      true  = a complete, valid packet was just parsed & applied
     *              false = parsing still in progress or error occurred (state reset)
     *
     * Typical usage:
     *   while (Serial.available()) {
     *     if (protocol.bodParse(Serial.read())) {
     *       // new data available via getLocation(), textAvailable(), etc.
     *     }
     *   }
     *
     * Robustness:
     *  - Recovers from garbage / partial frames by waiting for next STX
     *  - Validates payload length against MAX_PAYLOAD
     *  - Verifies checksum before applying any changes
     *
     * Complexity: O(1) amortized per byte
     */
    bool bodParse(uint8_t byte);


    /**
     * @brief Check whether a specific component has been received at least once
     * @return true  = value is valid (has been set by incoming packet)
     *         false = never received → do not use value
     */
    bool axisAvailable(uint8_t obj, Property prop, Axis axis);


    /**
     * @brief Get most recently received value for one component
     */
    float getValue(uint8_t obj, Property prop, Axis axis);


    /// @brief Get most recent location triplet (only reads — no availability check)
    void getLocation(uint8_t obj, float &x, float &y, float &z);


    /// @brief Get most recent rotation triplet
    void getRotation(uint8_t obj, float &x, float &y, float &z);


    /// @brief Get most recent scale triplet
    void getScale(uint8_t obj, float &x, float &y, float &z);


    /**
     * Receiving Text API (Future Use) 
     * These methods are reserved for future updates when Blender gain text sending capabilities.
     */
    bool textAvailable();
    const char* getText();


private:
    //  Internal types & state
    struct ObjectState
    {
        float values[9];    ///< [0–2] location  [3–5] rotation  [6–8] scale
        uint16_t bitmask;   ///< bit i set = values[i] is valid
        bool dirty;         ///< needs to be sent
    };

    ObjectState objects[BLENDIXSERIAL_MAX_OBJECTS];

    // text mirror 

    char textBuffer[BLENDIXSERIAL_MAX_TEXT];
    bool textDirty;
    bool textReceived;

    // decoder 

    enum RXSTATE
    {
        WAIT_STX,
        READ_HEADER,
        READ_PAYLOAD,
        READ_CHECKSUM,
        WAIT_ETX
    };

    // Decoder FSM
    RXSTATE rxState;

    uint8_t header[4];
    uint8_t payload[BLENDIXSERIAL_MAX_PAYLOAD];

    uint16_t payloadLen;
    uint16_t payloadIndex;
    uint8_t headerIndex;

    uint8_t checksum;

    //  Private implementation helpers

    void resetRX();

    uint16_t buildPayload(uint8_t *payload, uint8_t &msgType, uint8_t &objCount);

    void decodePacket(uint8_t msgType, uint8_t objCount);

};