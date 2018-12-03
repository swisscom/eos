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


#include "com_swisscom_eos_EosNativeBridge.h"
#include "util_log.h"
#include "eos_macro.h"
#include "osi_memory.h"
#include "osi_time.h"

#include "eos.h"

#include <stdio.h>
#include <limits.h>

#define MODULE_NAME ("jni")

#define PACKAGE                  "com/swisscom/eos"
#define NATIVE_BRIDGE_FUNC(func) Java_com_swisscom_eos_EosNativeBridge_##func

#define CLASS_EOS_OUT          (PACKAGE"/out/EosOut")
#define CLASS_EOS_OUT_FACT     (PACKAGE"/out/EosOutFactory")
#define CLASS_EOS_OUT_TGT      ( \
		PACKAGE"/out/EosOutFactory$EosOutTarget")
#define CLASS_NATIVE_BRIDGE    (PACKAGE"/EosNativeBridge")
#define CLASS_EOS_TIME         (PACKAGE"/EosTime")
#define CLASS_MEDIA_DESC       (PACKAGE"/media/EosMediaDesc")
#define CLASS_MEDIA_CONT       (PACKAGE"/media/EosMediaContainer")
#define CLASS_MEDIA_DRM        (PACKAGE"/media/EosDRMType")
#define CLASS_MEDIA_TRCK       (PACKAGE"/media/EosMediaTrack")
#define CLASS_MEDIA_TRCK_AUD   (PACKAGE"/media/EosMediaAudioTrack")
#define CLASS_MEDIA_TRCK_VID   (PACKAGE"/media/EosMediaVideoTrack")
#define CLASS_MEDIA_TRCK_TTXT  (PACKAGE"/media/EosMediaTeletextTrack")
#define CLASS_MEDIA_TRCK_SUB   (PACKAGE"/media/EosMediaSubtitleTrack")
#define CLASS_MEDIA_CODEC      (PACKAGE"/media/EosMediaCodec")
#define CLASS_MEDIA_TTXT_PAGE  (PACKAGE"/media/EosMediaTeletextPage")
#define CLASS_MEDIA_TTXT_PTYPE (PACKAGE"/media/EosMediaTeletextPageType")
#define CLASS_EXCEPTION        (PACKAGE"/event/EosException")
#define CLASS_PLYR_STATE       (PACKAGE"/event/EosPlayerState")
#define CLASS_PLAY_INFO        (PACKAGE"/event/EosPlayInfo")
#define CLASS_PBK_STATUS       (PACKAGE"/event/EosPlaybackStatus")
#define CLASS_CONN_STATE_CHNG  (\
		PACKAGE"/event/EosConnectionStateChange")
#define CLASS_CONN_STATE       (PACKAGE"/event/EosConnectionState")
#define CLASS_CONN_CHNG_REASON (\
		PACKAGE"/event/EosConnectionChangeReason")
#define CLASS_ERR_ENUM         (PACKAGE"/event/EosError")
#define CLASS_DATA_KIND_ENUM   (PACKAGE"/data/EosDataKind")
#define CLASS_DATA_FORMAT_ENUM (PACKAGE"/data/EosDataFormat")

#define SIGN_EOS_OUT_FACT_GET_OUT ( \
		"(L"PACKAGE"/out/EosOutFactory$EosOutTarget;)"\
		"L"PACKAGE"/out/EosOut;")
#define SIGN_EOS_OUT_TGT         \
		("L"PACKAGE"/out/EosOutFactory$EosOutTarget;")
#define SIGN_MEDIA_DESC_CTOR      ( \
		"(L"PACKAGE"/media/EosMediaContainer;" \
		"L"PACKAGE"/media/EosDRMType;)V")
#define SIGN_MEDIA_DESC_ADD_TRCK  ("(L"PACKAGE"/media/EosMediaTrack;)V")
#define SIGN_MEDIA_CONT           ("L"PACKAGE"/media/EosMediaContainer;")
#define SIGN_MEDIA_DRM            ("L"PACKAGE"/media/EosDRMType;")
#define SIGN_MEDIA_TRCK_CTOR      ("(IZL"PACKAGE"/media/EosMediaCodec;)V")
#define SIGN_MEDIA_TRCK_AUD_CTOR  ("(IZL"PACKAGE"/media/EosMediaCodec;" \
		"Ljava/lang/String;II)V")
#define SIGN_MEDIA_TRCK_VID_CTOR  ("(IZL"PACKAGE"/media/EosMediaCodec;III)V")
#define SIGN_MEDIA_TRCK_TTXT_CTOR ("(IZ)V")
#define SIGN_MEDIA_TRCK_TTXT_ADDP ("(L"PACKAGE"/media/EosMediaTeletextPage;)V")
#define SIGN_MEDIA_TRCK_SUB_CTOR  ("(IZLjava/lang/String;)V")
#define SIGN_MEDIA_CODEC          ("L"PACKAGE"/media/EosMediaCodec;")
#define SIGN_MEDIA_TTXT_PTYPE     ( \
		"L"PACKAGE"/media/EosMediaTeletextPageType;")
#define SIGN_MEDIA_TTXT_PAGE_CTOR ( \
		"(L"PACKAGE"/media/EosMediaTeletextPageType;"\
		"Ljava/lang/String;I)V")
#define SIGN_PLYR_STATE           ("L"PACKAGE"/event/EosPlayerState;")
#define SIGN_PLAY_INFO_CTOR       ("(L"PACKAGE"/EosTime;" \
		"L"PACKAGE"/EosTime;L"PACKAGE"/EosTime;I)V")
#define SIGN_PBK_STATUS_ENUM      ("L"PACKAGE"/event/EosPlaybackStatus;")
#define SIGN_CONN_CHANGE_CTOR     ( \
		"(L"PACKAGE"/event/EosConnectionState;" \
		"L"PACKAGE"/event/EosConnectionChangeReason;)V")
#define SIGN_CONN_STATE_ENUM      ("L"PACKAGE"/event/EosConnectionState;")
#define SIGN_CONN_CHNG_REASON     (\
		"L"PACKAGE"/event/EosConnectionChangeReason;")
#define SIGN_FROM_SECONDS         ("(I)L"PACKAGE"/EosTime;")
#define SIGN_NATIVE_BRIDGE_SNGL   ("()L"PACKAGE"/EosNativeBridge;")
#define SIGN_FIRE_STATE           ("(L"PACKAGE"/out/EosOut;" \
		"L"PACKAGE"/event/EosPlayerState;)V")
