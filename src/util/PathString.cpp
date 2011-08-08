/**
 * @file
 * @brief �p�X�����񑀍�Ɋւ�������t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2008-07-28
 */

#include <windows.h>
#include "PathString.h"


namespace {
using namespace aush;

// ------------------------------------------------------------------
// MBCS�p���������֐�
// ------------------------------------------------------------------

template<NativeChar CH>
struct is_char_pred
{
	static const NativeChar CH_ = CH;
	bool operator()(NativeChar ch) const { return ch == CH_;}
};

template<NativeChar CH1, NativeChar CH2>
struct is_char_or_char_pred
{
	static const NativeChar CH1_ = CH1;
	static const NativeChar CH2_ = CH2;
	bool operator()(NativeChar ch) const { return ch == CH1_ || ch == CH2_;}
};

/**
 * �w�肳�ꂽ�����̈ʒu�����߂܂��B
 */
template<typename Pred>
std::size_t find_first_char(const NativeString &str, Pred pred, std::size_t pos = 0)
{
	if(str.empty() || pos >= str.size()){
		return NativeString::npos;
	}
	for(NativeString::const_iterator it = str.begin() + pos; it != str.end(); ++it){
#ifdef _MBCS
		if(::IsDBCSLeadByte(*it)){
			if(++it == str.end()){//1�o�C�g�]���ɏ���
				break;
			}
		}
		else{
			if(pred(*it)){
				return it - str.begin();
			}
		}
#else
		if(pred(*it)){
			return it - str.begin();
		}
#endif
	}
	return NativeString::npos;
}

/**
 * �Ō�ɂ���w�肳�ꂽ�����̈ʒu�����߂܂��B
 */
template<typename Pred>
std::size_t find_last_char(const NativeString &str, Pred pred)
{
	std::size_t last_pos = NativeString::npos;
	for(NativeString::const_iterator it = str.begin(); it != str.end(); ++it){
#ifdef _MBCS
		if(::IsDBCSLeadByte(*it)){
			if(++it == str.end()){//1�o�C�g�]���ɏ���
				break;
			}
		}
		else{
			if(pred(*it)){
				last_pos = it - str.begin();
			}
		}
#else
		if(pred(*it)){
			last_pos = it - str.begin();
		}
#endif
	}
	return last_pos;
}

/**
 * �����񒆂̎��̕����̈ʒu��Ԃ��܂��B
 */
std::size_t next_char_pos(const NativeString &str, std::size_t pos)
{
	if(pos >= str.size()){
		return str.size(); //end
	}
#ifdef _MBCS
	else if(::IsDBCSLeadByte(str[pos])){
		if(pos + 1 < str.size()){
			return pos + 2;
		}
		else{
			return pos + 1; //double-byte character is broken.
		}
	}
#endif
	else{
		return pos + 1;
	}
}


typedef is_char_or_char_pred<AUSH_NATIVE_L('\\'), AUSH_NATIVE_L('/')> is_separator;
typedef is_char_pred<AUSH_NATIVE_L('.')> is_dot;


}//����



