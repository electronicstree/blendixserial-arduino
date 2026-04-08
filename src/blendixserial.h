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
 * @file blendixserial.h
 * @brief Bidirectional compact binary protocol for real-time 3D transform & text
 *        synchronisation between Blender and microcontrollers.
 *
 * Protocol characteristics:
 *   - Framed binary format with STX / ETX markers
 *   - 8-bit XOR checksum covering header + payload
 *   - Delta compression: only changed object properties are transmitted
 *   - Up to 9 float values per object (location / rotation / scale × 3 axes)
 *   - Optional short ASCII/UTF-8 text field in the same packet
 *   - Big-endian float representation
 *   - Designed for low-bandwidth, real-time applications
 *
 * Packet structure (big-endian fields):
 *
 *   [ STX (1) | msgType (1) | objCount (1) | payloadLen (2) | payload (N) | checksum (1) | ETX (1) ]
 *
 *   msgType:  1 = objects only
 *             2 = text only: text fills payload, no length prefix
 *             3 = objects + text: text preceded by 1-byte length prefix
 *
 *   Per-object payload layout:
 *   [ objID (1) | mask (2) | float_0 (4) | float_1 (4) | … ]
 *   Only floats whose corresponding mask bit is set are present.
 *
 *   Checksum: XOR of all bytes from msgType through end of payload (STX and ETX excluded).
 *
 * Threading note:
 *   This library targets single-threaded Arduino-framework environments
 *   (Uno, Mega, ESP8266, ESP32 default loop, etc.).  bodBuild() and bodParse()
 *   share one payload buffer; they must never execute concurrently.
 *   Do NOT call these functions from separate RTOS tasks without external locking.
 */


#pragma once
#include <Arduino.h>
#include "blendixserial_config.h"


//Compile-time limits (overridable via blendixserial_config.h)

#ifndef BLENDIXSERIAL_MAX_OBJECTS
#  define BLENDIXSERIAL_MAX_OBJECTS  3
#endif

#ifndef BLENDIXSERIAL_MAX_PAYLOAD
#  define BLENDIXSERIAL_MAX_PAYLOAD  128
#endif

#ifndef BLENDIXSERIAL_MAX_TEXT
#  define BLENDIXSERIAL_MAX_TEXT     24
#endif

/** Total worst-case output buffer size needed by bodBuild(). */
#define BLENDIXSERIAL_MAX_PACKET_SIZE  (BLENDIXSERIAL_MAX_PAYLOAD + 7)


// Protocol framing constants 
#define BLENDIXSERIAL_STX  0x02   ///< Start-of-frame marker (ASCII STX)
#define BLENDIXSERIAL_ETX  0x03   ///< End-of-frame marker   (ASCII ETX)


// Public enumerations

/**
 * @enum Property
 * @brief Transform property group.
 */
enum Property
{
    Location = 0,   ///< Position / translation   (maps to values[0..2])
    Rotation = 1,   ///< Rotation (XYZ extrinsic) (maps to values[3..5])
    Scale    = 2    ///< Non-uniform scale         (maps to values[6..8])
};

/**
 * @enum Axis
 * @brief Component within a Property.
 */
enum Axis
{
    X = 0,
    Y = 1,
    Z = 2
};



/**
 * @class blendixserial
 * @brief Stateful handler for the BlendixSerial protocol (send + receive).
 *
 * Thread safety: NONE.
 *   Designed exclusively for single-threaded Arduino loop() usage.
 *   See threading note in the file header.
 */
class blendixserial
{
public:

    /**
     * @brief Default constructor, brings the object into a clean initial state.
     *
     * Postconditions:
     *   - All object txMask / rxMask = 0, dirty = false
     *   - Text buffers cleared, all text flags false
     *   - RX parser in WAIT_STX state
     *
     * Complexity: O(1)
     */
    blendixserial();

    /**
     * @brief Set one transform component and mark it for transmission.
     *
     * @param obj    Object index [0 .. BLENDIXSERIAL_MAX_OBJECTS-1].
     *               Out-of-range values are silently ignored.
     * @param prop   Which property group (Location / Rotation / Scale).
     * @param axis   Which axis (X / Y / Z).
     * @param value  New value (IEEE 754 single-precision float).
     *
     * Marks the axis in txMask and sets the object dirty.
     * Any previous unsent value for this axis is overwritten.
     *
     * Complexity: O(1)
     */
    void setValue(uint8_t obj, Property prop, Axis axis, float value);