#define SIGN_FIRE_PLAY_INFO       ("(L"PACKAGE"/out/EosOut;" \
		"L"PACKAGE"/event/EosPlayInfo;)V")
#define SIGN_FIRE_PBK_STATUS      ("(L"PACKAGE"/out/EosOut;" \
		"L"PACKAGE"/event/EosPlaybackStatus;)V")
#define SIGN_FIRE_CONN_CHANGE     ("(L"PACKAGE"/out/EosOut;" \
		"L"PACKAGE"/event/EosConnectionStateChange;)V")
#define SIGN_FIRE_ERROR           ("(L"PACKAGE"/out/EosOut;" \
		"L"PACKAGE"/event/EosConnectionStateChange;)V")
#define SIGN_FIRE_DATA            ("(L"PACKAGE"/out/EosOut;" \
		"L"PACKAGE"/data/EosDataKind;" \
		"L"PACKAGE"/data/EosDataFormat;Ljava/lang/String;)V")
#define SIGN_ERR_ENUM             ("L"PACKAGE"/event/EosError;")
#define SIGN_EXC_CTOR             ("(L"PACKAGE"/event/EosError;)V")
#define SIGN_DATA_KIND            ("L"PACKAGE"/data/EosDataKind;")
#define SIGN_DATA_FORMAT          ("L"PACKAGE"/data/EosDataFormat;")

static void cache_classes(JNIEnv* jEnv, jobject jThis);
static eos_out_t conv_out_from_java(JNIEnv* jEnv, jobject jOut);
static jobject conv_out_to_java(JNIEnv* jEnv, eos_out_t out);
static jobject conv_track_to_java(JNIEnv* jEnv, eos_media_es_t* es);
static jobject conv_track_ttxt_to_java(JNIEnv* jEnv, eos_media_es_t* es);
static jobject conv_state_to_java(JNIEnv* jEnv, eos_state_t state);
static jobject conv_play_info_to_java(JNIEnv* jEnv,
		eos_play_info_event_t* play_info);
static jobject conv_pbk_status_to_java(JNIEnv* jEnv,
		eos_pbk_status_event_t pbk_status);
static jobject conv_conn_state_to_java(JNIEnv* jEnv,
		eos_conn_state_event_t* conn_state);
static jobject conv_secs_to_java(JNIEnv* jEnv, uint64_t secs);
static jobject conv_err_to_java(JNIEnv* jEnv, eos_error_t err);
static void throw_eos_exception(JNIEnv* jEnv, eos_error_t err);
static eos_error_t eos_callback(eos_out_t out, eos_event_t event,
		eos_event_data_t* data, void* cookie);
static eos_error_t eos_data_callback(eos_out_t out, eos_data_cls_t clazz,
		eos_data_fmt_t format, eos_data_t data, uint32_t size, void* cookie);
static bool is_valid_utf_bytes(const uint8_t* bytes);

static JavaVM *cached_jvm = NULL;
static util_log_t *log = NULL;
static jobject jObjSingleton = NULL;
static jclass jClazzaNativeBridge = NULL;
static jclass jClazzOutFact = NULL;
static jclass jClazzOutTgt = NULL;
static jclass jClazzEosTime = NULL;
static jclass jClazzEosPlayInfo = NULL;
static jclass jClazzEosPbkStatus = NULL;
static jclass jClazzConnStateChange = NULL;
static jclass jClazzConnState = NULL;
static jclass jClazzConnChangeReason = NULL;
static jclass jClazzState = NULL;
static jclass jClazzErr = NULL;
static jclass jClazzDataKind = NULL;
static jclass jClazzDataFormat = NULL;

jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved)
{
	JNIEnv* env = NULL;
	cached_jvm = vm;

	EOS_UNUSED(reserved);
	if(util_log_create(&log, "eos") != EOS_ERROR_OK)
	{
		return 0;
	}
	util_log_set_level(log, UTIL_LOG_LEVEL_INFO | UTIL_LOG_LEVEL_WARN |
			UTIL_LOG_LEVEL_ERR);
	util_log_set_color(log, 1);
	UTIL_LOGI(log, "Loading JNI binding...");
	if((*vm)->GetEnv(vm, (void**)&env, JNI_VERSION_1_4) != JNI_OK)
	{
		return 0;
	}

	return JNI_VERSION_1_4;
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(init)(JNIEnv* jEnv, jobject jThis)
{
	UTIL_LOGI(log, "Init...");
	eos_init();
	eos_set_event_cbk(eos_callback, NULL);
	eos_data_cbk_set(eos_data_callback, NULL);
	cache_classes(jEnv, jThis);
	UTIL_LOGI(log, "Done");
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(deinit)(JNIEnv* jEnv, jobject jThis)
{
	EOS_UNUSED(jEnv);
	EOS_UNUSED(jThis);

	UTIL_LOGI(log, "Deinit...");
	eos_deinit();
	UTIL_LOGI(log, "Done");
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(start)(JNIEnv* jEnv,
		jobject jThis, jobject jOut, jstring jInUrl, jstring jInExtras)
{
	const char *url = NULL;
	const char *ext = NULL;
	eos_error_t err = EOS_ERROR_OK;
	eos_out_t out = conv_out_from_java(jEnv, jOut);

	EOS_UNUSED(jThis);
	UTIL_LOGI(log, "Start");
	url = (*jEnv)->GetStringUTFChars(jEnv, jInUrl, 0);
	UTIL_LOGI(log, "Start %s", url);
	if(jInExtras != NULL)
	{
		ext = (*jEnv)->GetStringUTFChars(jEnv, jInExtras, 0);
	}
	err = eos_player_play((char*)url, (char*)ext, out);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
	UTIL_LOGI(log, "Done");
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(startBuffering)
		(JNIEnv* jEnv, jobject jThis, jobject jOut)
{
	eos_error_t err = EOS_ERROR_OK;
	eos_out_t out = conv_out_from_java(jEnv, jOut);

	EOS_UNUSED(jThis);
	UTIL_LOGI(log, "Start buffering");
	err = eos_player_buffer(out, true);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
	UTIL_LOGI(log, "Done");
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(stopBuffering)
		(JNIEnv* jEnv, jobject jThis, jobject jOut)
{
	eos_error_t err = EOS_ERROR_OK;
	eos_out_t out = conv_out_from_java(jEnv, jOut);

	EOS_UNUSED(jThis);
	UTIL_LOGI(log, "Stop buffering");
	err = eos_player_buffer(out, false);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
	UTIL_LOGI(log, "Done");
}

JNIEXPORT void JNICALL
NATIVE_BRIDGE_FUNC(trickPlay)(JNIEnv* jEnv, jobject jThis,
		jobject jOut, jlong jOffset, jshort jSpeed)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;

	EOS_UNUSED(jThis);
	UTIL_LOGI(log, "Trick play");
	err = eos_player_trickplay(out, jOffset, jSpeed);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
	UTIL_LOGI(log, "Done");
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(stop)(JNIEnv* jEnv,
		jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;

	EOS_UNUSED(jThis);
	UTIL_LOGI(log, "Stop");
	err = eos_player_stop(out);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
	UTIL_LOGI(log, "Done");
}

JNIEXPORT jobject JNICALL NATIVE_BRIDGE_FUNC(getMediaDesc)
		(JNIEnv* jEnv, jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;
	eos_media_desc_t *desc = NULL;
	jclass jClazzDesc = NULL;
	jmethodID jDescCtor = NULL;
	jobject jDesc = NULL;
	jmethodID jMethodAddTrack = NULL;

	jclass jClazzContainer = NULL;
	jfieldID jFieldContainer = NULL;
	jobject jObjectContainer = NULL;
	jclass jClazzDrm = NULL;
	jfieldID jFieldDrm = NULL;
	jobject jObjectDrm = NULL;
	uint8_t i = 0;

	EOS_UNUSED(jThis);
	if((desc = osi_malloc(sizeof(eos_media_desc_t))) == NULL)
	{
		throw_eos_exception(jEnv, EOS_ERROR_NOMEM);

		return NULL;
	}
	err = eos_player_get_media_desc(out, desc);
	if(err != EOS_ERROR_OK)
	{
		osi_free((void**)&desc);
		throw_eos_exception(jEnv, err);

		return NULL;
	}
	jClazzDesc = (*jEnv)->FindClass(jEnv, CLASS_MEDIA_DESC);
	jDescCtor = (*jEnv)->GetMethodID(jEnv, jClazzDesc,
			"<init>", SIGN_MEDIA_DESC_CTOR);
	jMethodAddTrack = (*jEnv)->GetMethodID(jEnv, jClazzDesc,
			"addTrack", SIGN_MEDIA_DESC_ADD_TRCK);
	jClazzContainer =  (*jEnv)->FindClass(jEnv, CLASS_MEDIA_CONT);
	switch(desc->container)
	{
	case EOS_MEDIA_CONT_MPEGTS:
		jFieldContainer = (*jEnv)->GetStaticFieldID(jEnv, jClazzContainer,
					"MPEG_TS", SIGN_MEDIA_CONT);
		break;
	case EOS_MEDIA_CONT_NONE:
	default:
		jFieldContainer = (*jEnv)->GetStaticFieldID(jEnv, jClazzContainer,
					"NONE", SIGN_MEDIA_CONT);
	}
	jObjectContainer = 	(*jEnv)->GetStaticObjectField(jEnv, jClazzContainer,
			jFieldContainer);
	jClazzDrm =  (*jEnv)->FindClass(jEnv, CLASS_MEDIA_DRM);
	jFieldDrm = (*jEnv)->GetStaticFieldID(jEnv, jClazzDrm,
				"NONE", SIGN_MEDIA_DRM);
	jObjectDrm = (*jEnv)->GetStaticObjectField(jEnv, jClazzDrm, jFieldDrm);
	jDesc = (*jEnv)->NewObject(jEnv, jClazzDesc, jDescCtor, jObjectContainer,
			jObjectDrm);

	for(i=0; i<desc->es_cnt; i++)
	{
		(*jEnv)->CallVoidMethod(jEnv, jDesc, jMethodAddTrack,
				conv_track_to_java(jEnv, &desc->es[i]));
	}
	osi_free((void**)&desc);

	return jDesc;
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(selectTrack)
		(JNIEnv* jEnv, jobject jThis, jobject jOut, jint jID)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;

	UTIL_LOGI(log, "Set track");
	EOS_UNUSED(jThis);
	err = eos_player_set_track(out, jID, true);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
	UTIL_LOGI(log, "Set track done");
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(deselectTrack)
		(JNIEnv* jEnv, jobject jThis, jobject jOut, jint jID)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;

	EOS_UNUSED(jThis);
	UTIL_LOGI(log, "Unset track");
	err = eos_player_set_track(out, jID, false);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
	UTIL_LOGI(log, "Unset track done");
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(setTtxtEnabled)
		(JNIEnv* jEnv, jobject jThis, jobject jOut, jboolean jEnable)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;

	EOS_UNUSED(jThis);
	err = eos_data_ttxt_enable(out, jEnable);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
}

JNIEXPORT jstring JNICALL NATIVE_BRIDGE_FUNC(getTtxtPage)
		(JNIEnv* jEnv, jobject jThis, jobject jOut, jint jPage)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;
	char *page = NULL;
	jstring ret = NULL;

	EOS_UNUSED(jThis);
	err = eos_player_get_ttxt_pg(out, jPage, &page);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
		return NULL;
	}
	ret = (*jEnv)->NewStringUTF(jEnv, page);
	osi_free((void**)&page);

	return ret;
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(setTtxtPage)
		(JNIEnv* jEnv, jobject jThis, jobject jOut, jint jPage, jint jSubPage)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;
	EOS_UNUSED(jThis);

	err = eos_data_ttxt_page_set(out, jPage, jSubPage);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
}

JNIEXPORT jint JNICALL NATIVE_BRIDGE_FUNC(getTtxtPageNumber)
		(JNIEnv* jEnv, jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	uint16_t page;
	uint16_t sub_page;
	EOS_UNUSED(jThis);

	if(eos_data_ttxt_page_get(out, &page, &sub_page) != EOS_ERROR_OK)
	{
		return 0;
	}

	return page;
}

JNIEXPORT jint JNICALL
NATIVE_BRIDGE_FUNC(getTtxtSubPageNumber)(JNIEnv* jEnv,
		jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	uint16_t page;
	uint16_t sub_page;
	EOS_UNUSED(jThis);

	if(eos_data_ttxt_page_get(out, &page, &sub_page) != EOS_ERROR_OK)
	{
		return 0;
	}

	return sub_page;
}

JNIEXPORT jint JNICALL NATIVE_BRIDGE_FUNC(getTtxtNextPage)
		(JNIEnv* jEnv, jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	uint16_t page;
	EOS_UNUSED(jThis);

	if(eos_data_ttxt_next_page_get(out, &page) != EOS_ERROR_OK)
	{
		return 0;
	}

	return page;
}

JNIEXPORT jint JNICALL
NATIVE_BRIDGE_FUNC(getTtxtNextSubPageNumber)(JNIEnv* jEnv,
		jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	uint16_t next;
	EOS_UNUSED(jThis);

	if(eos_data_ttxt_next_subpage_get(out, &next) != EOS_ERROR_OK)
	{
		return 0;
	}

	return next;
}

JNIEXPORT jint JNICALL NATIVE_BRIDGE_FUNC(getTtxtPreviousPage)
		(JNIEnv* jEnv, jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	uint16_t page;
	EOS_UNUSED(jThis);

	if(eos_data_ttxt_prev_page_get(out, &page) != EOS_ERROR_OK)
	{
		return 0;
	}

	return page;
}

JNIEXPORT jint JNICALL NATIVE_BRIDGE_FUNC(getTtxtRedPage)
  (JNIEnv* jEnv, jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	uint16_t page;
	EOS_UNUSED(jThis);

	if(eos_data_ttxt_red_page_get(out, &page) != EOS_ERROR_OK)
	{
		return 0;
	}

	return page;
}

JNIEXPORT jint JNICALL NATIVE_BRIDGE_FUNC(getTtxtGreenPage)
  (JNIEnv* jEnv, jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	uint16_t page;
	EOS_UNUSED(jThis);

	if(eos_data_ttxt_green_page_get(out, &page) != EOS_ERROR_OK)
	{
		return 0;
	}

	return page;
}


JNIEXPORT jint JNICALL
NATIVE_BRIDGE_FUNC(getTtxtPeviousSubPageNumber)(JNIEnv* jEnv,
		jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	uint16_t prev;
	EOS_UNUSED(jThis);

	if(eos_data_ttxt_prev_subpage_get(out, &prev) != EOS_ERROR_OK)
	{
		return 0;
	}

	return prev;
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(setTtxtTransparency)
  (JNIEnv* jEnv, jobject jThis, jobject jOut, jint jAlpha)
{
	eos_error_t err;
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	EOS_UNUSED(jThis);

	err = eos_data_ttxt_transparency_set(out, jAlpha);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}

}

JNIEXPORT jint JNICALL NATIVE_BRIDGE_FUNC(getTtxtBluePage)
		(JNIEnv* jEnv, jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	uint16_t blue;
	EOS_UNUSED(jThis);

	if(eos_data_ttxt_blue_page_get(out, &blue) != EOS_ERROR_OK)
	{
		return 0;
	}

	return blue;
}

JNIEXPORT jint JNICALL NATIVE_BRIDGE_FUNC(getTtxtYellowPage)
		(JNIEnv* jEnv, jobject jThis, jobject jOut)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	uint16_t yellow;
	EOS_UNUSED(jThis);

	UTIL_GLOGD("Get yellow");
	if(eos_data_ttxt_yellow_page_get(out, &yellow) != EOS_ERROR_OK)
	{
		return 0;
	}

	UTIL_GLOGD("Yellow %d", yellow);
	return yellow;
}

JNIEXPORT void JNICALL
NATIVE_BRIDGE_FUNC(setDvbSubtitlesEnabled)(JNIEnv* jEnv,
		jobject jThis, jobject jOut, jboolean jEnable)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;

	EOS_UNUSED(jThis);
	UTIL_GLOGD("setDvbSubtitlesEnabled %d", jEnable);
	err = eos_data_dvbsub_enable(out, jEnable);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(setupVideo)
		(JNIEnv* jEnv, jclass  clazz, jobject jOut, jint x, jint y, jint w, jint h)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;

	EOS_UNUSED(clazz);
	err = eos_out_vid_scale(out, (uint32_t)w, (uint32_t)h);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);

		return;
	}
	err = eos_out_vid_move(out, (uint32_t)x, (uint32_t)y);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(setAudioPassThrough)
		(JNIEnv* jEnv, jclass  clazz, jobject jOut, jboolean enable)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;
	eos_out_amode_t amode = enable ?  EOS_OUT_AMODE_PT : EOS_OUT_AMODE_STEREO;

	EOS_UNUSED(clazz);
	err = eos_out_aud_mode(out, amode);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
}

