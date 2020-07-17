/*
 * RtcpPacketImp.h
 *
 *  Created on: 2020/07/15
 *      Author: max
 *		Email: Kingsleyyau@gmail.com
 */

#ifndef RTP_PACKET_RTCPPACKETIMP_H_
#define RTP_PACKET_RTCPPACKETIMP_H_

#include <stddef.h>
#include <stdint.h>

#include "CommonHeader.h"

#include <base/Check.h>

#include <common/LogManager.h>

namespace mediaserver {
class RtcpPacketImp {
public:
	// Size of the rtcp common header.
	static constexpr size_t kHeaderLength = 4;

	RtcpPacketImp() = default;
	virtual ~RtcpPacketImp() = default;

	// Size of this packet in bytes (including headers).
	virtual size_t BlockLength() const = 0;

	// Creates packet in the given buffer at the given position.
	// Calls PacketReadyCallback::OnPacketReady if remaining buffer is too small
	// and assume buffer can be reused after OnPacketReady returns.
	virtual bool Create(uint8_t* packet, size_t* index, size_t max_length) const = 0;

	static void CreateHeader(size_t count_or_format, uint8_t packet_type,
			size_t block_length,  // Payload size in 32bit words.
			uint8_t* buffer, size_t* pos);

	static void CreateHeader(size_t count_or_format, uint8_t packet_type,
			size_t block_length,  // Payload size in 32bit words.
			bool padding,  // True if there are padding bytes.
			uint8_t* buffer, size_t* pos);

	// Size of the rtcp packet as written in header.
	size_t HeaderLength() const;

	uint32_t sender_ssrc_ = 0;
};

} /* namespace mediaserver */

#endif /* RTP_PACKET_RTCPPACKETIMP_H_ */
