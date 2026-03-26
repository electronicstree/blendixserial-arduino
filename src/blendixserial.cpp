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
 * @file blendixserial.cpp
 * @brief Implementation of the BlendixSerial bidirectional protocol handler
 *
 * See blendixserial.h for public API documentation.
 */


#include "blendixserial.h"

// ──────────────────────────────────────────────────────────────
//  Big-endian float <-> byte array conversion helpers
// ──────────────────────────────────────────────────────────────

/**
 * Serializes a 32-bit IEEE 754 float into 4 bytes in **big-endian** order.
 * We use a union to reinterpret the float bits as bytes, this is fast
 * and common in embedded code, but assumes the platform uses IEEE 754
 * (which is true for virtually all modern MCUs and Arduino environments).
 */
static void writeFloatBE(uint8_t *buffer, uint16_t &pos, float value)
{
    union
    {
        float f;    // 32-bit float
        uint8_t b[4];   // same memory viewed as 4 bytes
    } u;

    u.f = value;    // put float into union

    // Write bytes from highest to lowest address → big-endian
    buffer[pos++] = u.b[3]; // sign bit + exponent MSB
    buffer[pos++] = u.b[2];
    buffer[pos++] = u.b[1];
    buffer[pos++] = u.b[0]; // mantissa LSB
}


/**
 * Reconstructs a float from 4 big-endian bytes.
 * Inverse operation of writeFloatBE().
 */
static float readFloatBE(uint8_t *buffer, uint16_t &pos)
{
    union
    {
        float f;
        uint8_t b[4];
    } u;
    // Read bytes in reverse order compared to write (big-endian → host)
    u.b[3] = buffer[pos++];
    u.b[2] = buffer[pos++];
    u.b[1] = buffer[pos++];
    u.b[0] = buffer[pos++];

    return u.f;
}



// ──────────────────────────────────────────────────────────────
//  Construction & basic state management
// ──────────────────────────────────────────────────────────────

/**
 * Constructor – called when you create:  blendixserial protocol;
 * Goal: bring the object into a well-defined, empty state.
 */
blendixserial::blendixserial()
{   
    // Clear all object tracking state
    for(int i=0;i<BLENDIXSERIAL_MAX_OBJECTS;i++) 
    {
        objects[i].bitmask=0;   // no axes have valid/changed values
        objects[i].dirty=false; // nothing needs to be sent yet
    }

    // Text state starts clean
    textDirty=false;    // no pending text to send
    textReceived=false; // no text has arrived yet

    resetRX();  // Put receive parser into initial waiting state
}


/**
 * Resets the **receive** state machine only.
 * Called:
 *   - after successfully processing a packet
 *   - after detecting any parsing error
 *   - when starting fresh
 *
 * Important: does **not** clear the payload[] buffer contents —
 * we only care about indices and state, not old data (overwritten anyway).
 */
void blendixserial::resetRX()
{
    rxState = WAIT_STX;   // next byte must be STX to start new frame
    payloadIndex = 0;     // start filling payload from beginning
    headerIndex  = 0;     // start filling 4-byte header from beginning
}


// ──────────────────────────────────────────────────────────────
//  Transmit path – marking changed values
// ──────────────────────────────────────────────────────────────

/**
 * Core function for updating one single float value (x/y/z of loc/rot/scale).
 * 
 * Every time a value is set → we:
 *   1. store the new number
 *   2. mark that exact component as changed (using bit in bitmask)
 *   3. mark the whole object as dirty → it will be sent next time
 */
void blendixserial::setValue(uint8_t obj,Property prop,Axis axis,float value)
{
    if(obj>=BLENDIXSERIAL_MAX_OBJECTS) return; // silent ignore invalid index

    uint8_t index = prop*3 + axis;  // Calculate flat index:  Location→0-2, Rotation→3-5, Scale→6-8

    objects[obj].values[index]=value;   // store new value
    objects[obj].bitmask |= (1<<index); // mark this axis changed
    objects[obj].dirty=true;    // object needs sending
}

// Convenience wrappers – call setValue three times
void blendixserial::setLocation(uint8_t obj,float x,float y,float z)
{
    setValue(obj,Location,X,x);
    setValue(obj,Location,Y,y);
    setValue(obj,Location,Z,z);
}

void blendixserial::setRotation(uint8_t obj,float x,float y,float z)
{
    setValue(obj,Rotation,X,x);
    setValue(obj,Rotation,Y,y);
    setValue(obj,Rotation,Z,z);
}

void blendixserial::setScale(uint8_t obj,float x,float y,float z)
{
    setValue(obj,Scale,X,x);
    setValue(obj,Scale,Y,y);
    setValue(obj,Scale,Z,z);
}


/**
 * Queue text message to be sent in the next packet.
 * Text has lower priority than transform data when both are pending.
 */
