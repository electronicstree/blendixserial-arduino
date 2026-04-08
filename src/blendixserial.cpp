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
  Date: 9-April-2026
*/


/**
 * @file blendixserial.cpp
 * @brief Implementation of the BlendixSerial bidirectional protocol handler.
 *
 * See blendixserial.h for full public API documentation and the packet format.
 */


#include "blendixserial.h"


// Big-endian float helpers 
//
// We use a union to reinterpret float bits as bytes.  This is well-defined in
// C (union type-punning) and safe on all Arduino-supported MCUs (AVR, ARM,
// Xtensa) which use IEEE 754 single-precision floats.
//
// Big-endian byte order is used on the wire so both AVR (little-endian) and
// any future big-endian host decode the same way.

static void writeFloatBE(uint8_t *buf, uint16_t &pos, float value)
{
    union { float f; uint8_t b[4]; } u;
    u.f = value;
    buf[pos++] = u.b[3];   // MSB first
    buf[pos++] = u.b[2];
    buf[pos++] = u.b[1];
    buf[pos++] = u.b[0];   // LSB last
}

static float readFloatBE(const uint8_t *buf, uint16_t &pos)
{
    union { float f; uint8_t b[4]; } u;
    u.b[3] = buf[pos++];
    u.b[2] = buf[pos++];
    u.b[1] = buf[pos++];
    u.b[0] = buf[pos++];
    return u.f;
}


// ── Constructor & reset ───────────────────────────────────────────────────────

blendixserial::blendixserial()
{
    for (uint8_t i = 0; i < BLENDIXSERIAL_MAX_OBJECTS; i++)
    {
        objects[i].txMask = 0;
        objects[i].rxMask = 0;
        objects[i].dirty  = false;

        // Zero the value array so getValue() on an uninitialised axis returns
        // 0.0f rather than uninitialised memory.
        for (uint8_t j = 0; j < 9; j++)
            objects[i].values[j] = 0.0f;
    }

    txTextBuffer[0] = '\0';
    txTextDirty     = false;

#ifdef BLENDIXSERIAL_ENABLE_TEXT_RX
    rxTextBuffer[0] = '\0';
    rxTextReceived  = false;
#endif

    resetRX();
}

void blendixserial::resetRX()
{
    rxState      = WAIT_STX;
    headerIndex  = 0;
    payloadIndex = 0;
    payloadLen   = 0;
    rxChecksum   = 0;
}


// TX path — marking changed values

void blendixserial::setValue(uint8_t obj, Property prop, Axis axis, float value)
{
    //  bounds check (read path was missing this, TX path already had it)
    if (obj >= BLENDIXSERIAL_MAX_OBJECTS) return;

    uint8_t index = (uint8_t)prop * 3u + (uint8_t)axis;  // 0..8

    objects[obj].values[index]  = value;
    objects[obj].txMask        |= (uint16_t)(1u << index);  //  write txMask, not bitmask
    objects[obj].dirty          = true;
}

void blendixserial::setLocation(uint8_t obj, float x, float y, float z)
{
    setValue(obj, Location, X, x);
    setValue(obj, Location, Y, y);
    setValue(obj, Location, Z, z);
}

void blendixserial::setRotation(uint8_t obj, float x, float y, float z)
{
    setValue(obj, Rotation, X, x);
    setValue(obj, Rotation, Y, y);
    setValue(obj, Rotation, Z, z);
}

void blendixserial::setScale(uint8_t obj, float x, float y, float z)
{
    setValue(obj, Scale, X, x);
    setValue(obj, Scale, Y, y);
    setValue(obj, Scale, Z, z);
}

void blendixserial::setText(const char *text)
{
    strncpy(txTextBuffer, text, BLENDIXSERIAL_MAX_TEXT - 1);
    txTextBuffer[BLENDIXSERIAL_MAX_TEXT - 1] = '\0';  // guarantee null terminator
    txTextDirty = true;
}


//  RX query: reading received values 

bool blendixserial::axisAvailable(uint8_t obj, Property prop, Axis axis)
{
    //  bounds check on read path
    if (obj >= BLENDIXSERIAL_MAX_OBJECTS) return false;

    uint8_t index = (uint8_t)prop * 3u + (uint8_t)axis;
    return (objects[obj].rxMask & (uint16_t)(1u << index)) != 0;  //  read rxMask
}

