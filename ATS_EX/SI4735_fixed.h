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
        delay(100); // Wait for seek to start

        do {
            delay(200); // Increased delay for stability
            getStatus(0, 0);

            freq.raw.FREQH = currentStatus.resp.READFREQH;
            freq.raw.FREQL = currentStatus.resp.READFREQL;
            currentWorkFrequency = freq.value;
            if (showFunc) showFunc(freq.value);

            if (currentStatus.resp.ERR || (stopSeeking && stopSeeking())) {
                getStatus(0, 1); // '1' in the second argument cancels the ongoing seek.
                return;
            }

            // Check timeout
            if ((millis() - elapsed_seek) > maxSeekTime) {
                getStatus(0, 1); // Cancel on timeout
                return;
            }

        } while (!currentStatus.resp.STCINT); // Wait for Seek/Tune Complete Interrupt

        // Clear interrupt flag and get final result
        getStatus(1, 0);
        freq.raw.FREQH = currentStatus.resp.READFREQH;
        freq.raw.FREQL = currentStatus.resp.READFREQL;
        currentWorkFrequency = freq.value;
    }

    // Overloaded version without stopSeeking callback (matches original library interface)
    void seekStationProgress(void (*showFunc)(uint16_t f), uint8_t up_down) {
        seekStationProgress(showFunc, nullptr, up_down);
    }

    // ====================================================================================
    // ============================== SSB PATCH LOGIC ====================================
    // ====================================================================================
    // This section implements compressed SSB (Single Side Band) patch loading for SI4735
    // The patch enables advanced SSB features and improves reception quality
    // 
    // The compression algorithm works by:
    // 1. Storing only non-zero bytes from the original patch data
    // 2. Using offset tables to track special command positions (0x15 vs 0x16)
    // 3. Handling "cutoff" positions where data is split across two I2C transactions
    // 
    // Data structure:
    // - compressed_ssb_patch_content: actual non-zero patch bytes
    // - cmd_0x15_offsets: positions where command 0x15 is used (otherwise 0x16)
    // - cutoff_places_offsets: positions requiring special split handling
    // - cutoff_nonzero_lengths: number of non-zero bytes at cutoff positions
    //   (values < 100 indicate next line uses 0x15, >= 100 means normal continuation)
    //
    // The patch consists of 1105 lines, each sending 8 bytes via I2C
    // Base addresses change at specific line boundaries (0, 129, 405, 758, 1023+)
    // ====================================================================================
    // https://github.com/diqezit/ats20_ats_ex/issues/22#issuecomment-3237646622
    // Credit for the clever patch compression method goes to den3rats
    // ====================================================================================

#if PATCH_EX_SSB
private:
    // On-the-fly decompression logic for the SSB patch
    // This approach saves over 6KB of Flash by storing only non-zero data
    // and using small lookup tables to reconstruct the original 1105 patch lines

    // The patch is structured in memory segments not a flat array
    // This determines the correct base address for a given line index
    inline uint16_t getBaseForLine(uint16_t patch_line) {
        switch (patch_line) {
        case 0 ... 128:    return 0;
        case 129 ... 404:  return 256;
        case 405 ... 757:  return 512;
        case 758 ... 1022: return 768;
        default:           return 1024;
        }
    }

    // Calculates parameters for special "cutoff" lines that have variable data lengths
    // This avoids storing padding zeros. A single byte from cutoff_nonzero_lengths encodes
    // both the data length and whether a secondary 0x15 line must follow
    inline void getCutoffParams(uint16_t patch_line, uint16_t base,
        const uint8_t* cutoff_places_offsets,
        const uint8_t* cutoff_nonzero_lengths,
        uint8_t& cutoff_place_idx,
        uint8_t& non_zero_bytes, uint8_t& num_zero_after_0x15) {
        if ((base + pgm_read_byte_near(cutoff_places_offsets + cutoff_place_idx)) == patch_line) {
            non_zero_bytes = pgm_read_byte_near(cutoff_nonzero_lengths + cutoff_place_idx++);

            // This compacts two pieces of information into one byte:
            // 1) the length 2) the number of zeros after a follow-up 0x15 command
            if (non_zero_bytes < 100)   //Den
              num_zero_after_0x15 = 2;  //Den
            else {                      //Den
              num_zero_after_0x15 = 1;  //Den
              non_zero_bytes -= 100;    //Den
            }                           //Den

        } else {
            non_zero_bytes = 8;
            num_zero_after_0x15 = 0;
        }
    }

    // Unified function to send an 8-byte patch command via I2C
    inline bool sendPatchData(uint8_t cmd, uint8_t start_idx, uint8_t end_idx,
        const uint8_t* compressed_ssb_patch_content,
        uint16_t& patch_data_idx) {
        Wire.beginTransmission(deviceAddress);
        Wire.write(cmd);

        for (uint8_t i = 1; i < 8; i++) {
            Wire.write((i > start_idx && i < end_idx)
                ? pgm_read_byte_near(compressed_ssb_patch_content + patch_data_idx++)
                : 0x00);
        }

        Wire.endTransmission();
        waitToSend();

        Wire.requestFrom(deviceAddress, 1);
        return !(Wire.read() & 0B01000000);
    }

    // Orchestrates decompression and sending for a single patch line
    inline bool processSinglePatchLine(uint16_t& patch_line,
        const uint8_t* compressed_ssb_patch_content,
        const uint8_t* cutoff_places_offsets,
        const uint8_t* cutoff_nonzero_lengths,
        uint16_t& patch_data_idx,
        uint8_t& cutoff_place_idx) {

        uint16_t base = getBaseForLine(patch_line);
        uint8_t cmd = (patch_line == 0) ? 0x15 : 0x16;

        uint8_t non_zero_bytes;
        uint8_t num_zero_after_0x15;
        getCutoffParams(patch_line, base, cutoff_places_offsets, cutoff_nonzero_lengths,
            cutoff_place_idx, non_zero_bytes, num_zero_after_0x15);

        if (!sendPatchData(cmd, 0, non_zero_bytes, compressed_ssb_patch_content, patch_data_idx))
            return false;

        if (num_zero_after_0x15) {
            patch_line++;
            return sendPatchData(0x15, num_zero_after_0x15, 8, compressed_ssb_patch_content, patch_data_idx);
        }

        return true;
    }

