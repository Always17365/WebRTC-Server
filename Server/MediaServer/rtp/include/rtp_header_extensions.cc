#include <rtp/include/rtp_header_extensions.h>

#include <string.h>

#include <cmath>
#include <limits>

//#include "modules/rtp_rtcp/include/rtp_cvo.h"
// TODO(bug:9855) Move kNoSpatialIdx from vp9_globals.h to common_constants
//#include "modules/video_coding/codecs/interface/common_constants.h"
//#include "modules/video_coding/codecs/vp9/include/vp9_globals.h"
#include <rtp/base/checks.h>
#include <rtp/base/byte_io.h>

namespace mediaserver {
// Absolute send time in RTP streams.
//
// The absolute send time is signaled to the receiver in-band using the
// general mechanism for RTP header extensions [RFC8285]. The payload
// of this extension (the transmitted value) is a 24-bit unsigned integer
// containing the sender's current time in seconds as a fixed point number
// with 18 bits fractional part.
//
// The form of the absolute send time extension block:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ID   | len=2 |              absolute send time               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr RTPExtensionType AbsoluteSendTime::kId;
constexpr uint8_t AbsoluteSendTime::kValueSizeBytes;
constexpr const char AbsoluteSendTime::kUri[];

bool AbsoluteSendTime::Parse(mediaserver::ArrayView<const uint8_t> data,
                             uint32_t* time_24bits) {
  if (data.size() != 3)
    return false;
  *time_24bits = ByteReader<uint32_t, 3>::ReadBigEndian(data.data());
  return true;
}

bool AbsoluteSendTime::Write(mediaserver::ArrayView<uint8_t> data,
                             uint32_t time_24bits) {
  RTC_DCHECK_EQ(data.size(), 3);
  RTC_DCHECK_LE(time_24bits, 0x00FFFFFF);
  ByteWriter<uint32_t, 3>::WriteBigEndian(data.data(), time_24bits);
  return true;
}

// TransportSequenceNumber
//
//   0                   1                   2
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | L=1   |transport-wide sequence number |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
constexpr RTPExtensionType TransportSequenceNumber::kId;
constexpr uint8_t TransportSequenceNumber::kValueSizeBytes;
constexpr const char TransportSequenceNumber::kUri[];

bool TransportSequenceNumber::Parse(mediaserver::ArrayView<const uint8_t> data,
                                    uint16_t* transport_sequence_number) {
  if (data.size() != kValueSizeBytes)
    return false;
  *transport_sequence_number = ByteReader<uint16_t>::ReadBigEndian(data.data());
  return true;
}

bool TransportSequenceNumber::Write(mediaserver::ArrayView<uint8_t> data,
                                    uint16_t transport_sequence_number) {
  RTC_DCHECK_EQ(data.size(), ValueSize(transport_sequence_number));
  ByteWriter<uint16_t>::WriteBigEndian(data.data(), transport_sequence_number);
  return true;
}

// TransportSequenceNumberV2
//
// In addition to the format used for TransportSequencNumber, V2 also supports
// the following packet format where two extra bytes are used to specify that
// the sender requests immediate feedback.
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |  ID   | L=3   |transport-wide sequence number |T|  seq count  |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |seq count cont.|
//  +-+-+-+-+-+-+-+-+
//
// The bit |T| determines whether the feedback should include timing information
// or not and |seq_count| determines how many packets the feedback packet should
// cover including the current packet. If |seq_count| is zero no feedback is
// requested.
constexpr RTPExtensionType TransportSequenceNumberV2::kId;
constexpr uint8_t TransportSequenceNumberV2::kValueSizeBytes;
constexpr uint8_t
    TransportSequenceNumberV2::kValueSizeBytesWithoutFeedbackRequest;
constexpr const char TransportSequenceNumberV2::kUri[];
constexpr uint16_t TransportSequenceNumberV2::kIncludeTimestampsBit;

bool TransportSequenceNumberV2::Parse(
    mediaserver::ArrayView<const uint8_t> data,
    uint16_t* transport_sequence_number,
    absl::optional<FeedbackRequest>* feedback_request) {
  if (data.size() != kValueSizeBytes &&
      data.size() != kValueSizeBytesWithoutFeedbackRequest)
    return false;

  *transport_sequence_number = ByteReader<uint16_t>::ReadBigEndian(data.data());

  *feedback_request = absl::nullopt;
  if (data.size() == kValueSizeBytes) {
    uint16_t feedback_request_raw =
        ByteReader<uint16_t>::ReadBigEndian(data.data() + 2);
    bool include_timestamps =
        (feedback_request_raw & kIncludeTimestampsBit) != 0;
    uint16_t sequence_count = feedback_request_raw & ~kIncludeTimestampsBit;

    // If |sequence_count| is zero no feedback is requested.
    if (sequence_count != 0) {
      *feedback_request = {include_timestamps, sequence_count};
    }
  }
  return true;
}

bool TransportSequenceNumberV2::Write(
    mediaserver::ArrayView<uint8_t> data,
    uint16_t transport_sequence_number,
    const absl::optional<FeedbackRequest>& feedback_request) {
  RTC_DCHECK_EQ(data.size(),
                ValueSize(transport_sequence_number, feedback_request));

  ByteWriter<uint16_t>::WriteBigEndian(data.data(), transport_sequence_number);

  if (feedback_request) {
    RTC_DCHECK_GE(feedback_request->sequence_count, 0);
    RTC_DCHECK_LT(feedback_request->sequence_count, kIncludeTimestampsBit);
    uint16_t feedback_request_raw =
        feedback_request->sequence_count |
        (feedback_request->include_timestamps ? kIncludeTimestampsBit : 0);
    ByteWriter<uint16_t>::WriteBigEndian(data.data() + 2, feedback_request_raw);
  }
  return true;
}

}  // namespace mediaserver
