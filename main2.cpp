#include <iostream>
#include <cstring>

#ifdef PICO_BUILD
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "hardware/adc.h"
#include "hardware/sync.h"
#include "pico/util/queue.h"
#include "pico/lock_core.h"
#include "pico/multicore.h"
#endif

#include "radlib/util/WindowAverage.h"
#include "radlib/scamp/SCAMPDemodulator.h"
//#include "radlib/rtty/RTTYDemodulator.h"

#include "DemodulatorUtil.h"
#include "StationDemodulatorListener.h"

using namespace std;
using namespace radlib;

// ===== Data Shared with Main ================================================
extern queue_t adcSampleQueue;
// This is the queue used to receive characters back from the demodulator
extern queue_t demodRxQueue;
// This is the queue used to receive status from the demodulator
extern queue_t demodStatusQueue;
// This is the queue used to send commands to the demodulator
extern queue_t demodCommandQueue;
// Constants
extern uint16_t sampleFreq;
extern uint16_t lowFreq;
// ===== Data Shared with Main ================================================

// The size of the FFT used for frequency acquisition
static const uint16_t log2fftN = 9;
static const uint16_t fftN = 1 << log2fftN;
// Space for the demodulator to work in (no dynamic memory allocation!)
static q15 trigTable[fftN];
static q15 window[fftN];
static q15 buffer[fftN];
static cq15 fftResult[fftN];

// One-time setup
//static RTTYDemodulator demod(sampleFreq, lowFreq, log2fftN, trigTable, window, 
//    fftResult, buffer);
static SCAMPDemodulator demod(sampleFreq, lowFreq, log2fftN, trigTable, window, 
    fftResult, buffer);

// Diagnostic area
//static TestDemodulatorListener::Sample samples[2000];

// An area used for analysis of ADC samples
static int16_t adcStatArea[32];
static const int16_t adcStatAreaSizeLog2 = 5;
static WindowAverage adcStatSamples(adcStatAreaSizeLog2, adcStatArea);

static uint32_t maxUs = 0;

// Center adjustment for the ADC
static int16_t adcCenterAdjust = -40;

// A mutex used to protect the internal data structures
auto_init_mutex(lock1);

static uint32_t get_us() {
    absolute_time_t at = get_absolute_time();
    return to_us_since_boot(at);
}

/**
 * Looks for any inbound ADC samples and moves them into the demodulator
 * object for processing.  Also keeps track of the maximum processing
 * time required.
 */
static void poll_adc_sample_queue() {
    while (!queue_is_empty(&adcSampleQueue)) {
        // Pull off ADC queue
        uint16_t lastSample = 0;
        queue_remove_blocking(&adcSampleQueue, &lastSample);
        // Center around zero
        lastSample -= (2048);
        lastSample += adcCenterAdjust;
        // Capture sample for statistical analysis
        adcStatSamples.sample((int16_t)lastSample);
        // TODO: REMOVE FLOAT
        const float lastSampleF32 = lastSample / 2048.0;
        const q15 lastSampleQ15 = f32_to_q15(lastSampleF32);
        uint32_t start = get_us();
        demod.processSample(lastSampleQ15);
        uint32_t end = get_us();
        maxUs = std::max(maxUs, (end - start));
    }
}

static void poll_demod_command_queue() {
    while (!queue_is_empty(&demodCommandQueue)) {
        DemodulatorCommand cmd;
        queue_remove_blocking(&demodCommandQueue, &cmd);
        if (cmd.cmd == DemodulatorCommand::Command::RESET) {
            demod.reset();
        }
        else if (cmd.cmd == DemodulatorCommand::Command::SET_MARK) {
            demod.setFrequencyLock(cmd.markFreq);
        }
    }
}

void main2() {

    // Create the demodulator listener
    //TestDemodulatorListener demodListener(cout, samples, 180);
    //demodListener.setTriggerMode(TestDemodulatorListener::TriggerMode::ON_LOCK);
    //demodListener.setTriggerDelay(0);
    StationDemodulatorListener demodListener(&demodRxQueue);
    demod.setListener(&demodListener);

    //demod.setSymbolSpread(-170);
    //demod.setFrequencyLock(667);

    const uint32_t stausIntervalMs = 500;
    absolute_time_t nextStatusTime = make_timeout_time_ms(stausIntervalMs);
    
    // Event loop
    while (true) {

        mutex_enter_blocking(&lock1);

        // Check for inbound ADC samples
        poll_adc_sample_queue();

        // Check for inbound demodulator commands
        poll_demod_command_queue();

        // Generate demodulator status periodically
        if (absolute_time_diff_us(get_absolute_time(), nextStatusTime) <= 0) {
            
            // Make a status message and send back 
            DemodulatorStatus status;
            status.isLocked = demod.isFrequencyLocked();
            status.lockedMarkFreq = demod.getMarkFreq();

            queue_try_add(&demodStatusQueue, &status);

            nextStatusTime = make_timeout_time_ms(stausIntervalMs);
        }

        mutex_exit(&lock1);
    }
}

bool operator== (const DemodulatorStatus& lhs, const DemodulatorStatus& rhs) {
    return lhs.isLocked == rhs.isLocked &&
      lhs.lockedMarkFreq == rhs.lockedMarkFreq;
}

bool getLiveSpectrum(q15* mag, uint16_t size) {
    if (!mutex_is_initialized(&lock1)) {
        return false;
    }
    mutex_enter_blocking(&lock1);
    for (uint16_t i = 0; i < size && i < fftN; i++) {
        mag[i] = fftResult[i].approx_mag_q15();
    }
    mutex_exit(&lock1);
    return true;
}
