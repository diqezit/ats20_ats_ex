#pragma once

#include "SI4735.h"

class SI4735_fixed : public SI4735 {

private:

    // Read current frequency from last getStatus() response
    // and sync internal tracking variable
    inline uint16_t readStatusFreq() {
        si47x_frequency f;
        f.raw.FREQH = currentStatus.resp.READFREQH;
        f.raw.FREQL = currentStatus.resp.READFREQL;
        currentWorkFrequency = f.value;
        return f.value;
    }

public:

    // ====================================================================================
    // ============================== SEEK ================================================
    // ====================================================================================
    //
    // Original seekStation() is called inside a loop which re-issues the command
    // on every iteration. This version issues it once then polls status — correct
    // per AN332 and much faster

    uint16_t seekStationProgressGetFrequency(void (*showFunc)(uint16_t f),
        bool (*stopSeeking)(),
        uint8_t up_down) {
        if (lastMode == SSB_CURRENT_MODE)
            return (uint16_t)currentWorkFrequency;

        uint32_t seekStart = millis();

        seekStation(up_down, 0);
        delay(100);

        do {
            delay(200);
            getStatus(0, 0);

            uint16_t f = readStatusFreq();
            if (showFunc) showFunc(f);

            if (currentStatus.resp.ERR
                || (stopSeeking && stopSeeking())
                || ((millis() - seekStart) > maxSeekTime)) {
                // Cancel seek — response contains the actual chip frequency,
                // which may have changed during showFunc delay
                getStatus(0, 1);
                return readStatusFreq();
            }

        } while (!currentStatus.resp.STCINT);

        getStatus(1, 0); // clear STCINT
        return readStatusFreq();
    }

    void seekStationProgress(void (*showFunc)(uint16_t f),
        bool (*stopSeeking)(),
        uint8_t up_down) {
        (void)seekStationProgressGetFrequency(showFunc, stopSeeking, up_down);
    }

    // ====================================================================================
    // ============================== RDS MINI ============================================
    // ====================================================================================
    //
    // Minimal RDS for ATmega328P flash limits
    //  - enables RDS on FM without pulling full setRdsConfig()/getRdsStatus()/text buffers
    //  - queries FM_RDS_STATUS with a tiny I2C routine
    //  - provides block accessors for lightweight decoding in RDS.h
    //
    // IMPORTANT: do NOT call heavy RDS functions elsewhere
    //   (setRdsConfig/getRdsStatus/getRdsText*/getRdsAllData/getRdsTime)
    //   — LTO will link far more code and overflow Flash
    //
    // ====================================================================================

    void rdsEnableMini() {
        sendProperty(FM_RDS_CONFIG, 0xAA01);
        sendProperty(FM_RDS_INT_FIFO_COUNT, 1);
    }

    bool rdsQueryMini() {
        waitToSend();

        Wire.beginTransmission(deviceAddress);
        Wire.write(FM_RDS_STATUS);
        Wire.write((uint8_t)0x00);
        Wire.endTransmission();

        waitToSend();

        Wire.requestFrom((uint8_t)deviceAddress, (uint8_t)13);
        for (uint8_t i = 0; i < 13; i++) {
            currentRdsStatus.raw[i] = (uint8_t)Wire.read();
        }

        return !currentRdsStatus.resp.ERR;
    }

    inline const uint8_t* rdsBlockCDPtr() const {
        return &currentRdsStatus.raw[8];  // CH,CL,DH,DL
    }

    inline uint16_t rdsGetBlockB() const {
        uint8_t h = currentRdsStatus.raw[6];
        uint8_t l = currentRdsStatus.raw[7];
        return ((uint16_t)h << 8) | l;
    }

    inline uint8_t rdsGetBlockDH() const { return currentRdsStatus.resp.BLOCKDH; }
    inline uint8_t rdsGetBlockDL() const { return currentRdsStatus.resp.BLOCKDL; }
    inline uint8_t rdsGetBlockCH() const { return currentRdsStatus.resp.BLOCKCH; }
    inline uint8_t rdsGetBlockCL() const { return currentRdsStatus.resp.BLOCKCL; }