float blendixserial::getValue(uint8_t obj, Property prop, Axis axis)
{
    //  bounds check on read path
    if (obj >= BLENDIXSERIAL_MAX_OBJECTS) return 0.0f;

    uint8_t index = (uint8_t)prop * 3u + (uint8_t)axis;
    return objects[obj].values[index];
}

void blendixserial::getLocation(uint8_t obj, float &x, float &y, float &z)
{
    //  bounds check on triplet getters
    if (obj >= BLENDIXSERIAL_MAX_OBJECTS) { x = y = z = 0.0f; return; }

    x = getValue(obj, Location, X);
    y = getValue(obj, Location, Y);
    z = getValue(obj, Location, Z);
}

void blendixserial::getRotation(uint8_t obj, float &x, float &y, float &z)
{
    if (obj >= BLENDIXSERIAL_MAX_OBJECTS) { x = y = z = 0.0f; return; }

    x = getValue(obj, Rotation, X);
    y = getValue(obj, Rotation, Y);
    z = getValue(obj, Rotation, Z);
}

void blendixserial::getScale(uint8_t obj, float &x, float &y, float &z)
{
    if (obj >= BLENDIXSERIAL_MAX_OBJECTS) { x = y = z = 0.0f; return; }

    x = getValue(obj, Scale, X);
    y = getValue(obj, Scale, Y);
    z = getValue(obj, Scale, Z);
}


// RX text API (only compiled when TEXT_RX is enabled)

#ifdef BLENDIXSERIAL_ENABLE_TEXT_RX

bool blendixserial::textAvailable()
{
    return rxTextReceived;
}

const char *blendixserial::getText()
{
    //  getText() no longer clears the flag — caller must call clearText().
    // This prevents the double-consume bug where a second call to getText()
    // returned a still-valid pointer but textAvailable() had silently gone false.
    return rxTextBuffer;
}

void blendixserial::clearText()
{
    rxTextReceived = false;
}

#endif // BLENDIXSERIAL_ENABLE_TEXT_RX


// Payload builder

/**
 * Serialises all dirty objects and pending text into this->payload[].
 *
 * Dirty flags are intentionally NOT cleared here.
 * bodBuild() clears them only after the full frame is successfully assembled.
 * If this function returns 0 (overflow or nothing to send), the caller retains
 * all pending data and will retry on the next bodBuild() call.
 *
 * msgType is determined first (before any serialisation) and the
 * function exits immediately if there is nothing to send.
 */
uint16_t blendixserial::buildPayload(uint8_t &msgType, uint8_t &objCount)
{
    // Determine what we have to send 

    // Count dirty objects (we need this before deciding msgType)
    uint8_t dirtyCount = 0;
    for (uint8_t i = 0; i < BLENDIXSERIAL_MAX_OBJECTS; i++)
        if (objects[i].dirty) dirtyCount++;

    // decide msgType up front; exit early with 0 if nothing at all
    bool hasObjects = (dirtyCount > 0);
    bool hasText    = txTextDirty;

    if (!hasObjects && !hasText) return 0;

    if (hasObjects && !hasText) msgType = 1;
    else if (!hasObjects &&  hasText) msgType = 2;
    else msgType = 3;  // both

    // Serialise objects 

    uint16_t pos  = 0;
    objCount      = 0;

    for (uint8_t obj = 0; obj < BLENDIXSERIAL_MAX_OBJECTS; obj++)
    {
        if (!objects[obj].dirty) continue;

        uint16_t mask = objects[obj].txMask;  //  use txMask

        // Minimum space needed: 1 byte objID + 2 bytes mask = 3 bytes
        // Plus 4 bytes per set axis bit.
        // We check tightly here to avoid any overflow.
        uint8_t  setBits   = 0;
        uint16_t tmp       = mask;
        while (tmp) { setBits += (tmp & 1u); tmp >>= 1; }  // popcount

        uint16_t neededBytes = 3u + (uint16_t)setBits * 4u;

        if (pos + neededBytes > BLENDIXSERIAL_MAX_PAYLOAD)
        {
            // overflow — abort without clearing any dirty flags.
            // Return 0 so bodBuild() sends nothing and all data is preserved.
            return 0;
        }

        payload[pos++] = obj;                        // object ID
        payload[pos++] = (uint8_t)(mask >> 8);       // mask high byte
        payload[pos++] = (uint8_t)(mask & 0xFF);     // mask low byte

        for (uint8_t bit = 0; bit < 9; bit++)
        {
            if (mask & (uint16_t)(1u << bit))
                writeFloatBE(payload, pos, objects[obj].values[bit]);
        }

        objCount++;
    }

    //  Serialise text 

    if (hasText)
    {
        size_t   textLen  = strlen(txTextBuffer);
        uint16_t needed   = (msgType == 3)
                            ? (uint16_t)(1u + textLen)   // type 3: 1-byte length prefix
                            : (uint16_t)textLen;         // type 2: no prefix

        if (pos + needed > BLENDIXSERIAL_MAX_PAYLOAD)
            return 0;  // overflow — abort, preserve dirty flags

        if (msgType == 3)
            payload[pos++] = (uint8_t)textLen;  // explicit length byte for type 3

        memcpy(payload + pos, txTextBuffer, textLen);
        pos += (uint16_t)textLen;
    }

    return pos;  // total bytes written into payload[]
}


