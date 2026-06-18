/* Current Limitations:
- 1x Playback speed only
- 16-bit, mono files only (otherwise fun weirdness can happen).
- Only 1 file playing back at a time.
- Not sure how this would interfere with trying to use the SDCard/FatFs outside of
this module. However, by using the extern'd SDFile, etc. I think that would break things.
*/
#pragma once
#ifndef WAV_HEXA_PLAYER_H
#define WAV_HEXA_PLAYER_H /**< Macro */

#include "Utils.h"
#if USE_DAISY

#include "daisy_core.h"
#include "util/wav_format.h"
#include "ff.h"

#define WAV_FILENAME_MAX \
    256 /**< Maximum LFN (set to same in FatFs (ffconf.h) */

namespace daisy
{
// TODO: add bitrate, samplerate, length, etc.
/** Struct containing details of Wav File. */
struct WavHexFileInfo
{
    WAV_FormatTypeDef raw_data;               /**< Raw wav data */
    char              name[WAV_FILENAME_MAX]; /**< Wav filename */
};

/* 
TODO:
- Make template-y to reduce memory usage.
*/


/** Wav Player that will load .wav files from an SD Card,
and then provide a method of accessing the samples with
double-buffering. */
class WavHexaPlayer
{
  public:
    WavHexaPlayer() {}
    ~WavHexaPlayer() {}

    /** Initializes the WavHexaPlayer, loading up to max_files of wav files from an SD Card. */
    void Init(const char* search_path);

    /** Opens the file at index sel for reading.
    \param sel File to open
     */
    int Open(size_t sel);

    /** Closes whatever file is currently open.
    \return &
     */
    int Close();
    int CloseHex(int idx);

    /** \return The next sample if playing, otherwise returns 0 */
    int16_t Stream();
    int16_t StreamHex(int idx);


    /** Collects buffer for playback when needed. */
    void Prepare();
    void PrepareHex(int idx);

    /** Opens a file for reading in a specific hex buffer */
    int OpenHex(int idx, size_t sel);

    /** Resets the playback position to the beginning of the hex file immediately */
    void RestartHex(int idx);

    /** Resets the playback position to the beginning of the file immediately */
    void Restart();

    /** Updates all hex buffers for playback when needed. */
    void update();

    /** Sets whether or not the current file will repeat after completing playback. 
    \param loop To loop or not to loop.
    */
    inline void SetLooping(bool loop) { looping_ = loop; }
    inline void SetLoopingHex(int idx, bool loop) { hex_looping_[idx] = loop; }

    /** \return Whether the WavPlayer is looping or not. */
    inline bool GetLooping() const { return looping_; }
    inline bool GetLoopingHex(int idx) const { return hex_looping_[idx]; }

    /** \return The number of files loaded by the WavPlayer */
    inline size_t GetNumberFiles() const { return file_cnt_; }

    /** \return currently selected file.*/
    inline size_t GetCurrentFile() const { return file_sel_; }

  private:
    enum BufferState
    {
        BUFFER_STATE_IDLE,
        BUFFER_STATE_PREPARE_0,
        BUFFER_STATE_PREPARE_1,
    };

    BufferState GetNextBuffState();

    static constexpr size_t kMaxFiles   = 8;
    static constexpr size_t kBufferSize = 16384; // Augmenté pour amortir le seek time sur 6 fichiers (~170ms de battement)
    WavHexFileInfo             file_info_[kMaxFiles];
    size_t                  file_cnt_, file_sel_;
    volatile BufferState    buff_state_;
    int16_t                 buff_[kBufferSize];
    size_t                  read_ptr_;
    bool                    looping_, playing_;
    FIL                     fil_;
    int16_t                 hex_buff_[6][kBufferSize];
    FIL                     hex_fil_[6];
    volatile BufferState    hex_buff_state_[6];
    size_t                  hex_read_ptr_[6];
    bool                    hex_looping_[6];
    bool                    hex_playing_[6];
    size_t                  hex_file_sel_[6];
};

} // namespace daisy

#endif // USE_DAISY
#endif
