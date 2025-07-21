#pragma once

#include <SI4735.h>

class SI4735_fixed : public SI4735 {
public:

    // optimized version of seekStationProgress
    // original version calls seekStation() inside the loop, which is inefficient
    // This version calls it only once, and then polls the status, which is correct
    // and much faster way to seek progress
    void seekStationProgress(void (*showFunc)(uint16_t f), bool (*stopSeeking)(), uint8_t up_down) {
        si47x_frequency freq;
        long elapsed_seek = millis();

        if (lastMode == SSB_CURRENT_MODE)
            return;

        seekStation(up_down, 0);

        do {
            delay(maxDelaySetFrequency);
            getStatus(0, 0);

            freq.raw.FREQH = currentStatus.resp.READFREQH;
            freq.raw.FREQL = currentStatus.resp.READFREQL;
            currentWorkFrequency = freq.value;
            if (showFunc)
                showFunc(freq.value);

            if (currentStatus.resp.ERR || (stopSeeking && stopSeeking())) {
                getStatus(0, 1); // '1' in the second argument cancels the ongoing seek.
                return;
            }

        } while (!currentStatus.resp.VALID && !currentStatus.resp.BLTF && (millis() - elapsed_seek) < maxSeekTime);
    }

    // overrides the patch loading functions for potential performance gains
#if PATCH_EX_SSB
    // Optimized function to load the compressed SSB patch
    bool downloadCompressedPatch(
        const uint8_t* ssb_patch_content,
        const uint16_t ssb_patch_content_size,
        const uint16_t* cmd_0x15,
        const int16_t cmd_0x15_size_bytes) {

        uint16_t command_line = 0;
        uint16_t cmd_0x15_idx = 0;
        const uint16_t cmd_0x15_elem_count = cmd_0x15_size_bytes >> 1;

        for (uint16_t offset = 0; offset < ssb_patch_content_size; offset += 7) {

            // Select the command byte - 0x15 if the current line number is in the special list, otherwise 0x16
            uint8_t cmd = (cmd_0x15_idx < cmd_0x15_elem_count
                && pgm_read_word_near(cmd_0x15 + cmd_0x15_idx) == command_line)
                ? 0x15
                : 0x16;

            if (cmd == 0x15) cmd_0x15_idx++;

            Wire.beginTransmission(deviceAddress);
            Wire.write(cmd);

            for (uint8_t i = 0; i < 7; i++) {
                Wire.write(pgm_read_byte_near(ssb_patch_content + offset + i));
            }

            Wire.endTransmission();
            waitToSend();

            // This check ensures each 8-byte patch segment transferred successfully
            // Per AN332 (p.135), after each 0x15/0x16 command, device returns status byte
            // If ERR bit (bit 6) set, indicates failure (e.g., checksum error) — abort to prevent device instability or RAM corruption
            // Without this, partial patch may cause unpredictable behavior
            // Read 1-byte status; if bit 6 (0B01000000) set, return false to abort
            Wire.requestFrom(deviceAddress, 1);
            if (Wire.read() & 0B01000000) return false;

            command_line++;
        }

        delayMicroseconds(250);
        return true;
    }
#endif

};
