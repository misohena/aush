/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2011-08-03
 */
#ifndef AUSH_RESAMPLING_H_INCLUDED
#define AUSH_RESAMPLING_H_INCLUDED

#include <boost/cstdint.hpp>

namespace aush{

template<typename T>
class NearestResampler
{
	typedef boost::uint64_t SizeType;

	T inputLastData_;
	SizeType inputPosNumer_;
	const SizeType inputPosDenom_;
	const SizeType inputPosStep_;
public:
	NearestResampler(SizeType inputPosStep,
					 SizeType inputPosDenom)
			: inputLastData_()
			, inputPosNumer_(inputPosDenom + inputPosStep / 2)
			, inputPosDenom_(inputPosDenom)
			, inputPosStep_(inputPosStep)
	{}

	template<typename OutIt, typename InIt>
	void resample(OutIt outputFirst, OutIt outputLast, InIt inputFirst, InIt inputLast)
	{
		InIt inputPtr = inputFirst;
		for(OutIt outputPtr = outputFirst; outputPtr != outputLast; ++outputPtr){
			while(inputPosNumer_ >= inputPosDenom_){
				inputPosNumer_ -= inputPosDenom_;
				if(inputPtr != inputLast){
					inputLastData_ = *inputPtr;
					++inputPtr;
				}
			}
			*outputPtr = inputLastData_;
			inputPosNumer_ += inputPosStep_;
		}
	}

	SizeType calcRequiredInputSize(SizeType outputSize) const
	{
		///@todo 割り算で計算する。オーバーフローに注意。
		SizeType inputCount = 0;
		SizeType inputPosNumer = inputPosNumer_;
		while(outputSize--){
			while(inputPosNumer >= inputPosDenom_){
				inputPosNumer -= inputPosDenom_;
				++inputCount;
			}
			inputPosNumer += inputPosStep_;
		}
		return inputCount;
	}

	SizeType calcMaxOutputSize(SizeType inputSize) const
	{
		///@todo 割り算で計算する。オーバーフローに注意。
		SizeType outputCount = 0;
		SizeType inputPosNumer = inputPosNumer_;
		for(;;){
			while(inputPosNumer >= inputPosDenom_){
				inputPosNumer -= inputPosDenom_;
				if(inputSize == 0){
					break;
				}
				--inputSize;
			}
			++outputCount;
			inputPosNumer += inputPosStep_;
		}
		return outputCount;
	}

	SizeType incrementOutput(SizeType outputSize)
	{
		///@todo 割り算で計算する。オーバーフローに注意。
		SizeType inputCount = 0;
		while(outputSize--){
			while(inputPosNumer_ >= inputPosDenom_){
				inputPosNumer_ -= inputPosDenom_;
				++inputCount;
				//inputLastData_ =
			}
			inputPosNumer_ += inputPosStep_;
		}
		return inputCount;
	}
};

}//namespace aush
#endif