JNIEXPORT void JNICALL NATIVE_BRIDGE_FUNC(setVolumeLeveling)
		(JNIEnv* jEnv, jclass  clazz, jobject jOut, jboolean enable, jint level)
{
	eos_out_t out = conv_out_from_java(jEnv, jOut);
	eos_error_t err = EOS_ERROR_OK;
	eos_out_vol_lvl_t lvl = level;

	EOS_UNUSED(clazz);
	err = eos_out_vol_leveling(out, (bool)enable, lvl);
	if(err != EOS_ERROR_OK)
	{
		throw_eos_exception(jEnv, err);
	}
}

JNIEXPORT jstring JNICALL NATIVE_BRIDGE_FUNC(getVersion)
  (JNIEnv* jEnv, jclass  clazz)
{
	EOS_UNUSED(clazz);
	return (*jEnv)->NewStringUTF(jEnv, eos_get_ver_str());
}

static void cache_classes(JNIEnv* jEnv, jobject jThis)
{
	jclass clazz;

	jObjSingleton = (*jEnv)->NewGlobalRef(jEnv, jThis);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_NATIVE_BRIDGE);
	jClazzaNativeBridge = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_EOS_OUT_FACT);
	jClazzOutFact = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_EOS_OUT_TGT);
	jClazzOutTgt = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_EOS_TIME);
	jClazzEosTime = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_PLAY_INFO);
	jClazzEosPlayInfo = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_PBK_STATUS);
	jClazzEosPbkStatus = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_CONN_STATE_CHNG);
	jClazzConnStateChange = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_CONN_STATE);
	jClazzConnState = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_CONN_CHNG_REASON);
	jClazzConnChangeReason = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_PLYR_STATE);
	jClazzState = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_ERR_ENUM);
	jClazzErr = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_DATA_KIND_ENUM);
	jClazzDataKind = (*jEnv)->NewGlobalRef(jEnv, clazz);
	clazz = (*jEnv)->FindClass(jEnv, CLASS_DATA_FORMAT_ENUM);
	jClazzDataFormat = (*jEnv)->NewGlobalRef(jEnv, clazz);
}

