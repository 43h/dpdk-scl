/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _RTE_ETHER_H_
#define _RTE_ETHER_H_

/**
 * @file
 *
 * Ethernet Helpers in RTE
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdio.h>

#include <rte_random.h>
#include <rte_mbuf.h>
#include <rte_byteorder.h>

#define RTE_ETHER_ADDR_LEN  6 /**< Length of Ethernet address. mac地址6字节*/
#define RTE_ETHER_TYPE_LEN  2 /**< Length of Ethernet type field. 以太网报文类型2字节*/
#define RTE_ETHER_CRC_LEN   4 /**< Length of Ethernet CRC. 冗余校验2字节*/
#define RTE_ETHER_HDR_LEN   \
	(RTE_ETHER_ADDR_LEN * 2 + \
		RTE_ETHER_TYPE_LEN) /**< Length of Ethernet header. */
#define RTE_ETHER_MIN_LEN   64    /**< Minimum frame len, including CRC. 最小帧长*/
#define RTE_ETHER_MAX_LEN   1518  /**< Maximum frame len, including CRC. 最大帧长*/
#define RTE_ETHER_MTU       \
	(RTE_ETHER_MAX_LEN - RTE_ETHER_HDR_LEN - \
		RTE_ETHER_CRC_LEN) /**< Ethernet MTU. */

#define RTE_VLAN_HLEN       4  /**< VLAN (IEEE 802.1Q) header length. VLAN头长度*/
/** Maximum VLAN frame length (excluding QinQ), including CRC. */
#define RTE_ETHER_MAX_VLAN_FRAME_LEN \
	(RTE_ETHER_MAX_LEN + RTE_VLAN_HLEN)

#define RTE_ETHER_MAX_JUMBO_FRAME_LEN \
	0x3F00 /**< Maximum Jumbo frame length, including CRC. 巨型帧长*/

#define RTE_ETHER_MAX_VLAN_ID  4095 /**< Maximum VLAN ID. */

#define RTE_ETHER_MIN_MTU 68 /**< Minimum MTU for IPv4 packets, see RFC 791. */

/**
 * Ethernet address:
 * A universally administered address is uniquely assigned to a device by its
 * manufacturer. The first three octets (in transmission order) contain the
 * Organizationally Unique Identifier (OUI). The following three (MAC-48 and
 * EUI-48) octets are assigned by that organization with the only constraint
 * of uniqueness.
 * A locally administered address is assigned to a device by a network
 * administrator and does not contain OUIs.
 * See http://standards.ieee.org/regauth/groupmac/tutorial.html
 */
struct rte_ether_addr {
	uint8_t addr_bytes[RTE_ETHER_ADDR_LEN]; /**< Addr bytes in tx order */
} __rte_aligned(2); //2字节对齐

//第一字节的第二低位，标识是本地还是全局
#define RTE_ETHER_LOCAL_ADMIN_ADDR 0x02 /**< Locally assigned Eth. address. */
//第一节字节第一地位，标识组播(广播)还是单播
#define RTE_ETHER_GROUP_ADDR  0x01 /**< Multicast or broadcast Eth. address. */

/**
 * Check if two Ethernet addresses are the same.
 *
 * @param ea1
 *  A pointer to the first ether_addr structure containing
 *  the ethernet address.
 * @param ea2
 *  A pointer to the second ether_addr structure containing
 *  the ethernet address.
 *
 * @return
 *  True  (1) if the given two ethernet address are the same;
 *  False (0) otherwise.
 */
static inline int rte_is_same_ether_addr(const struct rte_ether_addr *ea1,
				     const struct rte_ether_addr *ea2)
{
	const uint16_t *w1 = (const uint16_t *)ea1;
	const uint16_t *w2 = (const uint16_t *)ea2;
    //如果用"w1[0] ==w2[0]"，实际是"w1[0] - w2[2]"，
    //位运算比减法快，所以这里用的是异或
    //异或性质：若a =b, a^b = 0; 
	return ((w1[0] ^ w2[0]) | (w1[1] ^ w2[1]) | (w1[2] ^ w2[2])) == 0;
}

