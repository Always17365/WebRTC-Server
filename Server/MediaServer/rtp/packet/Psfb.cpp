/*
 * Psfb.cpp
 *
 *  Created on: 2020/07/15
 *      Author: max
 *		Email: Kingsleyyau@gmail.com
 */

#include "Psfb.h"

namespace mediaserver {
// RFC 4585: Feedback format.
//
// Common packet format:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P|   FMT   |       PT      |          length               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 0 |                  SSRC of packet sender                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// 4 |                  SSRC of media source                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   :            Feedback Control Information (FCI)                 :
//   :                                                               :

void Psfb::ParseCommonFeedback(const uint8_t* payload) {
	sender_ssrc_ = (ByteReader<uint32_t>::ReadBigEndian(&payload[0]));
	media_ssrc_ = (ByteReader<uint32_t>::ReadBigEndian(&payload[4]));
}

void Psfb::CreateCommonFeedback(uint8_t* payload) const {
	ByteWriter<uint32_t>::WriteBigEndian(&payload[0], sender_ssrc_);
	ByteWriter<uint32_t>::WriteBigEndian(&payload[4], media_ssrc_);
}

} /* namespace mediaserver */