static eos_error_t eos_callback(eos_out_t out, eos_event_t event,
		eos_event_data_t* data, void* cookie)
{
	JNIEnv *jEnv = NULL;
	jobject jOut = NULL;
	jobject jEvent = NULL;
	jmethodID jMethodFire = NULL;

	EOS_UNUSED(cookie);
	UTIL_LOGD(log, "Callback received: %d", event);
#ifdef ANDROID
	if(((*cached_jvm)->AttachCurrentThread(cached_jvm, &jEnv, NULL)) != 0)
	{
		return EOS_ERROR_INVAL;
	}
#else
	if(((*cached_jvm)->AttachCurrentThread(cached_jvm, (void**)&jEnv, NULL)) != 0)
	{
		return EOS_ERROR_INVAL;
	}
#endif
	jOut = conv_out_to_java(jEnv, out);
	switch(event)
	{
	case EOS_EVENT_STATE:
		jEvent = conv_state_to_java(jEnv, data->state.state);
		jMethodFire = (*jEnv)->GetMethodID(jEnv, jClazzaNativeBridge,
				"fireStateChange", SIGN_FIRE_STATE);
		break;
	case EOS_EVENT_PLAY_INFO:
		jEvent = conv_play_info_to_java(jEnv, &data->play_info);
		jMethodFire = (*jEnv)->GetMethodID(jEnv, jClazzaNativeBridge,
				"firePlayInfo", SIGN_FIRE_PLAY_INFO);
		break;
	case EOS_EVENT_PBK_STATUS:
		jEvent = conv_pbk_status_to_java(jEnv, data->pbk_status);
		jMethodFire = (*jEnv)->GetMethodID(jEnv, jClazzaNativeBridge,
				"firePlaybackStatus", SIGN_FIRE_PBK_STATUS);
		break;
	case EOS_EVENT_CONN_STATE:
		jEvent = conv_conn_state_to_java(jEnv, &data->conn);
		jMethodFire = (*jEnv)->GetMethodID(jEnv, jClazzaNativeBridge,
				"fireConnectionChange", SIGN_FIRE_CONN_CHANGE);
		break;
	case EOS_EVENT_ERR:
		jEvent = conv_err_to_java(jEnv, data->err.err_code);
		jMethodFire = (*jEnv)->GetMethodID(jEnv, jClazzaNativeBridge,
				"fireError", SIGN_FIRE_ERROR);
		break;
	default:
		break;
	}
	if(jEvent !=NULL && jMethodFire != NULL)
	{
		(*jEnv)->CallVoidMethod(jEnv, jObjSingleton, jMethodFire,
				jOut, jEvent);
	}
	(*cached_jvm)->DetachCurrentThread(cached_jvm);

	return EOS_ERROR_OK;
}

static eos_error_t eos_data_callback(eos_out_t out, eos_data_cls_t clazz,
		eos_data_fmt_t format, eos_data_t data, uint32_t size, void* cookie)
{
	JNIEnv *jEnv = NULL;
	jobject jOut = NULL;
	jobject jKind = NULL;
	jobject jFormat = NULL;
	jmethodID jMethodFire = NULL;
	jfieldID jFieldEnum = NULL;

	EOS_UNUSED(size);
	EOS_UNUSED(cookie);

	if(data == NULL)
	{
		UTIL_LOGI(log, "Data is NULL. Ignoring it!");
		return EOS_ERROR_OK;
	}
#ifdef ANDROID
	if(((*cached_jvm)->AttachCurrentThread(cached_jvm, &jEnv, NULL))
			!= 0)
	{
		return EOS_ERROR_INVAL;
	}
#else
	if(((*cached_jvm)->AttachCurrentThread(cached_jvm, (void**)&jEnv, NULL))
			!= 0)
	{
		return EOS_ERROR_INVAL;
	}
#endif
	jOut = conv_out_to_java(jEnv, out);
	jMethodFire = (*jEnv)->GetMethodID(jEnv, jClazzaNativeBridge,
			"fireData", SIGN_FIRE_DATA);
	UTIL_GLOGD("Data handler: %d %d", clazz, format);
	switch(clazz)
	{
	case EOS_DATA_CLS_TTXT:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzDataKind,
				"TTXT", SIGN_DATA_KIND);
		break;
	case EOS_DATA_CLS_SUB:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzDataKind,
				"SUB", SIGN_DATA_KIND);
		break;
	case EOS_DATA_CLS_HBBTV:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzDataKind,
				"HBBTV", SIGN_DATA_KIND);
		break;
	case EOS_DATA_CLS_DSM_CC:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzDataKind,
				"DSMCC", SIGN_DATA_KIND);
		break;
	default:
		break;
	}
	if(jFieldEnum != NULL)
	{
		jKind = (*jEnv)->GetStaticObjectField(jEnv, jClazzDataKind,
				jFieldEnum);
	}
	switch(format)
	{
	case EOS_DATA_FMT_RAW:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzDataFormat,
				"RAW", SIGN_DATA_FORMAT);
		break;
	case EOS_DATA_FMT_JSON:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzDataFormat,
				"JSON", SIGN_DATA_FORMAT);
		break;
	case EOS_DATA_FMT_HTML:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzDataFormat,
				"HTML", SIGN_DATA_FORMAT);
		break;
	case EOS_DATA_FMT_BASE64_PNG:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzDataFormat,
				"BASE64_PNG", SIGN_DATA_FORMAT);
		break;
	default:
		break;
	}
	if(jFieldEnum != NULL)
	{
		jFormat = (*jEnv)->GetStaticObjectField(jEnv, jClazzDataFormat,
				jFieldEnum);
	}
	if(jMethodFire != NULL && jKind != NULL && jFormat != NULL)
	{
		if (is_valid_utf_bytes((uint8_t*)data))
		{
			(*jEnv)->CallVoidMethod(jEnv, jObjSingleton, jMethodFire,
					jOut, jKind, jFormat,
					(*jEnv)->NewStringUTF(jEnv, (char*) data));
		}
		else	
		{
			UTIL_LOGE(log, "Could not prepare for callback?! "
			"Not a UTF string: %s", (char*) data);
		}
	}
	else
	{
		UTIL_LOGE(log, "Could not prepare for callback?! "
				"jMethodFire %p jKind %p jFormat %p", jMethodFire, jKind,
				jFormat);
	}
	(*cached_jvm)->DetachCurrentThread(cached_jvm);

	return EOS_ERROR_OK;
}

