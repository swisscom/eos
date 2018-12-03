/***************************************************************************************
Copyright (c) 2015, Swisscom (Switzerland) Ltd.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Swisscom nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Architecture and development:
Vladimir Maksovic <Vladimir.Maksovic@swisscom.com>
Milenko Boric Herget <Milenko.BoricHerget@swisscom.com>
Dario Vieceli <Dario.Vieceli@swisscom.com>
***************************************************************************************/


#ifndef CRON_PLYR_H_
#define CRON_PLYR_H_

#include "eos_error.h"
#include "eos_types.h"
#include "sink.h"

/**
 * \enum cron_plyr_disp_t
 * Display definitions.
 */
typedef enum cron_plyr_disp
{
	CRON_PLYR_DISP_ANY = 0x1, /**< Any available display.   */
	CRON_PLYR_DISP_MAIN,      /**< Main display.            */
	CRON_PLYR_DISP_PIP        /**< PiP display.             */
} cron_plyr_disp_t;

/**
 * \enum cron_plyr_wm_t
 * Watermark definitions.
 */
typedef enum cron_plyr_wm
{
	CRON_PLYR_WM_LOW = 0x1,/**< Low watermark.     */
	CRON_PLYR_WM_NORMAL,   /**< Normal watermark.  */
	CRON_PLYR_WM_HIGH      /**< High watewrmark.   */
} cron_plyr_wm_t;

/**
 * \enum cron_plyr_ev_t
 * Event types.
 */
typedef enum cron_plyr_ev
{
	CRON_PLYR_EV_CAN_SYNC,	//!< CRON_PLYR_EV_CAN_SYNC
	CRON_PLYR_EV_IN_SYNC, 	//!< CRON_PLYR_EV_IN_SYNC
	CRON_PLYR_EV_DEC_BUFF,	//!< CRON_PLYR_EV_DEC_BUFF
	CRON_PLYR_EV_WM,      	//!< CRON_PLYR_EV_WM
	CRON_PLYR_EV_FRM,     	//!< CRON_PLYR_EV_FRM
	CRON_PLYR_EV_VDEC_INFO,	//!< CRON_PLYR_EV_VDEC_INFO
	CRON_PLYR_EV_VDEC_RES,	//!< CRON_PLYR_EV_VDEC_RES
	CRON_PLYR_EV_VOUT_END,  //!< CRON_PLYR_EV_VOUT_END
	CRON_PLYR_EV_VOUT_BEGIN,//!< CRON_PLYR_EV_VOUT_BEGIN
	CRON_PLYR_EV_ERR      	//!< CRON_PLYR_EV_ERR
} cron_plyr_ev_t;

/**
 * \enum cron_plyr_err_t
 * Error event codes.
 */
typedef enum cron_plyr_err
{
	CRON_PLYR_ERR_GEN = 0xF0,/**< General error.    */
	CRON_PLYR_ERR_DMX,       /**< Demuxing error.   */
	CRON_PLYR_ERR_DEC,       /**< Decoding error.   */
	CRON_PLYR_ERR_REN        /**< Rendering error   */
} cron_plyr_err_t;

/**
 * \enum cron_plyr_es_t
 * cron_plyr_es_t Elementary Stream types.
 */
typedef enum cron_plyr_es
{
	CRON_PLYR_ES_NONE,/**< Unknown/Invalidated.     */
	CRON_PLYR_ES_AUD, /**< Audio.                   */
	CRON_PLYR_ES_VID, /**< Video.                   */
	CRON_PLYR_ES_TTXT,/**< Teletext.                */
	CRON_PLYR_ES_SUB, /**< Subtitles.               */
	CRON_PLYR_ES_CC   /**< Closed Caption.          */
} cron_plyr_es_t;

/**
 * \enum cron_plyr_es_t
 * cron_plyr_es_t Elementary Stream types.
 */
typedef enum cron_vol_lvl
{
	CRON_VOL_LVL_LIGHT = 0,
	CRON_VOL_LVL_NORMAL,
	CRON_VOL_LVL_HEAVY
} cron_vol_lvl_t;

/**
 * \union cron_ply_ev_data_t
 * Event data.
 */