void blendixserial::setText(const char *text)
{
    strncpy(textBuffer,text,BLENDIXSERIAL_MAX_TEXT-1); // Copy string, but never overflow internal buffer
    textBuffer[BLENDIXSERIAL_MAX_TEXT-1]=0; // Force null terminator even if input was exactly max length

    textDirty=true; // mark that we have text waiting to be sent
}


// ──────────────────────────────────────────────────────────────
// Value Reading API (received / mirrored state)
// ──────────────────────────────────────────────────────────────

bool blendixserial::axisAvailable(uint8_t obj,Property prop,Axis axis)
{
    uint8_t index=prop*3+axis;
    return objects[obj].bitmask & (1<<index);
}

float blendixserial::getValue(uint8_t obj,Property prop,Axis axis)
{
    uint8_t index=prop*3+axis;
    return objects[obj].values[index];
}

// Group getters – no availability check (caller should have checked)
void blendixserial::getLocation(uint8_t obj,float &x,float &y,float &z)
{
    x=getValue(obj,Location,X);
    y=getValue(obj,Location,Y);
    z=getValue(obj,Location,Z);
}

void blendixserial::getRotation(uint8_t obj,float &x,float &y,float &z)
{
    x=getValue(obj,Rotation,X);
    y=getValue(obj,Rotation,Y);
    z=getValue(obj,Rotation,Z);
}

void blendixserial::getScale(uint8_t obj,float &x,float &y,float &z)
{
    x=getValue(obj,Scale,X);
    y=getValue(obj,Scale,Y);
    z=getValue(obj,Scale,Z);
}


// ──────────────────────────────────────────────────────────────
// Text API (received) | Future Use
// ──────────────────────────────────────────────────────────────

bool blendixserial::textAvailable()
{
    return textReceived;
}

const char* blendixserial::getText()
{
    textReceived=false;
    return textBuffer;
}



// ──────────────────────────────────────────────────────────────
// Payload Builder – creates compact delta payload
// ──────────────────────────────────────────────────────────────

/**
 * Builds the variable-length payload part (everything after header).
 * Only includes objects that have .dirty == true.
 * Returns number of bytes written or 0 on failure/overflow.
 */

uint16_t blendixserial::buildPayload(uint8_t *payload,uint8_t &msgType,uint8_t &objCount)
{
    uint16_t pos=0;
    objCount=0;
    msgType=0;

    // Iterate through all possible objects
    for(uint8_t obj=0; obj<BLENDIXSERIAL_MAX_OBJECTS; obj++)
    {
        if(!objects[obj].dirty) continue; // skip unchanged objects

        // Minimal space check before writing object header
        if(pos+3 > BLENDIXSERIAL_MAX_PAYLOAD) return 0;

        payload[pos++] = obj;   // object ID (0–N)

        uint16_t mask = objects[obj].bitmask;

        // 16-bit change mask (big-endian)
        payload[pos++] = (mask >> 8) & 0xFF;    // high byte
        payload[pos++] = mask & 0xFF;   // low byte

        for(uint8_t i=0;i<9;i++)    // Write only the floats that changed (according to mask)
        {
            if(mask & (1<<i))
            {
                if(pos+4 > BLENDIXSERIAL_MAX_PAYLOAD) return 0;

                writeFloatBE(payload,pos,objects[obj].values[i]);
            }
        }

        objects[obj].dirty=false;   // Clear dirty flag — object has now been serialized
        objCount++;
    }

    // Decide message type according to what we actually have
    if(objCount && !textDirty)
        msgType=1;  // objects only

    else if(!objCount && textDirty)
        msgType=2;  // text only

    else if(objCount && textDirty)
        msgType=3;  // both

    if (textDirty)  // Append text if needed
    {
        size_t len = strlen(textBuffer);
        
        if (msgType == 3)
        {
            if (pos + 1 + len > BLENDIXSERIAL_MAX_PAYLOAD) return 0; // Type 3 includes 1-byte length prefix
            payload[pos++] = (uint8_t)len;
        }
        else // type 2 → no length byte | text fills the rest of payload
        {
            if (pos + len > BLENDIXSERIAL_MAX_PAYLOAD) return 0;
        }
        
        // Copy actual string bytes (including null if present, but usually not needed)
        memcpy(payload + pos, textBuffer, len);
        pos += len;
        
        textDirty = false;  // text has been sent
    }

    return pos;
}



// ──────────────────────────────────────────────────────────────
//  Core transmit function – Packet Builder  
// ──────────────────────────────────────────────────────────────

/**
 * Main function called when you want to send current changes.
 * Returns number of bytes written to buffer (including STX…ETX),
 * or 0 if nothing is pending (no dirty objects and no text).
 */
