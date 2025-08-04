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

    // soft update of the AM RSSI
    // instead of a disruptive setFrequency() for push and update status - this uses the dedicated AM_RSQ_STATUS (0x43)
    // command via the base library getter
    // This command designed for polling signal quality without interrupting the audio path//
    void softAmRssiUpdate() {

        // base lib store 0x43 response in separate `currentRqsStatus` buff eer
        getCurrentReceivedSignalQuality(0);

        // copy result back to main `currentStatus` buffer
        //   makes the fresh RSSI value compatible with the standard getReceivedSignalStrengthIndicator() method,
        // which expects data in the format of a TUNE_STATUS (0x42) response
        // per  AN332 - RSSI is at the same offset (RESP4) in both responses
        currentStatus.raw[4] = currentRqsStatus.raw[4];
    }

    // Configures FM stereo decoder for forced mono or automatic blend mode
    // This single function replaces separate On/Off methods to save flash mem
    void setFmStereoMode(bool force_mono) {
        // Data is stored as pairs: { address, (auto_value << 8) | mono_value }
        static const uint16_t fm_settings[] PROGMEM = {
            0x1800, (49 << 8) | 127,    // FM_BLEND_RSSI_STEREO_THRESHOLD
            0x1801, (30 << 8) | 127,    // FM_BLEND_RSSI_MONO_THRESHOLD
            0x1804, (27 << 8) | 127,    // FM_BLEND_SNR_STEREO_THRESHOLD
            0x1805, (14 << 8) | 127,    // FM_BLEND_SNR_MONO_THRESHOLD
            0x1808, (20 << 8) | 0,      // FM_BLEND_MULTIPATH_STEREO_THRESHOLD
            0x1809, (60 << 8) | 0       // FM_BLEND_MULTIPATH_MONO_THRESHOLD
        };

        const uint8_t entries = (sizeof(fm_settings) / sizeof(fm_settings[0])) / 2;

        for (uint8_t i = 0; i < entries; i++) {
            // read address and packed values from the flat array
            uint16_t addr = pgm_read_word(&fm_settings[i * 2]);
            uint16_t packed = pgm_read_word(&fm_settings[i * 2 + 1]);

            // unpack the two 8-bit values
            uint8_t autoVal = packed >> 8;
            uint8_t monoVal = packed & 0xFF;

            sendProperty(addr, force_mono ? monoVal : autoVal);
        }
    }
};