//  bodBuild — assemble and return a complete frame 

uint16_t blendixserial::bodBuild(uint8_t *buffer)
{
    uint8_t  msgType  = 0;
    uint8_t  objCount = 0;

    // Build payload into this->payload[].
    // NOTE: payload[] is the shared TX/RX member buffer.  This is safe because
    // Arduino sketch code is single-threaded — bodBuild() and bodParse() can
    // never execute concurrently in a normal loop() context.
    uint16_t pLen = buildPayload(msgType, objCount);

    if (pLen == 0) return 0;  // nothing to send (or overflow — dirty flags intact)

    // Assemble frame 
    //
    // [ STX (1) | msgType (1) | objCount (1) | lenH (1) | lenL (1) | payload (pLen) | CS (1) | ETX (1) ]

    buffer[0] = BLENDIXSERIAL_STX;
    buffer[1] = msgType;
    buffer[2] = objCount;
    buffer[3] = (uint8_t)(pLen >> 8);
    buffer[4] = (uint8_t)(pLen & 0xFF);

    memcpy(buffer + 5, payload, pLen);

    // XOR checksum covers bytes 1..4 (header fields) + all payload bytes.
    // STX (buffer[0]) and ETX are excluded from the checksum.
    uint8_t cs = 0;
    for (uint8_t i = 1; i <= 4; i++) cs ^= buffer[i];
    for (uint16_t i = 0; i < pLen;  i++) cs ^= payload[i];

    buffer[5 + pLen] = cs;
    buffer[6 + pLen] = BLENDIXSERIAL_ETX;

    // Clear dirty flags ONLY here, after the frame is fully assembled.
    // If buildPayload() had returned 0 we would never reach this point,
    // so dirty flags are guaranteed to be preserved on any failure path.
    for (uint8_t obj = 0; obj < BLENDIXSERIAL_MAX_OBJECTS; obj++)
    {
        if (objects[obj].dirty)
        {
            objects[obj].dirty  = false;
            objects[obj].txMask = 0;   //  clear txMask (not rxMask)
        }
    }
    txTextDirty = false;

    return (uint16_t)(pLen + 7u);  // total frame size
}


//  bodParse: incremental byte-by-byte frame parser 

