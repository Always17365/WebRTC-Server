/*
 * NtpTime.h
 *
 *  Created on: 2019/12/25
 *      Author: max
 *		Email: Kingsleyyau@gmail.com
 */

#ifndef RTP_NTPTIME_H_
#define RTP_NTPTIME_H_

#include <unistd.h>
#include <stdint.h>

namespace mediaserver {

class NtpTime {
public:
	NtpTime();
	virtual ~NtpTime();
	NtpTime(uint32_t seconds, uint32_t fractions);
	explicit NtpTime(uint64_t value);
	NtpTime(const NtpTime&) = default;

	NtpTime& operator=(const NtpTime&) = default;
	explicit operator uint64_t() const;

	void Set(uint32_t seconds, uint32_t fractions);
	void Reset();

	int64_t ToMs() const;

	// NTP standard (RFC1305, section 3.1) explicitly state value 0 is invalid.
	bool Valid() const;

	uint32_t seconds() const;
	uint32_t fractions() const;
private:
	uint64_t value_;
};

} /* namespace mediaserver */

#endif /* RTP_NTPTIME_H_ */