    /**
     * @brief Set all three Location components at once.
     * Equivalent to three setValue() calls for Location X, Y, Z.
     */
    void setLocation(uint8_t obj, float x, float y, float z);

    /// @copydoc setLocation()
    void setRotation(uint8_t obj, float x, float y, float z);

    /// @copydoc setLocation()
    void setScale(uint8_t obj, float x, float y, float z);

    /**
     * @brief Queue a short text message for transmission in the next packet.
     *
     * @param text  Null-terminated C-string (UTF-8 compatible).
     *              Strings longer than (BLENDIXSERIAL_MAX_TEXT - 1) are silently truncated.
     *
     * Copies the string into the internal TX text buffer and sets the dirty
     * flag. Any previously queued unsent text is overwritten.
     *
     * Complexity: O(n), n = strlen(text), bounded by MAX_TEXT
     */
    void setText(const char *text);


    // TX build 

    /**
     * @brief Build one complete protocol frame if there is new data to send.
     *
     * @param buffer  Output buffer. Must be at least BLENDIXSERIAL_MAX_PACKET_SIZE bytes.
     * @return        Number of bytes written (7 .. MAX_PACKET_SIZE), or 0 if nothing
     *                is pending (no dirty objects and no pending text).
     *
     * Confidence when return > 0:
     *   - buffer[0] == STX, buffer[return-1] == ETX
     *   - Checksum is correct
     *   - Payload does not exceed BLENDIXSERIAL_MAX_PAYLOAD
     *   - All dirty flags (objects + text) are cleared ONLY on success
     *
     * If the payload would overflow MAX_PAYLOAD, returns 0 and leaves all
     * dirty flags unchanged so the next call will retry with the full data.
     * No data is silently lost.
     *
     * Complexity: O(k), k = number of changed axes across all dirty objects
     */
    uint16_t bodBuild(uint8_t *buffer);


    // RX parser 

    /**
     * @brief Feed one incoming byte into the incremental frame parser.
     *
     * @param byte  A single byte read from Serial (or any stream).
     * @return      true  = a complete, checksum-valid packet was just parsed
     *                      and applied to internal state.
     *              false = still accumulating, or a framing / checksum error
     *                      occurred (parser auto-resets and waits for next STX).
     *
     * Typical usage:
     * @code
     *   while (Serial.available()) {
     *     if (protocol.bodParse(Serial.read())) {
     *       // new values ready
     *     }
     *   }
     * @endcode
     *
     * Robustness:
     *   - Resynchronises on any garbage / partial frame by waiting for next STX
     *   - Rejects payloads larger than BLENDIXSERIAL_MAX_PAYLOAD
     *   - Verifies checksum before writing any state
     *
     * Complexity: O(1) amortised per byte
     */
    bool bodParse(uint8_t byte);


    // RX query: read received values 

    /**
     * @brief Check whether a specific axis has been received at least once.
     *
     * @param obj   Object index [0 .. BLENDIXSERIAL_MAX_OBJECTS-1].
     *              Out-of-range returns false.
     * @return true  = value is valid (arrived in at least one received packet)
     *         false = never received, or obj out of range
     */
    bool axisAvailable(uint8_t obj, Property prop, Axis axis);

    /**
     * @brief Get the most recently received value for one axis.
     *
     * @param obj  Object index [0 .. BLENDIXSERIAL_MAX_OBJECTS-1].
     *             Out-of-range returns 0.0f.
     * @note  Call axisAvailable() first if you need to know whether a value
     *        has actually arrived.
     */
    float getValue(uint8_t obj, Property prop, Axis axis);

    /**
     * @brief Read the most recent received Location triplet.
     * @note  No availability check is performed. Uninitialised axes read as 0.0f.
     */
    void getLocation(uint8_t obj, float &x, float &y, float &z);

    /// @brief Read the most recent received Rotation triplet.
    void getRotation(uint8_t obj, float &x, float &y, float &z);

    /// @brief Read the most recent received Scale triplet.
    void getScale(uint8_t obj, float &x, float &y, float &z);