typedef union cron_ply_ev_data
{
	struct
	{
		uint32_t millis;
	} dec_buff;
	struct
	{
		cron_plyr_wm_t wm;
	} wm;
	struct
	{
		uint64_t pts;
	} first_frm;
	struct
	{
		uint64_t pts;
	} frm;
	struct
	{
		cron_plyr_err_t code;
	} err;
	struct
	{
		uint32_t width;
		uint32_t height;
		uint32_t interlaced;
		uint32_t frame_rate_num;
		uint32_t frame_rate_den;
		uint32_t sar_width;
		uint32_t sar_height;
	} vdec_info;
	struct
	{
		uint32_t width;
		uint32_t height;
	} vdec_res;
} cron_ply_ev_data_t;

/**
 * Event callback. It is called synchronously from the chronos player code.
 * The call should exit as soon as possible.
 * @param[in] opaque User data.
 * @param[in] event Event.
 * @param[in] data Event data. For some events, it can be `NULL`
 * @return EOS_ERROR_OK on success or error code, if there was a problem
 * in an event handling.
 */
typedef eos_error_t (*cron_plyr_ev_cbk_t) (void* opaque, cron_plyr_ev_t event,
		cron_ply_ev_data_t* data);

/**
 * Chronos player handle.
 */
typedef struct cron_plyr cron_plyr_t;

/********************************************************************
 *                                                                  *
 *                      Global/system functions                     *
 *                                                                  *
 ********************************************************************/

/** \brief System/global initialization.
 *
 * General initialization is done during this function call.
 * Call to this function should precede all other Cronos Player calls.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_sys_init(void);
/** \brief System/global deinitialization.
 *
 * After Cronos Player functionalities are not needed this function will be called.
 * System resource should be release and session should be gracefully ended.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_sys_deinit(void);
/** \brief Returns player name.
 *
 * Returned name string will be used for logging purposes.
 * @return Name string, or `NULL`.
 */
const char* cron_plyr_sys_get_name(void);
/** \brief Get capabilities of this player.
 *
 * Each player object has same capabilities,
 * and those are returned with this function.
 * @param[out] caps Container argument for returned capabilities.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_sys_get_caps(uint64_t* caps);
/** \brief Get IO supported by this player.
 *
 * Each player object has same IO support,
 * and it is returned with this function.
 * @param[out] plug Container argument for returned IO type.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_sys_get_plug(link_io_type_t* plug);

/********************************************************************
 *                                                                  *
 *                    Player handle functions                       *
 *                                                                  *
 ********************************************************************/

/** \brief Player constructor.
 *
 * Creates a player object and returns its handle.
 * Player will use passed display to show video content.
 * @param[out] player Pointer which will contain created player handle.
 * @param[in] disp Display for the video.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_create(cron_plyr_t** player, cron_plyr_disp_t disp);
/** \brief Player destructor
 *
 * Destructs player object and releases all its system resources.
 * @param[in,out] player Pointer to the layer which should be destructed.
 * If everything goes well, this pointer will be nullified on function exit.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_destroy(cron_plyr_t** player);

/********************************************************************
 *                                                                  *
 *                    Player setup functions                        *
 *                                                                  *
 ********************************************************************/