namespace aush{


/**
 * �Ō�̃t�@�C���������̐擪�̈ʒu��Ԃ��܂��B
 */
PathString::size_type getLastFileNamePos(const PathString &s)
{
	// ���S�C���p�X
	// \\Server\a\b => b
	// \\Server\a\ => (empty)
	// \\Server\a => a
	// \\Server\ => (empty)
	// \\Server => \\Server  (*����)
	//
	// \\?\C:\a => a
	// \\?\C:\ => (empty)
	// \\?\C: => C:
	// \\?\ => (empty)
	// \\? => \\?  (*����)
	//
	// (\\?\\\Server\a�݂����Ȃ̂͂���́H)
	//
	// C:\a\b => b
	// C:\a\ => (empty)
	// C:\a => a
	// C:\ => (empty)
	//
	// �񊮑S�C���p�X
	// \\?\C:a => a (*����? ������Ă��Ȃ����ۂ�����)
	//
	// \a => a
	// \ => (empty)
	//
	// C:a\b => b
	// C:a\ => (empty)
	// C:a => a  (*����)
	// C: => C:
	//
	// .\a => a
	// .\ =>(empty)
	// . => .
	//
	// a/b => b
	// a\ => (empty)
	// a => a
	//
	// => (empty)

	const std::size_t lastSep = find_last_char(s, is_separator());
	const std::size_t afterSep = (lastSep == PathString::npos) ? 0 : lastSep + 1;
	if(afterSep >= s.size()){
		return s.size(); //��؂蕶���ŏI����Ă���B�܂��́A�����񂪋�B
	}

	// ����ȕ�����������B
	// �E \\Server
	// �E C:a
	// �E C:
	if(lastSep == 1 && is_separator()(s[0])){
		return 0; // \\�Ŏn�܂�ȍ~��؂肪�����ꍇ�A�S�̂��ЂƂ܂Ƃ܂�Ƃ���B
	}
	if(lastSep == PathString::npos && s.size() >= 3 && s[1] == AUSH_NATIVE_L(':')){ //PRN:�Ƃ��̂��Ƃ��l����Ɣ��������ǁAsplitpath�����������Ɠ�������������(_MAX_DRIVE=3)�A�܂��������B
		return 2; //�R�����ȍ~�݂̂��t�@�C�����B
	}

	return afterSep;
}

/**
 * �p�X�����̃t�@�C����������Ԃ��܂��B
 * �Ō�̃p�X��؂蕶���ȍ~��Ԃ��܂��B
 */
PathString getLastFileName(const PathString &s)
{
	return PathString(s, getLastFileNamePos(s));
}


/**
 * �p�X�����̃t�@�C����������؂藎�Ƃ����������Ԃ��܂��B
 *
 * �Ō�̃p�X��؂蕶���͎c���܂��B���͕�����̍Ōオ�p�X��؂蕶���ŏI����Ă���ꍇ�́A���͕���������̂܂ܕԂ��܂��B
 */
PathString chopLastFileName(const PathString &s)
{
	return PathString(s, 0, getLastFileNamePos(s));
}


/**
 * �����񂪗]���ȃZ�p���[�^�ŏI����Ă��邩�ǂ�����Ԃ��܂��B
 */
bool isTerminatedByRedundantSeparator(const PathString &s)
{
	// ���S�C���p�X
	// \\Server\a\ => true?
	// \\Server\ => true?
	// \\?\C:\ => false
	// \\?\ => false
	// C:\a\ => true
	// C:\ => false
	//
	// �񊮑S�C���p�X
	// \ => false
	// C:a\ => true
	// .\ => true
	// a\ => true

	std::size_t lastSepPos = s.size();
	NativeChar prevChar = AUSH_NATIVE_L('\0');
	NativeChar lastSepPrevChar = AUSH_NATIVE_L('\0');

	for(std::size_t pos = 0; pos < s.size(); pos = next_char_pos(s, pos)){
		NativeChar currChar = s[pos];
		if(is_separator()(currChar)){
			lastSepPos = pos;
			lastSepPrevChar = prevChar;
		}
		prevChar = currChar;
	}

	return (lastSepPos + 1 == s.size()
		&& lastSepPrevChar != AUSH_NATIVE_L(':')
		&& lastSepPrevChar != AUSH_NATIVE_L('?')
		&& lastSepPrevChar != AUSH_NATIVE_L('\0'));
}


/**
 * �p�X�����̗]���ȃZ�p���[�^��؂藎�Ƃ����������Ԃ��܂��B
 *
 * �؂藎�Ƃ��̂͗]���ȃZ�p���[�^�ł���A�؂藎�Ƃ����Ƃɂ���ĈӖ����ς���Ă��܂��ꍇ�͐؂藎�Ƃ��܂���B
 */
PathString chopLastRedundantSeparator(const PathString &s)
{
	if(isTerminatedByRedundantSeparator(s)){
		return PathString(s, 0, s.size() - 1);
	}
	else{
		return s;
	}
}

/**
 * �t�@�C���̊g���q(�h�b�g���܂�)��Ԃ��܂��B
 */
PathString getFileExtension(const PathString &s)
{
	const PathString filename = getLastFileName(s);
	if(filename.empty() || filename == AUSH_NATIVE_L(".") || filename == AUSH_NATIVE_L("..")){
		return PathString();
	}

	const std::size_t pos = find_last_char(filename, is_dot());
	if(pos >= filename.size()){
		return PathString();
	}

	return filename.substr(pos);
}


#if !defined(UNDER_CE)
/**
 * �t���p�X�������߂܂��B
 */
PathString getFullPath(const PathString &s)
{
	const DWORD bufferLen = MAX_PATH;
	TCHAR *filePart;
	TCHAR buffer[bufferLen + 1];
	DWORD len = GetFullPathName(s.c_str(), bufferLen, buffer, &filePart);
	if(len == 0 || len >= bufferLen){
		return PathString();
	}
	return PathString(buffer);
}

/**
 * �t���p�X���̃t�@�C���������������������߂܂��B
 *
 * chopLastFileName(getFullPath(s))�Ƃقړ������Ǝv���܂��B
 */
PathString getFullPathDir(const PathString &s)
{
	const DWORD bufferLen = MAX_PATH;
	TCHAR *filePart;
	TCHAR buffer[bufferLen + 1];
	DWORD len = GetFullPathName(s.c_str(), bufferLen, buffer, &filePart);
	if(len == 0 || len == bufferLen){
		return PathString();
	}

	if(filePart == NULL){
		return PathString(buffer); // s���p�X��؂�ŏI����Ă�����A//server��//server/share�̂悤��UNC�擪�B
	}
	else{
		return PathString(buffer, filePart); //�t�@�C�����̒��O�܂ł�Ԃ��B
	}
}
#endif


}//namespace aush
