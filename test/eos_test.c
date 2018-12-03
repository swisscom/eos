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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eos.h"
#include "eos_macro.h"
#include "eos_types.h"
#include "linenoise.h"

#define CMD_ARGS_DELIMITER (" ")
#define CMD_ARGS_DELIMITER_CHR (' ')
#define CMD_QUIT ("q")
#define CMD_EXIT ("exit")

#define APP_CONSOLE_PREFIX ("eos$ ")

#define OPT_MAIN_AV ("main")
#define OPT_AUX_AV ("aux")

#define OPT_ON ("on")
#define OPT_OFF ("off")

#define OPT_TRACK_ID ("id")
#define OPT_TRACK_ORD ("ord")

#define HISTORY_FILE ("./eos_test.txt")

typedef int (*command_func_t)(char** args);

typedef struct command
{
	char* cmd;
	char* desc;
	command_func_t func;
} command_t;

static eos_error_t event_callback(eos_out_t out, eos_event_t event, eos_event_data_t* data, void* cookie);
static eos_error_t data_callback(eos_out_t out, eos_data_cls_t clazz,
		eos_data_fmt_t format, eos_data_t data, uint32_t size, void* cookie);
static char** args_parse(const char* line);
static void args_release(char** line);
static command_func_t get_cmd_func(char* cmd);
static int get_out_opt(char* opt, eos_out_t* out);
static int get_onoff_opt(char* opt, bool* on);
static void completition_cbk(const char *str, linenoiseCompletions *lc);

static inline void eos_puts(char* msg)
{
	printf("%s\r\n", msg);
	fflush(stdout);
}

static int cmd_help(char** args);
static int cmd_start(char** args);
static int cmd_stop(char** args);
static int cmd_gettracks(char** args);
static int cmd_settrack(char** args);
static int cmd_scale(char** args);
static int cmd_move(char** args);
static int cmd_play(char** args);
static int cmd_ttxt_pull(char** args);
static int cmd_data_switch(char** args);
static int cmd_ttxt_set_page(char** args);
static int cmd_ttxt_get_page(char** args);
static int cmd_ttxt_next_page_get(char** args);
static int cmd_ttxt_prev_page_get(char** args);
static int cmd_ttxt_red_page_get(char** args);
static int cmd_ttxt_green_page_get(char** args);
static int cmd_ttxt_blue_page_get(char** args);
static int cmd_ttxt_yellow_page_get(char** args);
static int cmd_ttxt_transparency_set(char** args);
static int cmd_dvbsub(char** args);
static int cmd_ttxt(char** args);
static int cmd_audio_mode(char** args);
static int cmd_vol_leveling(char** args);

