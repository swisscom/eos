/*
 * Modified by Swisscom (Schweiz) AG
 */

#ifndef EOS_EXT_LIB_BITSTREAM_HBBTV_TRANSPORT_PROTOCOL_DESCRIPTOR_H_
#define EOS_EXT_LIB_BITSTREAM_HBBTV_TRANSPORT_PROTOCOL_DESCRIPTOR_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * Descriptor tag 0x02: Transport protocol descriptor
 *
 *****************************************************************************/
#define TRANSPORT_PROTOCOL_DESCRIPTOR_TAG 	0x02


static inline bool transport_protocol_desc_validate(const uint8_t *p_desc)
{
	/*TODO: descriptor validation */
	uint8_t temp = *p_desc;
	temp = 1;

    return temp;
}

static inline uint16_t transport_protocol_get_protocol_id(const uint8_t *p_desc)
{
	return ((p_desc[2] & 0x0f) << 8) | p_desc[3];
}

static inline uint8_t transport_protocol_get_url_base_length(const uint8_t *p_desc)
{
	return p_desc[0];
}

static inline void transport_protocol_get_url_base_byte(const uint8_t *p_desc, uint8_t* base_byte, uint8_t len)
{
	memcpy(base_byte, p_desc, len);
	//base_byte[len] = '\0';
	return;
}

static inline uint8_t transport_protocol_get_url_extension_count(const uint8_t *p_desc)
{
	return p_desc[0];
}

static inline void transport_protocol_get_url_extension_byte(const uint8_t *p_desc, uint8_t* base_byte, uint8_t len)
{
	memcpy(base_byte, p_desc, len);
	//base_byte[len] = '\0';
	return;
}

static inline void transport_protocol_get_initial_path__bytes(const uint8_t *p_desc, uint8_t* initial_path_bytes, uint8_t len)
{
	memcpy(initial_path_bytes, p_desc, len);
	initial_path_bytes[len] = '\0';
	return;
}
#ifdef __cplusplus
}
#endif

#endif /* EOS_EXT_LIB_BITSTREAM_HBBTV_TRANSPORT_PROTOCOL_DESCRIPTOR_H_ */