uint16_t blendixserial::bodBuild(uint8_t *buffer)
{
    uint8_t payload[BLENDIXSERIAL_MAX_PAYLOAD];
    uint8_t msgType=0;
    uint8_t objCount=0;

    uint16_t payloadLen=buildPayload(payload,msgType,objCount);

    if(payloadLen==0) return 0; // Early exit: nothing changed → don't send empty packet

    // ───── Fixed 5-byte header ─────
    buffer[0]=BLENDIXSERIAL_STX; // 0x02
    buffer[1]=msgType;  // 1,2, or 3
    buffer[2]=objCount; // how many objects follow
    buffer[3]=payloadLen>>8;    // length high byte
    buffer[4]=payloadLen&0xFF;  // length low byte

    // Copy prepared payload right after header
    memcpy(buffer+5,payload,payloadLen);

    // ───── Compute simple 8-bit XOR checksum ─────
    // Covers: msgType + objCount + len(2) + payload
    uint8_t checksum=0;

    for(int i=1;i<5;i++) checksum^=buffer[i];

    for(int i=0;i<payloadLen;i++) checksum^=payload[i];

    buffer[5+payloadLen]=checksum;
    buffer[6+payloadLen]=BLENDIXSERIAL_ETX; // 0x03

    return payloadLen+7; // total frame size
}


// ──────────────────────────────────────────────────────────────
// Packet Parser – incremental state machine
// ──────────────────────────────────────────────────────────────

/**
 * Feed one byte at a time (usually from Serial.read()).
 * Returns true only when a complete, valid packet was just processed.
 */
bool blendixserial::bodParse(uint8_t byte)
{
    switch(rxState)
    {

        case WAIT_STX:

        if(byte==BLENDIXSERIAL_STX) // We ignore everything until we see STX → resync point
        {
            resetRX(); // prepare for new frame
            rxState=READ_HEADER;
        }

        break;

        case READ_HEADER:
        // Collecting 4 bytes: msgType, objCount, payloadLen_high, payloadLen_low
        header[headerIndex++]=byte;

        // Reconstruct 16-bit length
        if(headerIndex==4)
        {
            payloadLen=(header[2]<<8)|header[3];

            // Safety: never accept oversized payload
            if(payloadLen>BLENDIXSERIAL_MAX_PAYLOAD)
            {
                resetRX();
                break;
            }
            rxState=READ_PAYLOAD;
        }
        break;

        case READ_PAYLOAD:

        // Fill payload buffer byte by byte
        payload[payloadIndex++]=byte;

        // When we have collected everything announced by length
        if(payloadIndex>=payloadLen)
            rxState=READ_CHECKSUM;
        break;

        case READ_CHECKSUM:
        // Store received checksum for later comparison
        checksum=byte;
        rxState=WAIT_ETX;

        break;

        case WAIT_ETX:

        if(byte==BLENDIXSERIAL_ETX)
        {
            // Verify integrity
            uint8_t calc=0; 
            for(int i=0;i<4;i++) calc^=header[i];
            for(int i=0;i<payloadLen;i++) calc^=payload[i];

            // Checksum matches → apply changes
            if(calc==checksum)
            {
                decodePacket(header[0],header[1]);
                resetRX();
                return true;    // success – caller can now read new values
            }
            // Checksum mismatch → discard
        }
        // Bad end marker or checksum → drop frame
        resetRX();
        break;

    }

    return false; // not yet finished / or error
}


// ──────────────────────────────────────────────────────────────
// Packet Decoder – applies received data to internal state
// ──────────────────────────────────────────────────────────────

/**
 * Called only after checksum has been verified.
 * Updates internal object state and/or text buffer.
 */
void blendixserial::decodePacket(uint8_t msgType,uint8_t objCount)
{
    uint16_t pos=0;

   
    if(msgType==1 || msgType==3)  // Process objects if present
    {
        for(uint8_t i=0;i<objCount;i++)
        {

            uint8_t obj=payload[pos++]; // which object
            // Read 16-bit mask telling us which axes are included
            uint16_t mask=(payload[pos]<<8)|payload[pos+1];
            pos+=2;

            objects[obj].bitmask=mask; // remember which fields are fresh

            for(uint8_t bit=0;bit<9;bit++) // Update only the values that were sent
            {
                if(mask&(1<<bit))
                {
                    objects[obj].values[bit]=readFloatBE(payload,pos);
                }
            }
        }
    }

    if (msgType == 2 || msgType == 3) // Process text if present
    {
        uint16_t textStart = pos;
        uint16_t textLen;

        if (msgType == 3)
        {
            // Type 3 includes explicit length byte
            textLen = payload[pos++];
            textStart = pos;
        }
        else // type 2
        {
            // type 2 = all remaining payload bytes are text
            textLen = payloadLen - pos;
        }

        // Protect against malicious / oversized text
        if (textLen >= BLENDIXSERIAL_MAX_TEXT)
            textLen = BLENDIXSERIAL_MAX_TEXT - 1;

        // Copy bytes into our fixed-size buffer
        memcpy(textBuffer, payload + textStart, textLen);
        textBuffer[textLen] = 0;    // always null-terminate
        textReceived = true;    // signal new text is available
    }
}