/**
 * Check if an Ethernet address is filled with zeros.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is filled with zeros;
 *   false (0) otherwise.
 */
static inline int rte_is_zero_ether_addr(const struct rte_ether_addr *ea)
{
	const uint16_t *w = (const uint16_t *)ea;

	return (w[0] | w[1] | w[2]) == 0;
}

/**
 * Check if an Ethernet address is a unicast address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a unicast address;
 *   false (0) otherwise.
 */
static inline int rte_is_unicast_ether_addr(const struct rte_ether_addr *ea)
{
	return (ea->addr_bytes[0] & RTE_ETHER_GROUP_ADDR) == 0; //检测第一字节最低位
}

/**
 * Check if an Ethernet address is a multicast address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a multicast address;
 *   false (0) otherwise.
 */
static inline int rte_is_multicast_ether_addr(const struct rte_ether_addr *ea)
{
	return ea->addr_bytes[0] & RTE_ETHER_GROUP_ADDR;
}

/**
 * Check if an Ethernet address is a broadcast address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a broadcast address;
 *   false (0) otherwise.
 */
static inline int rte_is_broadcast_ether_addr(const struct rte_ether_addr *ea)
{
	const uint16_t *w = (const uint16_t *)ea;

	return (w[0] & w[1] & w[2]) == 0xFFFF;
}

/**
 * Check if an Ethernet address is a universally assigned address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a universally assigned address;
 *   false (0) otherwise.
 */
static inline int rte_is_universal_ether_addr(const struct rte_ether_addr *ea)
{
	return (ea->addr_bytes[0] & RTE_ETHER_LOCAL_ADMIN_ADDR) == 0;
}

/**
 * Check if an Ethernet address is a locally assigned address.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is a locally assigned address;
 *   false (0) otherwise.
 */
static inline int rte_is_local_admin_ether_addr(const struct rte_ether_addr *ea)
{
	return (ea->addr_bytes[0] & RTE_ETHER_LOCAL_ADMIN_ADDR) != 0;
}

/**
 * Check if an Ethernet address is a valid address. Checks that the address is a
 * unicast address and is not filled with zeros.
 *
 * @param ea
 *   A pointer to a ether_addr structure containing the ethernet address
 *   to check.
 * @return
 *   True  (1) if the given ethernet address is valid;
 *   false (0) otherwise.
 */
static inline int rte_is_valid_assigned_ether_addr(const struct rte_ether_addr *ea)
{
	return rte_is_unicast_ether_addr(ea) && (!rte_is_zero_ether_addr(ea));
}

/**
 * Generate a random Ethernet address that is locally administered
 * and not multicast.
 * @param addr
 *   A pointer to Ethernet address.
 */
void
rte_eth_random_addr(uint8_t *addr);

/**
 * Copy an Ethernet address.
 *
 * @param ea_from
 *   A pointer to a ether_addr structure holding the Ethernet address to copy.
 * @param ea_to
 *   A pointer to a ether_addr structure where to copy the Ethernet address.
 */
static inline void
rte_ether_addr_copy(const struct rte_ether_addr *__restrict ea_from,
		    struct rte_ether_addr *__restrict ea_to)
{
	*ea_to = *ea_from;
}

/**
 * Macro to print six-bytes of MAC address in hex format
 */
#define RTE_ETHER_ADDR_PRT_FMT     "%02X:%02X:%02X:%02X:%02X:%02X"
/**
 * Macro to extract the MAC address bytes from rte_ether_addr struct
 */
