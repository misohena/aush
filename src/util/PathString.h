/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2008-07-28
 */
#ifndef AUSH_PATHSTRING_H_INCLUDED
#define AUSH_PATHSTRING_H_INCLUDED

#include "NativeString.h"

namespace aush{

	typedef NativeChar PathChar; ///< �t�@�C���V�X�e����̃p�X��\������Ƃ��Ɏg�������ł��B
	typedef NativeString PathString; ///< �t�@�C���V�X�e����̃p�X��\������Ƃ��Ɏg��������ł��B

	const unsigned int DEFAULT_PATH_CHARS = 4+256; //MAX_PATH(D:\<256 char><\0>)�BUnicode�ł̏ꍇ�͍ő�32768�����܂ł����\��������B


	PathString::size_type getLastFileNamePos(const PathString &s);
	PathString getLastFileName(const PathString &s);
	PathString chopLastFileName(const PathString &s);
	bool isTerminatedByRedundantSeparator(const PathString &s);
	PathString chopLastRedundantSeparator(const PathString &s);
	PathString getFileExtension(const PathString &s);


#if !defined(UNDER_CE)
	PathString getFullPath(const PathString &s);
	PathString getFullPathDir(const PathString &s);
#endif

}//namespace aush
#endif
