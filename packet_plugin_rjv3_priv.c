#include "packet_plugin_rjv3_priv.h"
#include "eth_frame.h"
#include "if_impl.h"
#include "checkV4.h"
#include "config.h"

#include <stdint.h>
#include <string.h>
#include <netinet/in.h>

#define IS_MD5_FRAME(frame) \
    (frame->header->eapol_hdr.type[0] == EAP_PACKET \
        && frame->header->eap_hdr.type[0] == MD5_CHALLENGE)
        
void rjv3_set_dhcp_en(uint8_t* dhcp_en_arr, DHCP_TYPE dhcp_type) {
    dhcp_en_arr[3] = (dhcp_type != DHCP_NONE);
}

void rjv3_set_local_mac(uint8_t* mac_buf) {
    IF_IMPL* _if_impl = get_if_impl();
    if (_if_impl == NULL) return;
    
    _if_impl->obtain_mac(_if_impl, mac_buf);
}

void rjv3_set_pwd_hash(uint8_t* hash_buf, ETH_EAP_FRAME* request) {
    if (IS_MD5_FRAME(request)) {
        uint8_t* _hash_buf;
        EAP_CONFIG* _eap_config = get_eap_config();
        
        _hash_buf = (uint8_t*)computePwd(request->content + sizeof(FRAME_HEADER) + 1,
                                          _eap_config->username, _eap_config->password); 
        memmove(hash_buf, _hash_buf, 16);
        /* 1 = sizeof(MD5-Value-Size), this is where MD5-Value starts */
    }
}

void rjv3_set_ipv6_addr(uint8_t* ll_slaac, uint8_t* ll_temp, uint8_t* global) {
    LIST_ELEMENT *_ip_list, *_ip_curr;
    IF_IMPL* _if_impl = get_if_impl();
    if (_if_impl == NULL) return;
    
    _if_impl->obtain_ip(_if_impl, &_ip_list);
    _ip_curr = _ip_list;
    
#define IP_ELEM ((IP_ADDR*)(_ip_curr->content))
    do {
        if (IP_ELEM->family == AF_INET6) {
            if ((IP_ELEM->ip[0] & 0xf0) == 0x20) { // 2xxx:: Global scope (ROUGH)
                memmove(global, IP_ELEM->ip, 16);
            } else if (IP_ELEM->ip[0] == 0xfe && IP_ELEM->ip[1] == 0x80) { // fe80:: Link local
                if (IP_ELEM->ip[11] == 0xff && IP_ELEM->ip[12] == 0xfe) {
                    /* ::xxff:fexx:: is the symbol of SLAAC */
                    memmove(ll_slaac, IP_ELEM->ip, 16);
                } else {
                    /* Privacy extension, will be used to form an global address also */
                    memmove(ll_temp, IP_ELEM->ip, 16);
                }
            }
        }
    } while ((_ip_curr = _ip_curr->next));
    list_destroy(_ip_list);
}

void rjv3_set_v3_hash(uint8_t* hash_buf, ETH_EAP_FRAME* request) {
    uint8_t* _v3_buf;

    /* computeV4 returns its internal buffer, which can not be freed */
    if (IS_MD5_FRAME(request)) {
        _v3_buf = computeV4(request->content + sizeof(FRAME_HEADER) + 1, /* position of MD5-Value */
                            *(request->content + sizeof(FRAME_HEADER))); /* position of MD5-Value-Size */
    } else {
        uint8_t _v3_pad[RJV3_PAD_SIZE] = {0};
        _v3_buf = computeV4(_v3_pad, RJV3_PAD_SIZE);
    }
    memmove(hash_buf, _v3_buf, 0x80);
}

void rjv3_set_service_name(uint8_t* name_buf, char* cmd_opt) {
    if (cmd_opt == NULL) {
        cmd_opt = "internet";
    }
    memmove(name_buf, cmd_opt, strlen(cmd_opt));
};