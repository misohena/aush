/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2011-08-03
 */
#ifndef AUSH_BYTEWAVEREADER_H_INCLUDED_20110803
#define AUSH_BYTEWAVEREADER_H_INCLUDED_20110803

#include <boost/shared_ptr.hpp>
#include "WaveReader.h"

namespace aush{ namespace dsound{

	class ByteWaveReader
		: public WaveReader
	{
		const unsigned char * const bufferBegin_;
		const unsigned char * const bufferEnd_;
		const WaveFormat format_;
		const WaveSize total_;
		const unsigned char *rptr_;
		boost::shared_ptr<void> buffer_;
	public:
		ByteWaveReader(const unsigned char *begin, const unsigned char *end, const WaveFormat &format, const boost::shared_ptr<void> &buffer = boost::shared_ptr<void>());

		virtual void close(void);
		virtual bool isOpen(void) const;
		virtual bool read(void *dst, std::size_t size, std::size_t *actualSize);
		virtual bool seek(WaveSize sample);
		virtual WaveSize tell(void) const;
		virtual bool isTerminated(void) const;
	
		virtual WaveFormat getFormat(void) const;
		virtual WaveSize getTotal(void) const;
	};


}}//namespace aush::dsound
#endif
