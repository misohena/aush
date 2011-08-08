/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2011-08-05
 */
#ifndef AUSH_PITCHSHIFTWAVEREADER_H_INCLUDED_20110805
#define AUSH_PITCHSHIFTWAVEREADER_H_INCLUDED_20110805

#include <vector>
#include <boost/shared_ptr.hpp>
#include "WaveReader.h"
#include "../audioproc/Resample.h"

struct SmbPitchShift;

namespace aush{namespace dsound{

	class PitchShiftWaveReader
		: public WaveReader
	{
		const boost::shared_ptr<WaveReader> source_;
		const WaveFormat format_;
		const int fftFrameSize_;
		const int fftOverlapFactor_;
		
		std::vector<boost::shared_ptr<SmbPitchShift> > shifters_;
		boost::shared_ptr<NearestResampler<unsigned int> > resampler_;
		float shiftRatio_;
		float resampleRatio_;
		WaveSize sourceNumSamples_;
		WaveSize outputNumSamples_;
		WaveSize outputPos_;
	public:
		PitchShiftWaveReader(
				const boost::shared_ptr<WaveReader> &source,
				int fftFrameSize,
				int fftOverlapFactor);

		void setShiftRatio(float shiftRatio);
		void setResampleRatio(float resampleRatio);

		inline float getShiftRatio(void) const { return shiftRatio_;}
		inline float getResampleRatio(void) const { return resampleRatio_;}

		virtual void close(void);
		virtual bool isOpen(void) const;
		virtual bool read(void *dst, std::size_t size, std::size_t *actualSize);
		virtual bool seek(WaveSize sample);
		virtual WaveSize tell(void) const;
		virtual bool isTerminated(void) const;
	
		virtual WaveFormat getFormat(void) const;
		virtual WaveSize getTotal(void) const;
	private:
		void initShifter(void);
	};


}}//namespace aush::dsound
#endif
