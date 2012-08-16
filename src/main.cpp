/**
 * @file
 * @brief Audio Shell
 * @author AKIYAMA Kouhei
 * @since 2011-08-01
 */
#include <iostream>
#include <algorithm>
#include <vector>
#include <cctype>
#include "dsound/DirectSound.h"
#include "dsound/OggFileReader.h"
#include "dsound/PlainWavFileReader.h"
#include "dsound/PitchShiftWaveReader.h"
#include "dsound/StreamSoundBuffer.h"
#include "util/PathString.h"
#include "util/CmdLineSplitter.h"

namespace aush{

	typedef boost::shared_ptr<dsound::StreamSoundBuffer> BufferPtr;
	typedef boost::shared_ptr<dsound::WaveReader> WaveReaderPtr;

	std::string toLowerCase(const std::string &src)
	{
		std::string result;
		result.reserve(src.size());
		std::string::const_iterator endit = src.end();
		for(std::string::const_iterator it = src.begin(); it != endit; ++it){
			result.push_back(std::tolower(*it));
		}
		return result;
	}


	WaveReaderPtr openWaveReader(const PathString &filename)
	{
		const PathString ext = toLowerCase(getFileExtension(filename));

		if(ext == ".ogg"){
			boost::shared_ptr<dsound::OggFileReader> readerOgg(new dsound::OggFileReader());
			if(!readerOgg->open(filename.c_str())){
				std::cerr << "cannot open file." << std::endl;
				return WaveReaderPtr();
			}
			return readerOgg;
		}
		else if(ext == ".wav"){
			boost::shared_ptr<dsound::PlainWavFileReader> readerWav(new dsound::PlainWavFileReader());
			if(!readerWav->open(filename.c_str())){
				std::cerr << "cannot open file." << std::endl;
				return WaveReaderPtr();
			}
			return readerWav;
		}
		else{
			std::cerr << "unknown file extension." << std::endl;
			return WaveReaderPtr();
		}
	}

	typedef boost::shared_ptr<dsound::PitchShiftWaveReader> PitchShiftWaveReaderPtr;
	class Sound
	{
		PitchShiftWaveReaderPtr reader_;
		BufferPtr buffer_;
	public:
		Sound(const PitchShiftWaveReaderPtr &reader, const BufferPtr &buffer)
			: reader_(reader), buffer_(buffer)
		{}
		~Sound()
		{
			buffer_->stop();
		}

		void play(void) { buffer_->play();}
		void stop(void) { buffer_->stop();}
		void seekToBeginning(void) { buffer_->setPositionSamples(0);}
		void setPitchShift(float r)
		{
			if(buffer_->isPlaying()){
				buffer_->stop();
				const dsound::WaveSize pos = buffer_->getPositionSamples();
				reader_->setShiftRatio(r);
				buffer_->setPositionSamples(pos);
				buffer_->play();
			}
			else{
				reader_->setShiftRatio(r);
			}
		}
		void setResampleRatio(float r)
		{
			const bool playing = buffer_->isPlaying();
			if(playing){
				buffer_->stop();
			}

			const DWORD pos = static_cast<DWORD>(buffer_->getPositionSamples() / reader_->getResampleRatio());
			reader_->setResampleRatio(r);
			buffer_->updateSourceSize();
			buffer_->setPositionSamples(static_cast<DWORD>(pos * reader_->getResampleRatio()));

			if(playing){
				buffer_->play();
			}
		}
		void waitForStop(void)
		{
			buffer_->waitForStop();
		}
	};
	typedef boost::shared_ptr<Sound> SoundPtr;


	SoundPtr openSound(const PathString &filename,
		const dsound::DirectSound &ds,
		float speedRatio)
	{
		const WaveReaderPtr reader = openWaveReader(filename);
		if(!reader){
			return SoundPtr();
		}

		const PitchShiftWaveReaderPtr pitchShiftReader(new dsound::PitchShiftWaveReader(reader, 512, 4));
		pitchShiftReader->setShiftRatio(1.f/speedRatio);
		pitchShiftReader->setResampleRatio(1.f/speedRatio);

		const BufferPtr buffer(new dsound::StreamSoundBuffer());
		if(!buffer->create(ds, pitchShiftReader)){
			std::cerr << "failed to create a direct sound buffer." << std::endl;
			return SoundPtr();
		}

		const SoundPtr sound(new Sound(pitchShiftReader, buffer));

		return sound;
	}




	int main()
	{
		dsound::DirectSound ds;
		if(!ds.create(::GetDesktopWindow())){
			std::cerr << "Failed to initialize DirectSound." << std::endl;
			return -1;
		}

		SoundPtr sound;
		float speedRatio = 1.0f;

		for(;;){
			std::string linestr;
			std::getline(std::cin, linestr);
			if(!std::cin){
				break;
			}
			std::vector<NativeString> args = splitCmdLineArgs(linestr.c_str());
			if(args.empty()){
				continue;
			}

			if(args[0] == "q" || args[0] == "quit" || args[0] == "exit" || args[0] == "bye"){
				break;
			}
			else if(args[0] == "open" && args.size() >= 2){
				if(SoundPtr newSound = openSound(args[1], ds, speedRatio)){
					sound = newSound; //stop old sound buffer & release.
				}
			}
			else if(args[0] == "close"){
				sound.reset();
			}
			else if(args[0] == "play"){
				if(sound){
					sound->play();
				}
			}
			else if(args[0] == "stop"){
				if(sound){
					sound->stop();
				}
			}
			else if(args[0] == "top" || args[0] == "begin"){
				if(sound){
					sound->seekToBeginning();
				}
			}
			else if(args[0] == "speed" && args.size() >= 2){
				const double r = std::strtod(args[1].c_str(), NULL);
				if(r >= 0.5 && r <= 2.0){
					speedRatio = static_cast<float>(r);
					if(sound){
						sound->setPitchShift(1.0f/speedRatio);
						sound->setResampleRatio(1.0f/speedRatio);
					}
				}
			}
			else if(args[0] == "sync"){
				if(sound){
					sound->waitForStop();
				}
			}
		}
		return 0;
	}
}//namespace aush

int main(int argc, char *argv[])
{
	::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	const int exitcode = aush::main();
	::CoUninitialize();
	return exitcode;
}