public:
    // Main entry point to upload the entire compressed SSB patch
    bool downloadCompressedPatch(const uint8_t* compressed_ssb_patch_content,
        const uint8_t* cutoff_places_offsets,
        const uint8_t* cutoff_nonzero_lengths) {
        uint16_t patch_data_idx = 0;
        uint8_t cutoff_place_idx = 0;
        const uint16_t ssb_patch_lines_count = 1105;

        for (uint16_t patch_line = 0; patch_line < ssb_patch_lines_count; patch_line++) {
            if (!processSinglePatchLine(patch_line, compressed_ssb_patch_content,
                cutoff_places_offsets, cutoff_nonzero_lengths,
                patch_data_idx, cutoff_place_idx)) {
                return false;
            }
        }

        delayMicroseconds(250);
        return true;
    }
#endif

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

    #if TEST
    // 
    // -----------------------------------------------------------------------------
    // Fast SI4735 reset + setup without Arduino pinMode()/digitalWrite()
    // 1) Reduce Flash usage (avoid pulling wiring_digital.c.o when possible)
    // 2) Keep reset timing exactly like the original library
    //
    // Notes:
    // - This implementation is hard-wired for ATmega328P + RESET_PIN = D12
    // - D12 on ATmega328P is port B, bit 4 (PB4)
    // - If you move RESET_PIN to another Arduino pin, you must change the port/bit
    // -----------------------------------------------------------------------------

    static inline void resetFastD12() {
        // D12 = PB4 on ATmega328P
        // DDRB controls direction, PORTB controls output level

        DDRB |= _BV(4);      // set PB4 as OUTPUT
        delay(10);

        PORTB &= ~_BV(4);    // drive RESET LOW
        delay(10);

        PORTB |= _BV(4);     // drive RESET HIGH
        delay(10);
    }

    // -----------------------------------------------------------------------------
    // Override setup(resetPin, defaultFunction) to avoid base class reset(),
    // which uses pinMode()/digitalWrite()
    //
    // defaultFunction:
    // - 0 = FM
    // - 1 = AM (LW/MW/SW)
    // -----------------------------------------------------------------------------
    void setup(uint8_t resetPin, uint8_t defaultFunction) {
        Wire.begin();

        this->resetPin = resetPin;

        // Configure POWER_UP arguments exactly as intended by the original library:
        // CTSIEN   = 0 (no CTS interrupt)
        // GPO2OEN  = 0 (GPO2 disabled)
        // PATCH    = 0 (normal boot)
        // XOSCEN   = XOSCEN_CRYSTAL (use 32.768 kHz crystal)
        // FUNC     = defaultFunction (0=FM, 1=AM)
        // OPMODE   = SI473X_ANALOG_AUDIO (analog LOUT/ROUT)
        setPowerUp(
            0,                    // CTSIEN
            0,                    // GPO2OEN
            0,                    // PATCH
            XOSCEN_CRYSTAL,       // XOSCEN
            defaultFunction,      // FUNC (0=FM, 1=AM)
            SI473X_ANALOG_AUDIO   // OPMODE
        );

        // Hardware reset without pinMode()/digitalWrite()
        // Assumes resetPin is D12 (PB4)
        resetFastD12();

        radioPowerUp();
        setVolume(30);       // library default
        getFirmware();       // cache firmware info
        delay(250);          // legacy settle delay
    }

    // -----------------------------------------------------------------------------
    // Override getDeviceI2CAddress(resetPin) to avoid base class reset(),
    // which uses pinMode()/digitalWrite()
    // Scans both possible SI47xx I2C addresses (0x11 and 0x63)
    // -----------------------------------------------------------------------------
    int16_t getDeviceI2CAddress(uint8_t resetPin) {
        this->resetPin = resetPin;

        // Hardware reset without pinMode()/digitalWrite()
        // Assumes resetPin is D12 (PB4)
        resetFastD12();

        Wire.begin();

        // Check 0x11 (SEN low)
        Wire.beginTransmission(SI473X_ADDR_SEN_LOW);
        int16_t error = Wire.endTransmission();
        if (error == 0) {
            setDeviceI2CAddress(0);
            return SI473X_ADDR_SEN_LOW;
        }

        // Check 0x63 (SEN high)
        Wire.beginTransmission(SI473X_ADDR_SEN_HIGH);
        error = Wire.endTransmission();
        if (error == 0) {
            setDeviceI2CAddress(1);
            return SI473X_ADDR_SEN_HIGH;
        }

        // Not found
        return 0;
    }
    #endif
};
