/**
 * @file
 * @brief �R�}���h���C����͂Ɋւ�������t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2007-08-27
 */
#include "CmdLineSplitter.h"

namespace aush {

/**
 * �R�}���h���C���������g�[�N�����ɕ������܂��B
 * @param cmdLineStr �R�}���h���C��������ł��B
 * @return cmdLineStr�𕪊����Ăł���������z��ł��B
 */
std::vector<NativeString> splitCmdLineArgs(const NativeChar *cmdLineStr)
{
	std::vector<NativeString> args;

	NativeString token;
	bool insideQuoted = false;
	int insideMbcChar = 0;
	int continuousBackSlashCount = 0;
	int continuousDelimiterCount = 1;//�ŏ��͋�؂蕶����������̂Ƃ��Ĉ����B

	//�����������Ă�1�o�C�g�����J�ɏ��������Ă����B
	for(const NativeChar *p = cmdLineStr; *p != AUSH_NATIVE_L('\0'); p++){
		//�����ŕ����̎�ނ����S�ɔ��f���Ă��܂��B
		const NativeChar ch = *p;
		const bool chIsBackSlash = (ch == AUSH_NATIVE_L('\\') && !insideMbcChar);
		const bool chIsQuote = (ch == AUSH_NATIVE_L('"') && !insideMbcChar);
		const bool chIsSpace = ((ch == AUSH_NATIVE_L(' ') || ch == AUSH_NATIVE_L('\t')) && !insideMbcChar);
		const bool chIsDelimiter = (chIsSpace && !insideQuoted);
		const bool chIsNormalChar = (!chIsBackSlash && !chIsQuote && !chIsDelimiter);//�X�y�[�X�ł����Ă��N�H�[�g����Ă���Βʏ�̕����ɂȂ邱�Ƃɒ��ӁB

		//������ʂ��Ƃ̏���

		if(chIsDelimiter){
			if(continuousDelimiterCount == 0){
				args.push_back(token);
				token.clear();
			}
		}
		else if(chIsQuote){
			//�N�H�[�g�O�̃o�b�N�X���b�V����W�J����B
			token.append(continuousBackSlashCount / 2, AUSH_NATIVE_L('\\'));
			if((continuousBackSlashCount % 2) == 0){
				//�o�b�N�X���b�V���������Ȃ�]��̃o�b�N�X���b�V���������A�N�H�[�g�͒P�ƂŗL���B
				//�N�H�[�g���J�n����B
				insideQuoted = !insideQuoted;
			}
			else{
				//�o�b�N�X���b�V������Ȃ�]��̃o�b�N�X���b�V��������A�N�H�[�g�̓G�X�P�[�v�����B
				//�G�X�P�[�v���ꂽ�N�H�[�g���󂯓����B
				token += AUSH_NATIVE_L('"');
			}
		}
		else if(chIsNormalChar){
			//�o�b�N�X���b�V����W�J����B
			token.append(continuousBackSlashCount, AUSH_NATIVE_L('\\'));
			//���ʂ̕������󂯓����B
			token += ch;
		}

		//�}���`�o�C�g��������E�o
		if(insideMbcChar){
			--insideMbcChar;
		}
		//�}���`�o�C�g�����̒��ɓ��������B
		else{
			insideMbcChar = getNativeCharLength(p) - 1;
		}

		//�A������o�b�N�X���b�V���𐔂���B
		if(chIsBackSlash){
			continuousBackSlashCount++;
		}
		else{
			continuousBackSlashCount = 0;
		}

		//�A������f���~�^�𐔂���B
		if(chIsDelimiter){
			continuousDelimiterCount++;
		}
		else{
			continuousDelimiterCount = 0;
		}
	}

	//�������̃o�b�N�X���b�V������������B
	token.append(continuousBackSlashCount, AUSH_NATIVE_L('\\'));

	//�Ō�̃g�[�N����ǉ�����B
	if( ! token.empty() || insideQuoted){
		args.push_back(token);
	}

	//���ʂ̕ԋp
	return args;
}


}//namespace aush