    // ====================================================================================
    // ============================== SSB PATCH LOGIC ====================================
    // ====================================================================================
    //
    // Compressed SSB patch loading for SI4735
    //
    // Compression saves over 6KB of Flash by storing only non-zero bytes and using
    // small lookup tables to reconstruct 1105 original patch lines on the fly
    //
    // Algorithm:
    //  1. Store only non-zero bytes from original patch data
    //  2. Use offset tables to track special command positions (0x15 vs 0x16)
    //  3. Handle "cutoff" positions where data splits across two I2C transactions
    //
    // Data layout:
    //  - compressed_ssb_patch_content: non-zero patch bytes
    //  - cutoff_places_offsets: positions requiring split handling
    //  - cutoff_nonzero_lengths: encodes both data length and 0x15 follow-up flag
    //    (values < 100 = next line uses 0x15, >= 100 = normal continuation)
    //
    // Base addresses change at line boundaries: 0, 129, 405, 758, 1023+
    //
    // ====================================================================================
    // https://github.com/diqezit/ats20_ats_ex/issues/22#issuecomment-3237646622
    // Credit for the clever patch compression method goes to den3rats
    // ====================================================================================

#if PATCH_EX_SSB
private:

    // Patch memory is segmented — this maps line index to base address
    inline uint16_t getBaseForLine(uint16_t patch_line) {
        switch (patch_line) {
        case 0 ... 128:    return 0;
        case 129 ... 404:  return 256;
        case 405 ... 757:  return 512;
        case 758 ... 1022: return 768;
        default:           return 1024;
        }
    }

    // Cutoff lines have variable non-zero length — one byte from the length table
    // encodes both data count and whether a secondary 0x15 line must follow
    inline void getCutoffParams(uint16_t patch_line, uint16_t base,
        const uint8_t* cutoff_places_offsets,
        const uint8_t* cutoff_nonzero_lengths,
        uint8_t& cutoff_place_idx,
        uint8_t& non_zero_bytes, uint8_t& num_zero_after_0x15) {
        if ((base + pgm_read_byte_near(cutoff_places_offsets + cutoff_place_idx)) == patch_line) {
            non_zero_bytes = pgm_read_byte_near(cutoff_nonzero_lengths + cutoff_place_idx++);

            // Two pieces of info packed into one byte:
            // 1) actual data length  2) zeros after follow-up 0x15 command
            if (non_zero_bytes < 100)       //Den
                num_zero_after_0x15 = 2;    //Den
            else {                          //Den
                num_zero_after_0x15 = 1;    //Den
                non_zero_bytes -= 100;      //Den
            }                               //Den

        } else {
            non_zero_bytes = 8;
            num_zero_after_0x15 = 0;
        }
    }

    // Sends one 8-byte patch command via I2C with zero-padding outside data range
    inline bool sendPatchData(uint8_t cmd, uint8_t start_idx, uint8_t end_idx,
        const uint8_t* compressed_ssb_patch_content,
        uint16_t& patch_data_idx) {
        Wire.beginTransmission(deviceAddress);
        Wire.write(cmd);

        for (uint8_t i = 1; i < 8; i++) {
            const uint8_t v = (uint8_t)((i > start_idx && i < end_idx)
                ? pgm_read_byte_near(compressed_ssb_patch_content + patch_data_idx++)
                : 0);
            Wire.write(v);
        }

        Wire.endTransmission();
        waitToSend();

        Wire.requestFrom(deviceAddress, 1);
        return !(Wire.read() & 0B01000000);
    }

    // Decompresses and sends a single patch line, handling cutoff splits
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

    // FM stereo/mono blend per AN332
    // force_mono=true: force mono (hides hiss on weak signals)
    // force_mono=false: automatic blend (default chip behavior)
    //
    // AN332 quirk:
    //  RSSI/SNR thresholds — higher = more mono (127=force mono, 0=force stereo)
    //  Multipath threshold — INVERTED (0=force mono, 100=force stereo)
    void setFmStereoMode(bool force_mono) {
        static const uint16_t fm_settings[] PROGMEM = {
            FM_BLEND_RSSI_STEREO_THRESHOLD_PROP,
                (FM_BLEND_RSSI_STEREO_DEFAULT << 8) | 127,
            FM_BLEND_RSSI_MONO_THRESHOLD_PROP,
                (FM_BLEND_RSSI_MONO_DEFAULT << 8) | 127,

                FM_BLEND_SNR_STEREO_THRESHOLD_PROP,
                    (FM_BLEND_SNR_STEREO_DEFAULT << 8) | 127,
                FM_BLEND_SNR_MONO_THRESHOLD_PROP,
                    (FM_BLEND_SNR_MONO_DEFAULT << 8) | 127,

                    FM_BLEND_MULTIPATH_STEREO_THRESHOLD_PROP,
                        (FM_MP_STEREO_THR_DEFAULT << 8) | 0,
                    FM_BLEND_MULTIPATH_MONO_THRESHOLD_PROP,
                        (FM_MP_MONO_THR_DEFAULT << 8) | 0
        };

        const uint8_t entries = (sizeof(fm_settings) / sizeof(fm_settings[0])) / 2;

        for (uint8_t i = 0; i < entries; i++) {
            uint16_t addr = pgm_read_word(&fm_settings[i * 2]);
            uint16_t packed = pgm_read_word(&fm_settings[i * 2 + 1]);

            uint8_t autoVal = packed >> 8;
            uint8_t monoVal = packed & 0xFF;

            sendProperty(addr, force_mono ? monoVal : autoVal);
        }
    }

