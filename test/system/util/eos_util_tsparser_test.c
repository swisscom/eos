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


#include "util_tsparser.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

eos_error_t print_media_desc(eos_media_desc_t desc)
{
        uint32_t i = 0;
        for (i = 0; i < desc.es_cnt; i++)
        {
                printf("%sTrack[%u]: %s (%s) ID:%u Languge:\"%s\"\n", (desc.es[i].selected == true)? "*":" ", i, media_codec_string(desc.es[i].codec), (EOS_MEDIA_IS_VID(desc.es[i].codec))?"V":(EOS_MEDIA_IS_AUD(desc.es[i].codec)?"A":(EOS_MEDIA_IS_DAT(desc.es[i].codec)?"D":(EOS_MEDIA_IS_DRM(desc.es[i].codec)?"E":"/"))), desc.es[i].id, (desc.es[i].lang[0] == '\0')? "none":desc.es[i].lang);
        }
        return EOS_ERROR_OK;
}

int main(int argc, char** argv)
{
        int fd = -1;
        eos_error_t err = EOS_ERROR_OK;
        uint8_t ts_packet[188];
        eos_media_desc_t desc;
        util_tsparser_t *tsparser = NULL;

        if (argc != 2)
        {
                printf("File path missing\n");
                return -1;
        }

        fd = open(argv[1], O_RDONLY);
        if (fd < 0)
        {
                printf("Unable to open file\n");
                return -1;
        }

        if (util_tsparser_create(&tsparser) != EOS_ERROR_OK)
        {
                printf("Unable to create a pid list\n");
                return -1;
        }
	memset(&desc, 0, sizeof(eos_media_desc_t));
        printf("\nParsing PMT from %s\n", argv[1]);
        while (read(fd, ts_packet, sizeof(ts_packet)) == sizeof(ts_packet))
        {
            err = util_tsparser_get_media_info(tsparser, ts_packet, sizeof(ts_packet), INFO_ID_FIRST_FOUND, &desc);
            if (err == EOS_ERROR_OK)
            {
                    break;
            }
        }

        util_tsparser_destroy(&tsparser);
        if (desc.es_cnt > 0)
        {
                print_media_desc(desc);
        }
        else
        {
                printf("Media info is not present\n");
        }

        close(fd);
        return 0;
}