/** \brief Declares what media needs to be played by the player.
 * Sets up media descriptor. The player should decide at this time
 * whether it can playback such media content.
 * <b>NOTE:<\b> Not all fields in the media descriptor will be set.
 * Only those fields which are available at the time will be set.
 * @param[in] player Chronos player handle.
 * @param[in] media Media descriptor. Must not be NULL.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_set_media_desc(cron_plyr_t* player,
		eos_media_desc_t* media);

/** \brief Enables/disables free run mode.
 *
 * When in the free run mode, player don't care about A/V sync.
 * This behavior helps in case of big A/V delay, when it is required
 * that the playback starts as soon as possible.
 * This function may be called before or during playback session.
 * @param[in] player Chronos player handle.
 * @param[in] enable `true` to enable, or `false` to disable.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_set_freerun(cron_plyr_t* player, bool enable);
/** \brief Gets free run flag value.
 *
 * Returns current value for the free run flag.
 * @param[in] player Chronos player handle.
 * @param[out] enable `true` if enabled, `false` otherwise.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_get_freerun(cron_plyr_t* player, bool* enable);
/** \brief Sets target synchronization time.
 *
 * Sets in what time it is expected that the player reaches A/V sync.
 * This function affects player behavior only when free run is active.
 * @see cron_plyr_set_freerun()
 * @param[in] player Chronos player handle.
 * @param[in] millis Target (relative) time in milliseconds.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_set_sync_time(cron_plyr_t* player,
		uint32_t millis);
/** \brief Gets target sync time.
 *
 * Returns current A/V sync target time in milliseconds.
 * @param[in] player Chronos player handle.
 * @param[out] millis Current relative target sync time in milliseconds.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_get_sync_time(cron_plyr_t* player,
		uint32_t* millis);
/** \brief Sets decoding threshold.
 *
 * Sets how much data (in milliseconds) player should accumulate
 * before the playback starts.
 * Decoding threshold change takes affect only on next playback.
 * @param[in] player Chronos player handle.
 * @param[in] millis Target buffer time in milliseconds.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_set_dec_trshld(cron_plyr_t* player, uint32_t millis);
/** \brief Gets decoding threshold.
 *
 * Gets how much data (in milliseconds) player is accumulated
 * before the playback starts.
 * @param[in] player Chronos player handle.
 * @param[out] millis Playback threshold time in milliseconds.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_get_dec_trshld(cron_plyr_t* player, uint32_t* millis);
/** \brief Sets keep last frame flag.
 *
 * Enables or disables 'keep last video frame' functionality.
 * When enabled, after playback is stopped (or even the player is destroyed),
 * the last video frame will be kept on the video output (video blanking will not be active).
 * This flag can be changed at any time (even if playback is not active).
 * Calling this function when playeback is stopped will NOT blanc the screen.
 * @see cron_plyr_vid_blank()
 * @param[in] player Chronos player handle.
 * @param[in] enable `true` to enable, or `false` to disable.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_set_keep_frm(cron_plyr_t* player, bool enable);
/** \brief Gets keep last frame flag.
 *
 * Returns current value for the 'keep last video frame' flag.
 * @param[in] player Chronos player handle.
 * @param[out] enable `true` if enabled, `false` otherwise.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_get_keep_frm(cron_plyr_t* player, bool* enable);
/** \brief Gets current amount of synchronized A/V data.
 *
 * This function will return the amount of A/V the buffered data (in milliseconds)
 * which is synchronized (audio and video are in sync).
 * Calling this function makes sense only if playback is active.
 * @param[in] player Chronos player handle.
 * @param[out] millis Buffered A/V data, which is in sync (in millisecond).
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_get_synced(cron_plyr_t* player, uint32_t* millis);
/** \brief Gets delay between audio and video.
 *
 * Returns the delay (in millisecond) between audio and video. It is assumed
 * that this delay is constant.
 * Calling this function makes sense only if playback is active.
 * @param[in] player Chronos player handle.
 * @param[out] millis A/V delay in milliseconds
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_get_av_delay(cron_plyr_t* player, int32_t* millis);
/** \brief Sets watermark level.
 *
 * Sets buffer watermark level in milliseconds. Level must be set separately
 * for each watermark type. Player can return error, if this function
 * is called after the playbeck starts.
 * @see cron_plyr_wm_t
 * @param[in] player Chronos player handle.
 * @param[in] wm Watermark type.
 * @param[in] millis The buffer level (in milliseconds).
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_set_wm(cron_plyr_t* player, cron_plyr_wm_t wm,
		uint32_t millis);

/********************************************************************
 *                                                                  *
 *                    Elementary stream control                     *
 *                                                                  *
 ********************************************************************/