#define RTE_ETHER_ADDR_BYTES(mac_addrs) ((mac_addrs)->addr_bytes[0]), \
					 ((mac_addrs)->addr_bytes[1]), \
					 ((mac_addrs)->addr_bytes[2]), \
					 ((mac_addrs)->addr_bytes[3]), \
					 ((mac_addrs)->addr_bytes[4]), \
					 ((mac_addrs)->addr_bytes[5])

#define RTE_ETHER_ADDR_FMT_SIZE         18
/**
 * Format 48bits Ethernet address in pattern xx:xx:xx:xx:xx:xx.
 * //将地址转换为字符串
 * @param buf
 *   A pointer to buffer contains the formatted MAC address.
 * @param size
 *   The format buffer size.
 * @param eth_addr
 *   A pointer to a ether_addr structure.
 */
void
rte_ether_format_addr(char *buf, uint16_t size,
		      const struct rte_ether_addr *eth_addr);
/**
 * Convert string with Ethernet address to an ether_addr.
 * //将字符串转换为eth地址
 * @param str
 *   A pointer to buffer contains the formatted MAC address.
 *   The supported formats are:
 *     XX:XX:XX:XX:XX:XX or XXXX:XXXX:XXXX
 *   where XX is a hex digit: 0-9, a-f, or A-F.
 * @param eth_addr
 *   A pointer to a ether_addr structure.
 * @return
 *   0 if successful
 *   -1 and sets rte_errno if invalid string
 */
int
rte_ether_unformat_addr(const char *str, struct rte_ether_addr *eth_addr);

/**
 * Ethernet header: Contains the destination address, source address
 * and frame type. 帧头定义
 */
struct rte_ether_hdr {
	struct rte_ether_addr dst_addr; /**< Destination address. */
	struct rte_ether_addr src_addr; /**< Source address. */
	rte_be16_t ether_type; /**< Frame type. */
} __rte_aligned(2);

/**
 * Ethernet VLAN Header.
 * Contains the 16-bit VLAN Tag Control Identifier and the Ethernet type
 * of the encapsulated frame. vlan头定义
 */
struct rte_vlan_hdr {
	rte_be16_t vlan_tci;  /**< Priority (3) + CFI (1) + Identifier Code (12) */
	rte_be16_t eth_proto; /**< Ethernet type of encapsulated frame. */
} __rte_packed;



/* Ethernet frame types */ //以太网帧类型
#define RTE_ETHER_TYPE_IPV4 0x0800 /**< IPv4 Protocol. */
#define RTE_ETHER_TYPE_IPV6 0x86DD /**< IPv6 Protocol. */
#define RTE_ETHER_TYPE_ARP  0x0806 /**< Arp Protocol. */
#define RTE_ETHER_TYPE_RARP 0x8035 /**< Reverse Arp Protocol. */
#define RTE_ETHER_TYPE_VLAN 0x8100 /**< IEEE 802.1Q VLAN tagging. */
#define RTE_ETHER_TYPE_QINQ 0x88A8 /**< IEEE 802.1ad QinQ tagging. */
#define RTE_ETHER_TYPE_QINQ1 0x9100 /**< Deprecated QinQ VLAN. */
#define RTE_ETHER_TYPE_QINQ2 0x9200 /**< Deprecated QinQ VLAN. */
#define RTE_ETHER_TYPE_QINQ3 0x9300 /**< Deprecated QinQ VLAN. */
#define RTE_ETHER_TYPE_PPPOE_DISCOVERY 0x8863 /**< PPPoE Discovery Stage. */
#define RTE_ETHER_TYPE_PPPOE_SESSION 0x8864 /**< PPPoE Session Stage. */
#define RTE_ETHER_TYPE_ETAG 0x893F /**< IEEE 802.1BR E-Tag. */
#define RTE_ETHER_TYPE_1588 0x88F7
	/**< IEEE 802.1AS 1588 Precise Time Protocol. */
