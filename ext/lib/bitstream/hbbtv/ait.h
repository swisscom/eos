/*
 * Modified by Swisscom (Schweiz) AG
 */

/*
 * ETSI TS 102 809
 * Signalling and carriage of interactive applications and services
 * in Hybrid broadcast/broadband environments
 * */
#ifndef EOS_EXT_LIB_BITSTREAM_HBBTV_AIT_H_
#define EOS_EXT_LIB_BITSTREAM_HBBTV_AIT_H_

#ifdef __cplusplus
extern "C"
{
#endif

/*****************************************************************************
 * Application Information Table
 *****************************************************************************/
#define AIT_TABLE_ID            0x74
#define AIT_HEADER_SIZE			10

typedef enum app_ctrl_code
{
	 AIT_AUTOSTART  = 0x01,
	 AIT_PRESENT    = 0x02
}cron_plyr_step_t;


static inline uint16_t ait_get_common_descriptors_length(const uint8_t *p_ait)
{
    return ((p_ait[8] & 0x0f) << 8) | p_ait[9];
}

static inline uint16_t ait_get_application_loop_length(const uint8_t *p_ait)
{
	uint16_t common_desc_len = ait_get_common_descriptors_length(p_ait);

	return ((p_ait[common_desc_len + AIT_HEADER_SIZE] & 0x0f) << 8) | p_ait[common_desc_len + AIT_HEADER_SIZE+1];
}

static inline uint8_t ait_get_application_control_code(const uint8_t *p_ait)
{
	return p_ait[0];
}

static inline uint32_t ait_get_organisation_id(const uint8_t *p_ait)
{
	uint32_t o_id = 0;

	o_id = p_ait[0] << 24;
	o_id |= p_ait[1] << 16;
	o_id |= p_ait[2] << 8;
	o_id |= p_ait[3];

	return o_id;
}

static inline uint16_t ait_get_application_id(const uint8_t *p_ait)
{
	uint32_t a_id = 0;

	a_id = p_ait[0] << 8;
	a_id |= p_ait[1] ;

	return a_id;
}

static inline uint16_t ait_get_application_descriptors_loop_length(const uint8_t *p_ait)
{
	return ((p_ait[0] & 0x0f) << 8) | p_ait[1];

}

static inline bool ait_validate(const uint8_t *p_ait, uint32_t len)
{
	uint16_t section_len = psi_get_length(p_ait) ;
	uint16_t common_desc_len = ait_get_common_descriptors_length(p_ait);
	uint16_t app_loop_len = ait_get_application_loop_length(p_ait);


    if (!psi_get_syntax(p_ait) ||
    		psi_get_tableid(p_ait) != AIT_TABLE_ID)
        return false;

    if (!psi_check_crc(p_ait))
        return false;

    if(section_len != (7 + common_desc_len + 2 + app_loop_len + PSI_CRC_SIZE) ||
    		(((uint32_t)section_len + PSI_HEADER_SIZE) != len))
    	return false;

    return true;
}

static inline const char* ait_app_contr_codes_string(uint8_t app_contr_codes)
{
	switch (app_contr_codes)
	{
		case 0X01: return "AUTOSTART";
		case 0X02: return "PRESENT";
		case 0X03: return "DESTROY";
		case 0X04: return "KILL";
		case 0X05: return "PREFETCH";
		case 0X06: return "REMOTE";
		case 0X07: return "DISABLED";
		case 0X08: return "PLAYBACK_AUTOSTART";
		default: break;
	}
	return "/";
}

#ifdef __cplusplus
}
#endif


#endif /* EOS_EXT_LIB_BITSTREAM_HBBTV_AIT_H_ */