static eos_out_t conv_out_from_java(JNIEnv* jEnv, jobject jOut)
{
	jclass jClazzOut;
	jmethodID jMethodGetID;
	jint jID;

	jClazzOut = (*jEnv)->FindClass(jEnv, CLASS_EOS_OUT);
	jMethodGetID = (*jEnv)->GetMethodID(jEnv, jClazzOut, "getID", "()I");
	jID = (*jEnv)->CallIntMethod(jEnv, jOut, jMethodGetID);
	if(jID == EOS_OUT_MAIN_AV)
	{
		return EOS_OUT_MAIN_AV;
	}
	if(jID == EOS_OUT_AUX_AV)
	{
		return EOS_OUT_AUX_AV;
	}
	UTIL_LOGW(log, "Unknown output (%d). Defaulting to MAIN");

	return EOS_OUT_MAIN_AV;
}

static jobject conv_out_to_java(JNIEnv* jEnv, eos_out_t out)
{

	jfieldID jFieldEnum = NULL;
	jobject jObjectEnum = NULL;
	jmethodID jMethodGetOut = NULL;

	if(out == EOS_OUT_AUX_AV)
	{
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzOutTgt,
				"AUX", SIGN_EOS_OUT_TGT);
	}
	else
	{
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzOutTgt,
				"MAIN", SIGN_EOS_OUT_TGT);
	}
	jObjectEnum = (*jEnv)->GetStaticObjectField(jEnv, jClazzOutTgt,
			jFieldEnum);
	jMethodGetOut = (*jEnv)->GetStaticMethodID(jEnv, jClazzOutFact,
			"getOut", SIGN_EOS_OUT_FACT_GET_OUT);

	return (*jEnv)->CallStaticObjectMethod(jEnv, jClazzOutFact, jMethodGetOut,
			jObjectEnum);
}

static jobject conv_state_to_java(JNIEnv* jEnv, eos_state_t state)
{
	jfieldID jFieldEnum = NULL;

	switch(state)
	{
	case EOS_STATE_STOPPED:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzState,
				"STOPPED", SIGN_PLYR_STATE);
		break;
	case EOS_STATE_PLAYING:
		UTIL_LOGW(log, "Converting EOS_STATE_PLAYING");
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzState,
				"PLAYING", SIGN_PLYR_STATE);
		break;
	case EOS_STATE_PAUSED:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzState,
				"PAUSED", SIGN_PLYR_STATE);
		break;
	case EOS_STATE_BUFFERING:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzState,
				"BUFFERING", SIGN_PLYR_STATE);
		break;
	case EOS_STATE_TRANSITIONING:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzState,
				"TRANSITIONING", SIGN_PLYR_STATE);
		break;
	default:
		return NULL;
	}

	return (*jEnv)->GetStaticObjectField(jEnv, jClazzState, jFieldEnum);
}

static jobject conv_play_info_to_java(JNIEnv* jEnv,
		eos_play_info_event_t* play_info)
{
	jmethodID jMethod = NULL;
	jobject jBegin = NULL;
	jobject jEnd = NULL;
	jobject jPosition = NULL;

	jBegin = conv_secs_to_java(jEnv, play_info->begin);
	jEnd = conv_secs_to_java(jEnv, play_info->end);
	jPosition = conv_secs_to_java(jEnv, play_info->position);
	jMethod = (*jEnv)->GetMethodID(jEnv, jClazzEosPlayInfo, "<init>",
			SIGN_PLAY_INFO_CTOR);

	return (*jEnv)->NewObject(jEnv, jClazzEosPlayInfo, jMethod,
			jBegin, jPosition, jEnd, play_info->speed);
}

static jobject conv_pbk_status_to_java(JNIEnv* jEnv,
		eos_pbk_status_event_t pbk_status)
{
	jfieldID jFieldEnum = NULL;

	switch(pbk_status)
	{
	case EOS_PBK_BOS:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzEosPbkStatus,
				"BEGIN_OF_STREAM", SIGN_PBK_STATUS_ENUM);
		break;
	case EOS_PBK_EOS:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzEosPbkStatus,
				"END_OF_STREAM", SIGN_PBK_STATUS_ENUM);
		break;
	case EOS_PBK_HIGH_WM:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzEosPbkStatus,
				"BUFF_HIGH_WATERMARK", SIGN_PBK_STATUS_ENUM);
		break;
	case EOS_PBK_LOW_WM:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzEosPbkStatus,
				"BUFF_LOW_WATERMARK", SIGN_PBK_STATUS_ENUM);
		break;
	default:
		UTIL_LOGE(log, "Unsupported PBK status: %d", pbk_status);

		return NULL;
	}

	return (*jEnv)->GetStaticObjectField(jEnv, jClazzEosPbkStatus, jFieldEnum);
}

static jobject conv_conn_state_to_java(JNIEnv* jEnv,
		eos_conn_state_event_t* conn_state)
{
	jfieldID jFieldEnum = NULL;
	jobject jState = NULL;
	jobject jReason = NULL;
	jmethodID jMethod = NULL;

	switch(conn_state->state)
	{
	case EOS_CONNECTED:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzConnState,
				"CONNECTED", SIGN_CONN_STATE_ENUM);
		break;
	case EOS_DISCONNECTED:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzConnState,
				"DISCONNECTED", SIGN_CONN_STATE_ENUM);
		break;
	default:
		UTIL_LOGE(log, "Unsupported connection state: %d", conn_state->state);
		return NULL;
	}
	jState = (*jEnv)->GetStaticObjectField(jEnv, jClazzConnState, jFieldEnum);
	if(jState == NULL)
	{
		UTIL_LOGE(log, "Connection state not found: %d", conn_state->state);

		return NULL;
	}

	switch(conn_state->reason)
	{
	case EOS_CONN_USER:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzConnChangeReason,
				"USER_REQUEST", SIGN_CONN_CHNG_REASON);
		break;
	case EOS_CONN_WR_ERR:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzConnChangeReason,
				"WRITE_ERROR", SIGN_CONN_CHNG_REASON);
		break;
	case EOS_CONN_RD_ERR:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzConnChangeReason,
				"READ_ERROR", SIGN_CONN_CHNG_REASON);
		break;
	case EOS_CONN_DRM_ERR:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzConnChangeReason,
				"DRM_ERROR", SIGN_CONN_CHNG_REASON);
		break;
	case EOS_CONN_SERVER_ERR:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzConnChangeReason,
				"SERVER_ERROR", SIGN_CONN_CHNG_REASON);
		break;
	default:
		UTIL_LOGE(log, "Unsupported connection change reason: %d",
				conn_state->reason);
		return NULL;
	}
	jReason = (*jEnv)->GetStaticObjectField(jEnv, jClazzConnChangeReason,
			jFieldEnum);
	if(jReason == NULL)
	{
		UTIL_LOGE(log, "Connection change reason not found: %d",
				conn_state->reason);

		return NULL;
	}
	jMethod = (*jEnv)->GetMethodID(jEnv, jClazzConnStateChange, "<init>",
				SIGN_CONN_CHANGE_CTOR);

	return (*jEnv)->NewObject(jEnv, jClazzConnStateChange, jMethod,
			jState, jReason);
}