static command_t cmds[] =
{
	{"help", "Shows help", cmd_help},
	{"start", "Starts playback: start <main|aux> <url> [<position> <speed>]", cmd_start},
	{"stop", "Stops playback: stop <main|aux>", cmd_stop},
	{"gettracks", "Retrieves available tracks: gettracks <main|aux>", cmd_gettracks},
	{"settrack", "Turns ON/OFF a given track: settrack <main|aux> "
			"<on|off> <id <track id>|ord <ordinal number>>", cmd_settrack},
	{"scale", "Scales video to given size: scale <main|aux> <width> <height>", cmd_scale},
	{"move", "Moves video to given position: scale <main|aux> <x> <y>", cmd_move},
	{"play", "Plays the stream from a given position at given speed: play <main|aux> <position> <speed>", cmd_play},
	{"ttxt_pull", "Retrieves and prints TTXT page: ttxt_pull <main|aux> <page #>", cmd_ttxt_pull},
	{"data_switch", "Enable/Disable data handling: data_switch <on|off>", cmd_data_switch},
	{"ttxt_set_page", "Set TTXT page and subpage: ttxt_set_page <main|aux> <page> <subpage>", cmd_ttxt_set_page},
	{"ttxt_get_page", "Get TTXT current page: ttxt_get_page ", cmd_ttxt_get_page},
	{"ttxt_next_page_get", "Get TTXT next page: ttxt_next_page_get ", cmd_ttxt_next_page_get},
	{"ttxt_red_page_get", "Get TTXT red page: ttxt_red_page_get ", cmd_ttxt_red_page_get},
	{"ttxt_green_page_get", "Get TTXT green page: ttxt_green_page_get ", cmd_ttxt_green_page_get},
	{"ttxt_prev_page_get", "Get TTXT previous page: ttxt_prev_page_get ", cmd_ttxt_prev_page_get},
	{"ttxt_blue_page_get", "Get TTXT blue page: ttxt_blue_page_get ", cmd_ttxt_blue_page_get},
	{"ttxt_yellow_page_get", "Get TTXT yellow page: ttxt_yellow_page_get ", cmd_ttxt_yellow_page_get},
	{"ttxt_transparency_set", "Set TTXT transparency: ttxt_transparency_set ", cmd_ttxt_transparency_set},
	{"dvbsub", "Enable/Disable DVB subtitles: dvbsub <main|aux> <on|off>", cmd_dvbsub},
	{"ttxt", "Enable/Disable Teletext: ttxt <main|aux>  <on|off>", cmd_ttxt},
	{"passthrough", "Switches passthrough on/off <1|0>" , cmd_audio_mode},
	{"vol_lvl", "Sets volume leveling <on/off> [light|normal|heavy] " , cmd_vol_leveling},
	{NULL, NULL, NULL}
};

int main(int argc, char** argv)
{
	char *line = NULL, **args = NULL;
	char *parse_line;
	eos_error_t err = EOS_ERROR_OK;
	command_func_t func = NULL;
	uint8_t q = 0;

	/* kill warning */
	if(argc == 0 && argv == NULL)
	{

	}
	if((err = eos_init()) != EOS_ERROR_OK)
	{
		return err;
	}
	eos_set_event_cbk(event_callback, NULL);
	linenoiseHistoryLoad(HISTORY_FILE);
	linenoiseSetCompletionCallback(completition_cbk);
	while((line = linenoise(APP_CONSOLE_PREFIX)) != NULL)
	{
		parse_line = calloc(1, strlen(line) + 1);
		strcpy(parse_line, line);
		args = args_parse(parse_line);
		if(strcmp(args[0], CMD_QUIT) == 0 || strcmp(args[0], CMD_EXIT) == 0)
		{
			q = 1;
		}
		else if((func = get_cmd_func(args[0])) != NULL)
		{
			func(args + 1);
			linenoiseHistoryAdd(line);
			linenoiseHistorySave(HISTORY_FILE);
		}
		args_release(args);
        free(line);
        free(parse_line);
        if(q == 1)
        {
        	break;
        }
	}
	if((err = eos_deinit()) != EOS_ERROR_OK)
	{
		return err;
	}

	return 0;
}

static eos_error_t event_callback(eos_out_t out, eos_event_t event, eos_event_data_t* data, void* cookie)
{
	(void)(cookie);
	(void)data;

	switch(event)
	{
	case EOS_EVENT_STATE:
		{
			char* text = NULL;
			switch(data->state.state)
			{
				case EOS_STATE_PAUSED:
					text = "paused";
					break;
				case EOS_STATE_TRANSITIONING:
					text = "transitioning";
					break;
				case EOS_STATE_STOPPED:
					text = "stopped";
					break;
				case EOS_STATE_PLAYING:
					text = "playing";
					break;
				case EOS_STATE_BUFFERING:
					text = "buffering";
					break;
				default:
					text = "in unknown state";
					break;
			}
			printf("Playback %s for %d\r\n", text, out);
			fflush(stdout);
		}
		break;
	default:
		break;
	}
	return EOS_ERROR_OK;
}

static eos_error_t data_callback(eos_out_t out, eos_data_cls_t clazz,
		eos_data_fmt_t format, eos_data_t data, uint32_t size, void* cookie)
{
	EOS_UNUSED(out);
	EOS_UNUSED(clazz);
	EOS_UNUSED(format);
	EOS_UNUSED(size);
	EOS_UNUSED(cookie);

	eos_puts((char*)data);

	return EOS_ERROR_OK;
}