bool blendixserial::bodParse(uint8_t byte)
{
    switch (rxState)
    {
        //  Waiting for frame start 
        case WAIT_STX:
            if (byte == BLENDIXSERIAL_STX)
            {
                resetRX();          // clear indices
                rxState = READ_HEADER;
            }
            // Any other byte is silently discarded (resync behaviour)
            break;

        //    Collecting 4-byte header 
        //    header[0] = msgType
        //    header[1] = objCount
        //    header[2] = payloadLen high byte
        //    header[3] = payloadLen low byte
        case READ_HEADER:
            header[headerIndex++] = byte;

            if (headerIndex == 4)
            {
                payloadLen = ((uint16_t)header[2] << 8) | header[3];

                if (payloadLen > BLENDIXSERIAL_MAX_PAYLOAD)
                {
                    // Declared length is too large — discard and resync
                    resetRX();
                    break;
                }

                // Special case: zero-length payload is valid for a text-only
                // packet with an empty string, but meaningless otherwise.
                // Jump straight to checksum if nothing to collect.
                rxState = (payloadLen == 0) ? READ_CHECKSUM : READ_PAYLOAD;
            }
            break;

        //  Filling payload buffer 
        case READ_PAYLOAD:
            payload[payloadIndex++] = byte;

            if (payloadIndex >= payloadLen)
                rxState = READ_CHECKSUM;
            break;

        // Storing received checksum 
        case READ_CHECKSUM:
            rxChecksum = byte;   // renamed from 'checksum' to avoid shadowing
            rxState    = WAIT_ETX;
            break;

        // Verifying frame end and checksum 
        case WAIT_ETX:
            if (byte == BLENDIXSERIAL_ETX)
            {
                // Compute expected checksum over header[0..3] + payload[0..payloadLen-1]
                uint8_t calc = 0;
                for (uint8_t  i = 0; i < 4;          i++) calc ^= header[i];
                for (uint16_t i = 0; i < payloadLen;  i++) calc ^= payload[i];

                if (calc == rxChecksum)
                {
                    decodePacket(header[0], header[1]);
                    resetRX();
                    return true;   // complete valid packet processed
                }
                // Checksum mismatch — fall through to resetRX()
            }
            // Bad ETX byte or checksum failure — discard frame and resync
            resetRX();
            break;
    }

    return false;
}


// ─decodePacket — apply verified packet to internal state 

void blendixserial::decodePacket(uint8_t msgType, uint8_t objCount)
{
    uint16_t pos = 0;

    //  Decode objects (msgType 1 or 3) 
    if (msgType == 1 || msgType == 3)
    {
        for (uint8_t i = 0; i < objCount; i++)
        {
            if (pos + 3u > payloadLen) break;  // guard against malformed packet

            uint8_t obj = payload[pos++];

            // Silently skip objects with an out-of-range ID to avoid memory corruption.
            // FIX 1: bounds check inside decoder as well
            uint16_t mask = ((uint16_t)payload[pos] << 8) | payload[pos + 1];
            pos += 2;

            if (obj < BLENDIXSERIAL_MAX_OBJECTS)
            {
                // write rxMask — does not touch txMask or dirty flag
                objects[obj].rxMask = mask;

                for (uint8_t bit = 0; bit < 9; bit++)
                {
                    if (mask & (uint16_t)(1u << bit))
                    {
                        if (pos + 4u > payloadLen) break;  // truncated float
                        objects[obj].values[bit] = readFloatBE(payload, pos);
                    }
                }
            }
            else
            {
                // Unknown object ID — skip over its float data to stay in sync
                uint8_t setBits = 0;
                uint16_t tmp = mask;
                while (tmp) { setBits += (tmp & 1u); tmp >>= 1; }
                pos += (uint16_t)setBits * 4u;
            }
        }
    }

    // Decode text (msgType 2 or 3) 
    //
    // Text encoding on the wire:
    //   msgType 2 (text only):   text bytes fill the rest of the payload.
    //                            Length = payloadLen - pos.  No length prefix.
    //   msgType 3 (objects+text): 1-byte explicit length prefix precedes text.
    //
    // This asymmetry is intentional: for type 2, payloadLen already tells us
    // where the packet ends so a prefix would waste one byte on every text-only
    // packet (important on low-bandwidth links).
    if (msgType == 2 || msgType == 3)
    {
#ifdef BLENDIXSERIAL_ENABLE_TEXT_RX
        uint16_t textLen;
        uint16_t textStart;

        if (msgType == 3)
        {
            if (pos >= payloadLen) return;          // no room for length byte
            textLen   = payload[pos++];
            textStart = pos;
        }
        else  // msgType == 2
        {
            textLen   = payloadLen - pos;
            textStart = pos;
        }

        // Clamp to our buffer capacity
        if (textLen >= BLENDIXSERIAL_MAX_TEXT)
            textLen = BLENDIXSERIAL_MAX_TEXT - 1;

        memcpy(rxTextBuffer, payload + textStart, textLen);
        rxTextBuffer[textLen] = '\0';
        rxTextReceived = true;

#endif // BLENDIXSERIAL_ENABLE_TEXT_RX
        // When TEXT_RX is disabled, incoming text is silently ignored.
        // The packet is still accepted and objects within it are decoded normally.
        (void)pos;  // suppress unused-variable warning when TEXT_RX is disabled
    }
}
