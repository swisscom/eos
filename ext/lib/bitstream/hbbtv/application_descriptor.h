/*
 * Modified by Swisscom (Schweiz) AG
 */

#ifndef EOS_EXT_LIB_BITSTREAM_HBBTV_APPLICATION_DESCRIPTOR_H_
#define EOS_EXT_LIB_BITSTREAM_HBBTV_APPLICATION_DESCRIPTOR_H_

/*****************************************************************************
 * Descriptor tag 0x00: Application descriptor
 *
 *****************************************************************************/
#define APPLICATION_DESCRIPTOR_TAG 	0x00


static inline bool application_desc_validate(const uint8_t *p_desc)
{
	/*TODO: descriptor validation */
	uint8_t temp = *p_desc;
	temp = 1;

    return temp;
}

#endif /* EOS_EXT_LIB_BITSTREAM_HBBTV_APPLICATION_DESCRIPTOR_H_ */