#define RTE_ETHER_TYPE_SLOW 0x8809 /**< Slow protocols (LACP and Marker). */
#define RTE_ETHER_TYPE_TEB  0x6558 /**< Transparent Ethernet Bridging. */
#define RTE_ETHER_TYPE_LLDP 0x88CC /**< LLDP Protocol. */
#define RTE_ETHER_TYPE_MPLS 0x8847 /**< MPLS ethertype. */
#define RTE_ETHER_TYPE_MPLSM 0x8848 /**< MPLS multicast ethertype. */
#define RTE_ETHER_TYPE_ECPRI 0xAEFE /**< eCPRI ethertype (.1Q supported). */

/**
 * Extract VLAN tag information into mbuf 从mbuf里删除vlan头
 *
 * Software version of VLAN stripping
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   - 0: Success
 *   - 1: not a vlan packet
 */
static inline int rte_vlan_strip(struct rte_mbuf *m)
{
	struct rte_ether_hdr *eh
		 = rte_pktmbuf_mtod(m, struct rte_ether_hdr *); //找到eth头
	struct rte_vlan_hdr *vh;

	if (eh->ether_type != rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN)) //检测类型
		return -1;

	vh = (struct rte_vlan_hdr *)(eh + 1); //eth头就是vlan头
	m->ol_flags |= RTE_MBUF_F_RX_VLAN | RTE_MBUF_F_RX_VLAN_STRIPPED;//增加vlan卸载标识
	m->vlan_tci = rte_be_to_cpu_16(vh->vlan_tci); //保存vlan id

	/* Copy ether header over rather than moving whole packet *///eth头后移
	//调用rte_pktmbuf_adj调整数据长度，然后将eth头往后复制12字节
	memmove(rte_pktmbuf_adj(m, sizeof(struct rte_vlan_hdr)),
		eh, 2 * RTE_ETHER_ADDR_LEN);

	return 0;
}

/**
 * Insert VLAN tag into mbuf.
 *
 * Software version of VLAN unstripping
 *
 * @param m
 *   The packet mbuf.
 * @return
 *   - 0: On success
 *   -EPERM: mbuf is is shared overwriting would be unsafe
 *   -ENOSPC: not enough headroom in mbuf
 */
static inline int rte_vlan_insert(struct rte_mbuf **m)
{
	struct rte_ether_hdr *oh, *nh;
	struct rte_vlan_hdr *vh;

	/* Can't insert header if mbuf is shared */
    //RTE_MBUF_DIRECT 确保不是引用型报文，即只有mbuf头，没实际data区域
    //rte_mbuf_refcnt_read 检测引用技术，避免在引用时，被修改
	if (!RTE_MBUF_DIRECT(*m) || rte_mbuf_refcnt_read(*m) > 1)
		return -EINVAL;

	/* Can't insert header if the first segment is too short */
	if (rte_pktmbuf_data_len(*m) < 2 * RTE_ETHER_ADDR_LEN)
		return -EINVAL;

	oh = rte_pktmbuf_mtod(*m, struct rte_ether_hdr *);
	nh = (struct rte_ether_hdr *)(void *)
		rte_pktmbuf_prepend(*m, sizeof(struct rte_vlan_hdr));
	if (nh == NULL)
		return -ENOSPC;

	memmove(nh, oh, 2 * RTE_ETHER_ADDR_LEN);
	nh->ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);

	vh = (struct rte_vlan_hdr *) (nh + 1);
	vh->vlan_tci = rte_cpu_to_be_16((*m)->vlan_tci);
    //去掉vlan下载标识
	(*m)->ol_flags &= ~(RTE_MBUF_F_RX_VLAN_STRIPPED | RTE_MBUF_F_TX_VLAN);
    //是否有隧道
	if ((*m)->ol_flags & RTE_MBUF_F_TX_TUNNEL_MASK)
		(*m)->outer_l2_len += sizeof(struct rte_vlan_hdr);
	else
		(*m)->l2_len += sizeof(struct rte_vlan_hdr);

	return 0;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHER_H_ */