/** \brief Activates elementary stream by media descriptor index.
 *
 * Sets active elementary stream. This call just "selects" given ES.
 * This function must be called for each type of elementary stream
 * (audio, video, TTXT...). Index is the identifier in the media descriptor.
 * @see eos_media_desc_t
 * @param[in] player Chronos player handle.
 * @param[in] es_idx Elementary stream index.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_es_set(cron_plyr_t* player, uint8_t es_idx);
/** \brief Deactivates elementary stream by media descriptor index.
 *
 * Invalidates ES selection.
 * @see eos_media_desc_t
 * @see cron_plyr_es_set
 * @param[in] player Chronos player handle.
 * @param[in] es_idx Elementary stream index.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_es_unset(cron_plyr_t* player, uint8_t es_idx);
/** \brief Gets selected elementary stream type.
 *
 * Returns selected ES type for given index in the media descriptor.
 * If ES on passed index is NOT selected, es will be set to CRON_PLYR_ES_NONE.
 * @see eos_media_desc_t
 * @param[in] player Chronos player handle.
 * @param[in] es_idx ES index in the media descriptor.
 * @param[out] es Elementary stream.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_es_get(cron_plyr_t* player, uint8_t es_idx,
		cron_plyr_es_t* es);
/** \brief Starts playback of the elementary stream.
 *
 * This function is called when ES switching is required (e.g. changing audio track).
 * In such case ES is stopped, than different ID is set, and after that,
 * this function is called to start different elementary stream.
 * ES ID must be already set.
 * @see cron_plyr_es_set()
 * @see cron_plyr_es_stop()
 * @param[in] player Chronos player handle.
 * @param[in] es Elementary stream type.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_es_start(cron_plyr_t* player, cron_plyr_es_t es);
/** \brief Stops elementary stream.
 *
 * Stopping of the elementary stream is done with this function.
 * Stream must be already started for this function to take effect.
 * @param[in] player Chronos player handle.
 * @param[in] es Elementary stream type.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_es_stop(cron_plyr_t* player, cron_plyr_es_t es);
/** \brief Enables/disables given elementary stream.
 *
 * Elementary stream is enabled/disabled with this call.
 * Disabling some ES is effectively muting (synchronization must NOT
 * be done when ES is enabled).
 * @param[in] player Chronos player handle.
 * @param[in] es Elementary stream type.
 * @param enabled `true` to enable, or `false` to disable.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_es_enable(cron_plyr_t* player, cron_plyr_es_t es,
		bool enabled);


/********************************************************************
 *                                                                  *
 *                             Events                               *
 *                                                                  *
 ********************************************************************/

/** \brief Registers/unregisters event handler.
 * Registers and event handler.
 * @param[in] player Chronos player handle.
 * @param[in] cbk Event callback to register. If `ǸULL` is passed,
 * previously registered callback is removed.
 * @param[in] opaque User data. Will be passed on every callback call.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_reg_ev_hnd(cron_plyr_t* player, cron_plyr_ev_cbk_t cbk, void* opaque);


/********************************************************************
 *                                                                  *
 *                        Playback control                          *
 *                                                                  *
 ********************************************************************/

/** \brief Starts the playback.
 * After all Elementary Streams are set, this function must be called so that actual playback starts.
 * Same goes for stoppedstreams.
 * @param[in] player Chronos player handle.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_start(cron_plyr_t* player);
/** \brief Stops the playback.
 * Stops currently running playback.
 * @param[in] player Chronos player handle.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_stop(cron_plyr_t* player);
/** \brief Pauses the playback.
 * Pauses currently running playback.
 * @param[in] player Chronos player handle.
 * @param[in] buffering keep buffering data during pause state.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_pause(cron_plyr_t* player, bool buffering);
/** \brief Resumes paused playback.
 * Resumes the stream playback if it was previously paused.
 * @param[in] player Chronos player handle.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_resume(cron_plyr_t* player);
/** \brief Pauses the playback fo given time.
 * Pauses running playback for defined time in milliseconds.
 * After the timeout passes, the playback is automatically resumed.
 * @param[in] player Chronos player handle.
 * @param[in] millis Pause timeout in milliseconds.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_pause_resume(cron_plyr_t* player, uint32_t millis);
/** \brief Resets the player.
 * This is "safety net" function, called when to many playback errors happened.
 * During this call, player should try to reset internal state and recover from the errors.
 * There is no guaranty that this call will successfully recover the player.
 * @param[in] player Chronos player handle.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_rst(cron_plyr_t* player);
/** \brief Forces the A/V sync.
 * When If there is enough data so that A/V sync can happen,
 * this function may be called to force A/V sync.
 * @param[in] player Chronos player handle.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_force_sync(cron_plyr_t* player);

/********************************************************************
 *                                                                  *
 *                         Buffer control                           *
 *                                                                  *
 ********************************************************************/