    //  RX text API (requires BLENDIXSERIAL_ENABLE_TEXT_RX)
    //
    // These methods are compiled in only when BLENDIXSERIAL_ENABLE_TEXT_RX is
    // defined (in blendixserial_config.h or before the #include in your sketch).
    //
    // Background: currently the Blender add-on does not send text to the MCU.
    // The TX text path (setText / bodBuild) is fully functional in both modes.
    // The RX text path is reserved for a future Blender update and is gated
    // behind this flag to save RAM on memory-constrained boards (e.g. Uno).
    //
    // To enable:  #define BLENDIXSERIAL_ENABLE_TEXT_RX   (in config or sketch)
    // Memory cost: +BLENDIXSERIAL_MAX_TEXT bytes (default: +24 bytes)

#ifdef BLENDIXSERIAL_ENABLE_TEXT_RX

    /**
     * @brief Returns true if a new text message has arrived since the last
     *        clearText() call.
     */
    bool textAvailable();

    /**
     * @brief Returns a pointer to the null-terminated received text buffer.
     *
     * The returned pointer is valid until the next received packet that
     * contains text, or until clearText() is called.
     *
     * This function does NOT consume / clear the available flag.
     * Call clearText() explicitly when you are done with the text.
     */
    const char *getText();

    /**
     * @brief Mark the received text as consumed.
     *
     * After this call, textAvailable() returns false until the next text
     * packet arrives. Does not modify the buffer contents.
     */
    void clearText();

#endif // BLENDIXSERIAL_ENABLE_TEXT_RX


private:

    // Internal object state 

    struct ObjectState
    {
        float    values[9];   ///< [0-2] location  [3-5] rotation  [6-8] scale
        uint16_t txMask;      ///< Axes marked dirty for TX (set by setValue)
        uint16_t rxMask;      ///< Axes received from remote (set by decodePacket)
        bool     dirty;       ///< true = has unsent changes, include in next bodBuild
    };

    ObjectState objects[BLENDIXSERIAL_MAX_OBJECTS];


    // Text state

    char txTextBuffer[BLENDIXSERIAL_MAX_TEXT];  ///< Outgoing text (setText → bodBuild)
    bool txTextDirty;                           ///< true = txTextBuffer has unsent text

#ifdef BLENDIXSERIAL_ENABLE_TEXT_RX
    char rxTextBuffer[BLENDIXSERIAL_MAX_TEXT];  ///< Incoming text (decodePacket → getText)
    bool rxTextReceived;                        ///< true = new text arrived, not yet consumed
#endif


    // RX parser state machine 

    enum RxState
    {
        WAIT_STX,       ///< Waiting for 0x02, resync point
        READ_HEADER,    ///< Collecting 4 header bytes (msgType, objCount, lenH, lenL)
        READ_PAYLOAD,   ///< Filling payload[] byte by byte up to payloadLen
        READ_CHECKSUM,  ///< Reading the single checksum byte
        WAIT_ETX        ///< Expecting 0x03, then verifying and applying
    };

    RxState  rxState;

    uint8_t  header[4];                       ///< [0]=msgType [1]=objCount [2]=lenH [3]=lenL
    uint8_t  payload[BLENDIXSERIAL_MAX_PAYLOAD]; ///< Shared TX/RX buffer -- see threading note

    uint16_t payloadLen;    ///< Declared payload length from received header
    uint16_t payloadIndex;  ///< Current write position in payload[] during RX
    uint8_t  headerIndex;   ///< Current write position in header[] during RX
    uint8_t  rxChecksum;    ///< Received checksum byte (stored in READ_CHECKSUM state)


    //  Private helpers 

    /** Reset RX parser to WAIT_STX. Called after success or any error. */
    void resetRX();

    /**
     * Build the variable-length payload into this->payload[].
     *
     * @param msgType   Output: 1, 2, or 3 (set by this function).
     * @param objCount  Output: number of objects serialised.
     * @return          Number of bytes written, or 0 if nothing to send or
     *                  payload would overflow MAX_PAYLOAD.
     *
     * IMPORTANT: Dirty flags are NOT cleared here. They are cleared by
     * bodBuild() only after the complete frame is successfully assembled.
     * This ensures no data is lost on overflow.
     */
    uint16_t buildPayload(uint8_t &msgType, uint8_t &objCount);

    /**
     * Apply a verified, checksum-correct packet to internal state.
     * Called only from bodParse() after checksum passes.
     */
    void decodePacket(uint8_t msgType, uint8_t objCount);
};
