/*
 * Modified by Swisscom (Schweiz) AG
 */

#ifndef EXT_LIB_BITSTREAM_HBBTV_APPLICATION_USAGE_DESCRIPTOR_H_
#define EXT_LIB_BITSTREAM_HBBTV_APPLICATION_USAGE_DESCRIPTOR_H_

#define APPLICATION_USAGE_DESCRIPTOR_TAG 	0x16


static inline bool application_usage_desc_validate(const uint8_t *p_desc)
{
	/*TODO: descriptor validation */
	uint8_t temp = *p_desc;
	temp = 1;

    return temp;
}

#endif /* EXT_LIB_BITSTREAM_HBBTV_APPLICATION_USAGE_DESCRIPTOR_H_ */