static jobject conv_secs_to_java(JNIEnv* jEnv, uint64_t secs)
{
	jmethodID jMethod = NULL;

	jMethod = (*jEnv)->GetStaticMethodID(jEnv, jClazzEosTime,
			"fromSeconds", SIGN_FROM_SECONDS);
	if(secs > INT_MAX)
	{
		UTIL_LOGW(log, "We must limit value: %llu", secs);
		secs = INT_MAX;
	}

	return (*jEnv)->CallStaticObjectMethod(jEnv, jClazzEosTime, jMethod,
			(jint)secs);
}

static jobject conv_err_to_java(JNIEnv* jEnv, eos_error_t err)
{

	jfieldID jFieldEnum = NULL;

	switch(err)
	{
	case EOS_ERROR_GENERAL:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"GENERAL", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_INVAL:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"INVAL", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_NFOUND:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"NFOUND", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_NOMEM:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"NOMEM", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_BUSY:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"BUSY", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_PERM:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"PERM", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_TIMEDOUT:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"TIMEDOUT", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_EMPTY:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"EMPTY", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_EOF:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"EOF", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_EOL:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"EOL", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_BOL:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"BOL", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_OVERFLOW:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"OVERFLOW", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_NIMPLEMENTED:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"NIMPLEMENTED", SIGN_ERR_ENUM);
		break;
	case EOS_ERROR_FATAL:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"FATAL", SIGN_ERR_ENUM);
		break;
	case EOS_ERR_UNKNOWN:
		jFieldEnum = (*jEnv)->GetStaticFieldID(jEnv, jClazzErr,
				"UNKNOWN", SIGN_ERR_ENUM);
		break;
	default:
		return NULL;
	}

	return (*jEnv)->GetStaticObjectField(jEnv, jClazzErr, jFieldEnum);
}

static void throw_eos_exception(JNIEnv* jEnv, eos_error_t err)
{
	jclass jClazzExc = NULL;
	jmethodID jMethodExcCtr = NULL;
	jthrowable jThrow = NULL;
	jobject jObjectEnum = NULL;

	if(err == EOS_ERROR_OK)
	{
		return;
	}
	UTIL_LOGI(log, "Throwing exception for: %d", err);
	jClazzExc = (*jEnv)->FindClass(jEnv, CLASS_EXCEPTION);
	jMethodExcCtr = (*jEnv)->GetMethodID(jEnv, jClazzExc, "<init>",
			SIGN_EXC_CTOR);

	jObjectEnum = conv_err_to_java(jEnv, err);
	jThrow = (*jEnv)->NewObject(jEnv, jClazzExc, jMethodExcCtr, jObjectEnum);

	(*jEnv)->Throw(jEnv, jThrow);
}

static jobject conv_track_to_java(JNIEnv* jEnv, eos_media_es_t* es)
{
	jclass jClazzTrck = NULL;
	jmethodID jMethodTrckCtr = NULL;
	jclass jClazzCodec = NULL;
	jfieldID jFieldCodec = NULL;
	jobject jObjectCodec = NULL;

	jClazzTrck = (*jEnv)->FindClass(jEnv, CLASS_MEDIA_TRCK);
	jMethodTrckCtr = (*jEnv)->GetMethodID(jEnv, jClazzTrck, "<init>",
			SIGN_MEDIA_TRCK_CTOR);
	jClazzCodec = (*jEnv)->FindClass(jEnv, CLASS_MEDIA_CODEC);
	switch(es->codec)
	{
	case EOS_MEDIA_CODEC_MPEG1:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"MPEG1", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_MPEG2:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"MPEG2", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_MPEG4:	//H.263
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"MPEG4", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_H264:	//AVC
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"H264", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_H265:	//HEVC
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"H265", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_MP1:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"MP1", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_MP2:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"MP2", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_MP3:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"MP3", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_AAC:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"AAC", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_HEAAC:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"HEAAC", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_AC3:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"AC3", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_EAC3:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"EAC3", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_DTS:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"DTS", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_LPCM:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"LPCM", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_TTXT:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"TTXT", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_DVB_SUB:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"SUB", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_CC:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"CC", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_HBBTV:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"HBBTV", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_DSMCC_A:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"DSMCC_A", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_DSMCC_B:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"DSMCC_B", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_DSMCC_C:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"DSMCC_C", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_DSMCC_D:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"DSMCC_D", SIGN_MEDIA_CODEC);
		break;
	case EOS_MEDIA_CODEC_VMX:
	case EOS_MEDIA_CODEC_UNKNOWN:
	default:
		jFieldCodec = (*jEnv)->GetStaticFieldID(jEnv, jClazzCodec,
				"UNKNOWN", SIGN_MEDIA_CODEC);
		break;
	}
	jObjectCodec = (*jEnv)->GetStaticObjectField(jEnv, jClazzCodec,
			jFieldCodec);

	if(EOS_MEDIA_IS_AUD(es->codec))
	{
		jClazzTrck = (*jEnv)->FindClass(jEnv, CLASS_MEDIA_TRCK_AUD);
		jMethodTrckCtr = (*jEnv)->GetMethodID(jEnv, jClazzTrck, "<init>",
				SIGN_MEDIA_TRCK_AUD_CTOR);

		return (*jEnv)->NewObject(jEnv, jClazzTrck, jMethodTrckCtr,
				(jint)es->id, (jboolean)es->selected, jObjectCodec,
				(*jEnv)->NewStringUTF(jEnv, es->lang), es->atr.audio.rate,
				es->atr.audio.ch);
	}

	if(EOS_MEDIA_IS_VID(es->codec))
	{
		jClazzTrck = (*jEnv)->FindClass(jEnv, CLASS_MEDIA_TRCK_VID);
		jMethodTrckCtr = (*jEnv)->GetMethodID(jEnv, jClazzTrck, "<init>",
				SIGN_MEDIA_TRCK_VID_CTOR);

		return (*jEnv)->NewObject(jEnv, jClazzTrck, jMethodTrckCtr,
				(jint)es->id, (jboolean)es->selected, jObjectCodec,
				es->atr.video.fps, es->atr.video.resolution.width,
				es->atr.video.resolution.height);
	}

	if(es->codec == EOS_MEDIA_CODEC_DVB_SUB)
	{
		jClazzTrck = (*jEnv)->FindClass(jEnv, CLASS_MEDIA_TRCK_SUB);
		jMethodTrckCtr = (*jEnv)->GetMethodID(jEnv, jClazzTrck, "<init>",
				SIGN_MEDIA_TRCK_SUB_CTOR);

		return (*jEnv)->NewObject(jEnv, jClazzTrck, jMethodTrckCtr,
				(jint)es->id, (jboolean)es->selected,
				(*jEnv)->NewStringUTF(jEnv, es->lang));
	}

	if(es->codec == EOS_MEDIA_CODEC_TTXT)
	{
		return conv_track_ttxt_to_java(jEnv, es);
	}

	return (*jEnv)->NewObject(jEnv, jClazzTrck, jMethodTrckCtr, (jint)es->id,
			(jboolean)es->selected, jObjectCodec,
			(*jEnv)->NewStringUTF(jEnv, es->lang));
}