    // Set SSB audio bandwidth + sideband cutoff in one I2C send
    inline void setSSBAudioBwAndCutoff(uint8_t audioBw, uint8_t cutoff) {
        currentSSBMode.param.AUDIOBW = audioBw;
        currentSSBMode.param.SBCUTFLT = cutoff;
        sendSSBModeProperty();
    }

    // Batch SSB mode config — one I2C transaction instead of five
    inline void configureSSBModeBatch(
        uint8_t avcen,
        uint8_t dspAfcDis,
        uint8_t avcDiv,
        uint8_t audioBw,
        uint8_t smuteSel
    ) {
        // Clear to avoid stale reserved bits
        currentSSBMode.raw[0] = 0;
        currentSSBMode.raw[1] = 0;

        currentSSBMode.param.AVCEN = avcen;
        currentSSBMode.param.DSP_AFCDIS = dspAfcDis;
        currentSSBMode.param.AVC_DIVIDER = avcDiv;
        currentSSBMode.param.AUDIOBW = audioBw;
        currentSSBMode.param.SMUTESEL = smuteSel;

        sendSSBModeProperty();
    }

#if TEST
    // -----------------------------------------------------------------------------
    // Fast SI4735 reset via direct port manipulation
    // Avoids Arduino pinMode()/digitalWrite() to keep wiring_digital.c.o unlinked
    //
    // Hard-wired for ATmega328P D12 = PB4
    // If RESET_PIN moves to another pin — change port/bit below
    // -----------------------------------------------------------------------------

    static void __attribute__((noinline)) delay10ms() {
        delay(10);
    }

    static inline void resetFastD12() {
        DDRB |= _BV(4);    // PB4 OUTPUT
        delay10ms();
        PORTB &= ~_BV(4);    // RESET LOW
        delay10ms();
        PORTB |= _BV(4);    // RESET HIGH
        delay10ms();
    }

    // Override setup() to use direct port reset instead of base class reset()
    void setup(uint8_t resetPin, uint8_t defaultFunction) {
        (void)resetPin;

        setPowerUp(
            0,                    // CTSIEN
            0,                    // GPO2OEN
            0,                    // PATCH
            XOSCEN_CRYSTAL,       // XOSCEN
            defaultFunction,      // FUNC (0=FM, 1=AM)
            SI473X_ANALOG_AUDIO   // OPMODE
        );

        resetFastD12();
        radioPowerUp();
        setVolume(30);
        getFirmware();
        delay(250);
    }

    // Override getDeviceI2CAddress() to use direct port reset
    // Scans both SI47xx addresses: 0x11 (SEN low) and 0x63 (SEN high)
    int16_t getDeviceI2CAddress(uint8_t resetPin) {
        this->resetPin = resetPin;
        resetFastD12();

        // Wire.begin() expected from OLED init earlier in startup sequence

        Wire.beginTransmission(SI473X_ADDR_SEN_LOW);
        int16_t error = Wire.endTransmission();
        if (error == 0) {
            setDeviceI2CAddress(0);
            return SI473X_ADDR_SEN_LOW;
        }

        Wire.beginTransmission(SI473X_ADDR_SEN_HIGH);
        error = Wire.endTransmission();
        if (error == 0) {
            setDeviceI2CAddress(1);
            return SI473X_ADDR_SEN_HIGH;
        }

        return 0;
    }
#endif
};