static char** args_parse(const char* line)
{
	int count = 1;
	unsigned int i = 0;
	char check = 0;
	char **ret = NULL;
	char *tmp = NULL;

	while(i < strlen(line))
	{
		check = line[i];
		if(check == CMD_ARGS_DELIMITER_CHR)
		{
			count++;
		}
		i++;
	}
	/* for end of array */
	count++;
	ret = calloc(count, sizeof(char*));
	i = 0;
	tmp = strtok((char*)line, CMD_ARGS_DELIMITER);
	while(tmp != NULL)
	{
		ret[i] = tmp;
		tmp = strtok(NULL, CMD_ARGS_DELIMITER);
		i++;
	}
	if(ret[0] == NULL)
	{
		ret[0] = (char *)line;
	}

	return ret;
}

static void args_release(char** line)
{
	if(line == NULL)
	{
		return;
	}
	free(line);

	return;
}

static command_func_t get_cmd_func(char* cmd)
{
	command_t *tmp = NULL;
	int i = 0;

	if(cmd == NULL)
	{
		return NULL;
	}
	while(cmds[i].cmd != NULL)
	{
		tmp = &cmds[i];
		if(strcmp(cmd, tmp->cmd) == 0)
		{
			return tmp->func;
		}
		i++;
	}

	return NULL;
}

static int get_out_opt(char* opt, eos_out_t* out)
{
	if(opt == NULL || out == NULL)
	{
		return -1;
	}
	if(strcmp(opt, OPT_MAIN_AV) == 0)
	{
		*out = EOS_OUT_MAIN_AV;
	}
	else if(strcmp(opt, OPT_AUX_AV) == 0)
	{
		*out = EOS_OUT_AUX_AV;
	}
	else
	{
		return -1;
	}

	return 0;
}

static int get_onoff_opt(char* opt, bool* on)
{
	if(opt == NULL || on == NULL)
	{
		return -1;
	}
	if(strcmp(opt, OPT_ON) == 0)
	{
		*on = true;
	}
	else if(strcmp(opt, OPT_OFF) == 0)
	{
		*on = false;
	}
	else
	{
		return -1;
	}

	return 0;
}

static int cmd_help(char** args)
{
	int i = 0;
	(void)(args);

	eos_puts("EOS test app commands:");
	while(cmds[i].cmd != NULL)
	{
		printf("%s  \t   %s\r\n", cmds[i].cmd, cmds[i].desc);
		i++;
	}
	return 0;
}