static jobject conv_track_ttxt_to_java(JNIEnv* jEnv, eos_media_es_t* es)
{
	jclass jClazzTrck = NULL;
	jmethodID jMethodTrckCtr = NULL;
	jobject jObjectTtxtTrack = NULL;
	jmethodID jMethodAddPage = NULL;
	jclass jClazzPage = NULL;
	jmethodID jMethodPageCtr = NULL;
	jobject jObjectPage = NULL;
	jclass jClazzPageType = NULL;
	jfieldID jFieldPageType = NULL;
	jobject jObjectPageType = NULL;
	int i = 0;

	jClazzTrck = (*jEnv)->FindClass(jEnv, CLASS_MEDIA_TRCK_TTXT);
	jMethodTrckCtr = (*jEnv)->GetMethodID(jEnv, jClazzTrck, "<init>",
			SIGN_MEDIA_TRCK_TTXT_CTOR);
	jMethodAddPage = (*jEnv)->GetMethodID(jEnv, jClazzTrck, "addPage",
			SIGN_MEDIA_TRCK_TTXT_ADDP);
	jObjectTtxtTrack = (*jEnv)->NewObject(jEnv, jClazzTrck, jMethodTrckCtr,
			(jint)es->id, (jboolean)es->selected);

	jClazzPage = (*jEnv)->FindClass(jEnv, CLASS_MEDIA_TTXT_PAGE);
	jMethodPageCtr = (*jEnv)->GetMethodID(jEnv, jClazzPage, "<init>",
			SIGN_MEDIA_TTXT_PAGE_CTOR);
	jClazzPageType = (*jEnv)->FindClass(jEnv, CLASS_MEDIA_TTXT_PTYPE);

	for(i=0; i<es->atr.ttxt.page_info_cnt; i++)
	{
		switch(es->atr.ttxt.page_infos[i].type)
		{
		case EOS_MEDIA_TTXT_INITIAL_PAGE:
			jFieldPageType = (*jEnv)->GetStaticFieldID(jEnv,
					jClazzPageType, "INITIAL_PAGE", SIGN_MEDIA_TTXT_PTYPE);
			break;
		case EOS_MEDIA_TTXT_SUB:
			jFieldPageType = (*jEnv)->GetStaticFieldID(jEnv,
					jClazzPageType, "SUB", SIGN_MEDIA_TTXT_PTYPE);
			break;
		case EOS_MEDIA_TTXT_INFO:
			jFieldPageType = (*jEnv)->GetStaticFieldID(jEnv,
					jClazzPageType, "INFO", SIGN_MEDIA_TTXT_PTYPE);
			break;
		case EOS_MEDIA_TTXT_SCHEDULE:
			jFieldPageType = (*jEnv)->GetStaticFieldID(jEnv,
					jClazzPageType, "SCHEDULE", SIGN_MEDIA_TTXT_PTYPE);
			break;
		case EOS_MEDIA_TTXT_CC:
			jFieldPageType = (*jEnv)->GetStaticFieldID(jEnv,
					jClazzPageType, "CC", SIGN_MEDIA_TTXT_PTYPE);
			break;
		}
		jObjectPageType = (*jEnv)->GetStaticObjectField(jEnv, jClazzPageType,
				jFieldPageType);
		jObjectPage = (*jEnv)->NewObject(jEnv, jClazzPage, jMethodPageCtr,
				jObjectPageType,
				(*jEnv)->NewStringUTF(jEnv,
						es->atr.ttxt.page_infos[i].lang),
				es->atr.ttxt.page_infos[i].page);
		(*jEnv)->CallVoidMethod(jEnv, jObjectTtxtTrack, jMethodAddPage,
				jObjectPage);
	}

	return jObjectTtxtTrack;
}

// function taken from art/runtime/check_jni.cc Android N
//                     dalvik/vm/CheckJni.cpp	Android JB
static bool is_valid_utf_bytes(const uint8_t* bytes) 
{
	while (*bytes != '\0') 
	{
		const uint8_t* utf8 = bytes++;
		// Switch on the high four bits.
		switch (*utf8 >> 4) 
		{
			case 0x00:
			case 0x01:
			case 0x02:
			case 0x03:
			case 0x04:
			case 0x05:
			case 0x06:
			case 0x07:
				break;
			case 0x08:
			case 0x09:
			case 0x0a:
			case 0x0b:
				return false;
			/* fall through */
			case 0x0f:
			if ((*utf8 & 0x08) == 0) 
			{
				utf8 = bytes++;
				if ((*utf8 & 0xc0) != 0x80) 
				{
					return false;
				}
			}
			else 
			{
				return false;
			}
			/* fall through */
			case 0x0e:
			utf8 = bytes++;
			if ((*utf8 & 0xc0) != 0x80) 
			{
				return false;
			}
			/* fall through */
			case 0x0c:
			case 0x0d:
			utf8 = bytes++;
			if ((*utf8 & 0xc0) != 0x80) 
			{
				return false;
			}
			break;
		}
	}
	return true;
}