/** \brief Flushes \b ALL buffers.
 * During playback transitions, and in other occasions,
 * it is very important that all playback buffers are flushed. This function will
 * do exactly that, leaving no traces of previous data in the playback pipeline.
 * @param[in] player Chronos player handle.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_flush(cron_plyr_t* player);
/** \brief Gets data buffer.
 * This function is allocating a buffer from the player.
 * Retrieved buffer is automatically "locked", and it is expected that only
 * the caller is accessing it.
 * @see cron_plyr_buff_cons()
 * @param[in] player Chronos player handle.
 * @param[in,out] buff Pointer to the buffer. Will be set to `NULL` if function fails.
 * @param[in,out] size Requested size. On the function exit, this parameter
 * will be set to the actual size of the buffer.
 * @param[in] millis Timeout in which this call must return. If set to -1,
 * the call will wait for the buffer indefinitely.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_buff_get(cron_plyr_t* player, uint8_t** buff,
		size_t* size, link_data_ext_info_t* extended_info, int32_t millis, uint16_t id);
/** @brief Gives the buffer back to the player for data consumption.
 * When data is ready and allocated buffer is fully filled, this function
 * must be called so that the player can process it. Buffer is automatically "unlocked"
 * and ready for processing (it will not be touch outside of the Chronos player).
 * @see cron_plyr_buff_get()
 * @param[in] player Chronos player handle.
 * @param[in] buff Pointer to the buffer allocated with `cron_plyr_buff_get`.
 * @param[in] size Buffer size.
 * @param[in] millis Timeout in which this call must return. If set to -1,
 * the call will wait for the buffer to be consumed indefinitely.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_buff_cons(cron_plyr_t* player, uint8_t* buff,
		size_t size, link_data_ext_info_t* extended_info, int32_t millis, uint16_t id);
/********************************************************************
 *                                                                  *
 *                     A/V setup and control                        *
 *                                                                  *
 ********************************************************************/

/** @brief Blanks video output.
 * Call to this function sets video output to the black color.
 * @param[in] player Chronos player handle.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_vid_blank(cron_plyr_t* player);
/** @brief Positions video plane.
 * To have video on certain position on the screen, this function must be called.
 * If playback is still not started, video will be moved after the playback starts.
 * @param[in] player Chronos player handle.
 * @param[in] x Absolute position on X axis.
 * @param[in] y Absolute position on Y axis.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_vid_move(cron_plyr_t* player, uint16_t x, uint16_t y);
/** @brief Resizes video plane.
 * This function resizes video to passed size. If playback is still not started,
 * video will be scaled after the playback starts.
 * @param[in] player Chronos player handle.
 * @param[in] width Target width.
 * @param[in] height Target height.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_vid_scale(cron_plyr_t* player, uint16_t width,
		uint16_t height);
/** @brief Sets audio pass trough.
 * Enables or disables audio pass trough.
 * @param[in] player Chronos player handle. If <code>NULL<\code>, global PT should be set (if supported).
 * @param[in] enable `true` to enable, or `false` to disable pass trough.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_aud_pt(cron_plyr_t* player, bool enable);
/** @brief Sets volume leveling.
 * Enables or disables volume leveling with specific mode.
 * @param[in] player Chronos player handle.
 * @param[in] enable `true` to enable, or `false` to disable volume leveling.
 * @param[in] lvl Leveling mode.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_vol_leveling(cron_plyr_t* player, bool enable, cron_vol_lvl_t lvl);
/** @brief Gives specific features available for this player.
 * As the sink implementation can vary from platform to platform, some specific
 * features may be announced by the player to EOS via <code>>cron_plyr_sys_get_caps<\code>.
 * These features are requested by upper layers via this function.
 * Check "lynx.h" for the list of available features.
 * @see cron_plyr_sys_get_caps
 * @see lynx.h
 * @param[in] player Chronos player handle.
 * @param[in] cap Functionality requested.
 * @param ctrl_funcs Pointer to the set of callbacks related to the requested functionality.
 * @return EOS_ERROR_OK on success or proper error code.
 */
eos_error_t cron_plyr_get_ext(cron_plyr_t* player, link_cap_t cap,
		void** ctrl_funcs);

#endif /* CRON_PLYR_H_ */