static int cmd_start(char** args)
{
	char *out = args[0];
	char *url = args[1];
	char *position = NULL;
	char *speed= NULL;
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	char *extras = NULL;
	int32_t extras_max_len = 64;

	position = args[2];
	if (position)
	{
		speed = args[3];
	}

	eos_puts("START called");
	if(out == NULL || url == NULL)
	{
		eos_puts("Not enough arguments!!!");
		return -1;
	}
	if(get_out_opt(out, &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}
	if (position || speed)
	{
		extras = calloc(1, extras_max_len + 1);
	}

	if (position)
	{
		snprintf(extras, extras_max_len, "pos=%s", position);
	}

	if (speed)
	{
		snprintf(extras + strlen(extras), extras_max_len - strlen(extras), "&speed=%s", speed);
	}

	if(eos_player_play(url, extras, eos_out) != EOS_ERROR_OK)
	{
		eos_puts("PLAY failed!!!");
		if (extras)
		{
			free(extras);
		}
		return -1;
	}

	if (extras)
	{
		free(extras);
	}
	eos_puts("START done");

	return 0;
}

static int cmd_stop(char** args)
{
	char *out = args[0];
	eos_out_t eos_out = EOS_OUT_MAIN_AV;

	eos_puts("STOP called");
	if(get_out_opt(out, &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}
	if(eos_player_stop(eos_out) != EOS_ERROR_OK)
	{
		eos_puts("STOP failed!!!");
		return -1;
	}
	eos_puts("STOP done");

	return 0;

}

static int cmd_gettracks(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	eos_media_desc_t desc;
	unsigned int i = 0;

	if (get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}

	if (eos_player_get_media_desc(eos_out, &desc) != EOS_ERROR_OK)
	{
		eos_puts("Tracks retrieval failed!!!");
		return -1;
	}

	for (i = 0; i < desc.es_cnt; i++)
	{
		printf("%sTrack[%u]: %s (%s) ID:%u Languge:\"%s\"\r\n",
				(desc.es[i].selected == true)? "*":" ",
				i, media_codec_string(desc.es[i].codec),
				(EOS_MEDIA_IS_VID(desc.es[i].codec))?"V":
						(EOS_MEDIA_IS_AUD(desc.es[i].codec)?"A":
						(EOS_MEDIA_IS_DAT(desc.es[i].codec)?"D":"/")),
				desc.es[i].id, (desc.es[i].lang[0] == '\0')?
						"none":desc.es[i].lang);
	}

	return 0;
}

static int cmd_settrack(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	bool on = true;
	unsigned int id = (unsigned int)-1;
	unsigned int ordnum = (unsigned int)-1;
	unsigned int i = 0;
	eos_media_desc_t desc;

	if (get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}

	if (get_onoff_opt(args[1], &on) != 0)
	{
		eos_puts("Bad on/off argument!!!");
		return -1;
	}

	if ((args[2] == NULL) || (args[3] == NULL))
	{
		eos_puts("No stream identification argument!!!");
		return -1;
	}
	if (strcmp(args[2], OPT_TRACK_ID) == 0)
	{
		if (sscanf(args[3], "%u", &id) != 1)
		{
			eos_puts("Bad id argument!!!");
			return -1;
		}
	}

	if (strcmp(args[2], OPT_TRACK_ORD) == 0)
	{
		if (sscanf(args[3], "%u", &ordnum) != 1)
		{
			eos_puts("Bad ord argument!!!");
			return -1;
		}
	}

	if ((id == (unsigned int)-1) && (ordnum == (unsigned int)-1))
	{
		eos_puts("Bad stream identification argument!!!");
		return -1;
	}

	if (eos_player_get_media_desc(eos_out, &desc) != EOS_ERROR_OK)
	{
		eos_puts("Tracks retrieval failed!!!");
		return -1;
	}

	if ((ordnum != (unsigned int)-1) && (ordnum >= desc.es_cnt))
	{
		eos_puts("Bad ord argument!!!");
		return -1;
	}

	for (i = 0; i < desc.es_cnt; i++)
	{
		if ((ordnum == i) || (id == desc.es[i].id))
		{
			break;
		}
	}

	if (i >= desc.es_cnt)
	{
		eos_puts("Identification argument not present in the list!!!");
		return -1;
	}

	printf("Turning %s track[%u]: %s ID:%u Languge:\"%s\"\n",
			(on == true)? "ON":"OFF", i,
			(EOS_MEDIA_IS_VID(desc.es[i].codec))?"Video":
					(EOS_MEDIA_IS_AUD(desc.es[i].codec)?"Audio":
					(EOS_MEDIA_IS_DAT(desc.es[i].codec)?"Data ":
					"Undef")), desc.es[i].id,
			(desc.es[i].lang[0] == '\0')? "none":desc.es[i].lang);

	if (eos_player_set_track(eos_out, desc.es[i].id, on) != EOS_ERROR_OK)
	{
		eos_puts("Failed");
		return -1;
	}
	eos_puts("Successful");
	return 0;
}

static int cmd_audio_mode(char** args)
{
	char* on = args[0];
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	eos_out_amode_t audio_mode;

	eos_puts("PASSTHROUGH called");

	int enable = atoi((const char*)on);

	(enable == 1) ? (audio_mode = EOS_OUT_AMODE_PT) : (audio_mode = EOS_OUT_AMODE_STEREO);

	if(eos_out_aud_mode(eos_out, audio_mode) != EOS_ERROR_OK)
	{
		eos_puts("PASSTHROUGH failed!!!");
		return -1;
	}
	eos_puts("PASSTHROUGH set");

	return 0;
}

static int cmd_vol_leveling(char** args)
{
	bool on = true;
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	eos_out_vol_lvl_t lvl = EOS_OUT_VOL_LVL_NORMAL;

	eos_puts("VOL LVL called");
	if (get_onoff_opt(args[0], &on) != 0)
	{
		eos_puts("Bad on/off argument!!!");
		return -1;
	}

	if (args[1] != NULL)
	{
		if (strcasecmp(args[1], "light") == 0)
		{
			lvl = EOS_OUT_VOL_LVL_LIGHT;
		}
		else if (strcasecmp(args[1], "heavy") == 0)
		{
			lvl = EOS_OUT_VOL_LVL_HEAVY;
		}
		else
		{
			lvl = EOS_OUT_VOL_LVL_NORMAL;
		}
	}

	if (eos_out_vol_leveling(eos_out, on, lvl) != EOS_ERROR_OK)
	{
		eos_puts("VOL LVL failed!!!");
	}
	eos_puts("VOL LVL exited");

	return 0;
}

static int cmd_scale(char** args)
{
	char *out = args[0];
	char *w = args[1];
	char *h = args[2];
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	int width = 0, height = 0;

	eos_puts("SCALE called");
	if(get_out_opt(out, &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}
	width = atoi((const char*)w);
	height = atoi((const char*)h);

	if(eos_out_vid_scale(eos_out, width, height) != EOS_ERROR_OK)
	{
		eos_puts("SCALE failed!!!");
		return -1;
	}
	eos_puts("SCALE exited");

	return 0;
}

static int cmd_move(char** args)
{
	char *out = args[0];
	char *x = args[1];
	char *y = args[2];
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	int pos_x = 0, pos_y = 0;

	eos_puts("MOVE called");
	if(get_out_opt(out, &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}
	pos_x = atoi((const char*)x);
	pos_y = atoi((const char*)y);

	if(eos_out_vid_move(eos_out, pos_x, pos_y) != EOS_ERROR_OK)
	{
		eos_puts("MOVE failed!!!");
		return -1;
	}
	eos_puts("MOVE exited");

	return 0;
}

static int cmd_play(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	int pos = 0;
	int speed = 0;

	eos_puts("PLAY called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}
	pos = atoi((const char*)args[1]);
	speed = atoi((const char*)args[2]);

	if (eos_player_trickplay(eos_out, pos, speed))
	{
		eos_puts("PLAY failed!!!");
		return -1;
	}
	eos_puts("PLAY exited");

	return 0;
}

static int cmd_ttxt_pull(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	int page = 0;
	char *json = NULL;

	eos_puts("TTXT PULL called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}
	page = atoi((const char*)args[1]);
	if(eos_player_get_ttxt_pg(eos_out, (uint16_t)page, &json) != EOS_ERROR_OK)
	{
		eos_puts("TTXT PULL failed!!!");
		return -1;
	}
	eos_puts(json);
	free(json);
	eos_puts("TTXT PULL exited");

	return 0;
}

static int cmd_data_switch(char** args)
{
	bool on = false;

	eos_puts("DATA SWITCH called");
	if(get_onoff_opt(args[0], &on) != 0)
	{
		eos_puts("Bad on/off argument!!!");
		return -1;
	}
	if(on)
	{
		eos_data_cbk_set(data_callback, NULL);
	}
	else
	{
		eos_data_cbk_set(NULL, NULL);
	}
	eos_puts("DATA SWITCH exited");

	return 0;
}

static int cmd_ttxt_set_page(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	uint16_t page = 0;
	uint16_t sub_page = 0;

	eos_puts("TTXT SET PAGE called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}
	page = atoi((const char*)args[1]);
	sub_page = atoi((const char*)args[2]);
	eos_data_ttxt_page_set(eos_out, page, sub_page);
	eos_puts("TTXT SET PAGE exited");

	return 0;
}

static int cmd_dvbsub(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	bool on = false;

	eos_puts("DVBSUB called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}
	if(get_onoff_opt(args[1], &on) != 0)
	{
		eos_puts("Bad on/off argument!!!");
		return -1;
	}
	eos_data_dvbsub_enable(eos_out, on);
	eos_puts("DVBSUB exited");

	return 0;
}

static int cmd_ttxt(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	bool on = false;

	eos_puts("TTXT called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}
	if(get_onoff_opt(args[1], &on) != 0)
	{
		eos_puts("Bad on/off argument!!!");
		return -1;
	}
	eos_data_ttxt_enable(eos_out, on);
	eos_puts("TTXT exited");

	return 0;
}


static int cmd_ttxt_get_page(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	uint16_t page = 0;
	uint16_t sub_page = 0;

	eos_puts("TTXT GET PAGE called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}

	eos_data_ttxt_page_get(eos_out, &page, &sub_page);
	printf("TTXT GET PAGE page = %d sub_page = %d\n", page, sub_page);
	eos_puts("TTXT GET PAGE exited");

	return 0;
}

static int cmd_ttxt_next_page_get(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	uint16_t next_page = 0;

	eos_puts("TTXT NEXT PAGE called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}

	eos_data_ttxt_next_page_get(eos_out, &next_page);
	printf("TTXT NEXT PAGE page = %d\n", next_page);
	eos_puts("TTXT NEXT PAGE exited");

	return 0;
}

static int cmd_ttxt_prev_page_get(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	uint16_t prev_page = 0;

	eos_puts("TTXT PREV PAGE called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}

	eos_data_ttxt_prev_page_get(eos_out, &prev_page);
	printf("TTXT PREV PAGE page = %d\n", prev_page);
	eos_puts("TTXT PREV PAGE exited");

	return 0;
}

static int cmd_ttxt_red_page_get(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	uint16_t red = 0;

	eos_puts("TTXT RED PAGE called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}

	eos_data_ttxt_red_page_get(eos_out, &red);
	printf("TTXT RED PAGE page = %d\n", red);
	eos_puts("TTXT RED PAGE exited");

	return 0;
}

static int cmd_ttxt_green_page_get(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	uint16_t green = 0;

	eos_puts("TTXT GREEN PAGE called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}

	eos_data_ttxt_green_page_get(eos_out, &green);
	printf("TTXT GREEN PAGE page = %d\n", green);
	eos_puts("TTXT GREEN PAGE exited");

	return 0;
}

static int cmd_ttxt_blue_page_get(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	uint16_t blue = 0;

	eos_puts("TTXT BLUE PAGE called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}

	eos_data_ttxt_blue_page_get(eos_out, &blue);
	printf("TTXT BLUE PAGE page = %d\n", blue);
	eos_puts("TTXT BLUE PAGE exited");

	return 0;
}

static int cmd_ttxt_yellow_page_get(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	uint16_t yellow = 0;

	eos_puts("TTXT YELLOW PAGE called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}

	eos_data_ttxt_yellow_page_get(eos_out, &yellow);
	printf("TTXT YELLOW PAGE page = %d\n", yellow);
	eos_puts("TTXT YELLOW PAGE exited");

	return 0;
}

static int cmd_ttxt_transparency_set(char** args)
{
	eos_out_t eos_out = EOS_OUT_MAIN_AV;
	uint8_t alpha = 0;

	eos_puts("TTXT SET TRANSPARENCY called");
	if(get_out_opt(args[0], &eos_out) != 0)
	{
		eos_puts("Bad out argument!!!");
		return -1;
	}
	alpha = atoi((const char*)args[1]);
	eos_data_ttxt_transparency_set(eos_out, alpha);
	eos_puts("TTXT SET TRANSPARENCY exited");

	return 0;
}

static void completition_cbk(const char *str, linenoiseCompletions *lc)
{
	int i = 0;

	while(cmds[i].cmd != NULL)
	{
		if(strstr(cmds[i].cmd, str) == cmds[i].cmd)
		{
			linenoiseAddCompletion(lc, cmds[i].cmd);
		}
		i++;
	}
}

