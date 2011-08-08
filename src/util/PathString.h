/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2008-07-28
 */
#ifndef AUSH_PATHSTRING_H_INCLUDED
#define AUSH_PATHSTRING_H_INCLUDED

#include "NativeString.h"

namespace aush{

	typedef NativeChar PathChar; ///< ファイルシステム上のパスを表現するときに使う文字です。
	typedef NativeString PathString; ///< ファイルシステム上のパスを表現するときに使う文字列です。

	const unsigned int DEFAULT_PATH_CHARS = 4+256; //MAX_PATH(D:\<256 char><\0>)。Unicode版の場合は最大32768文字までいく可能性がある。


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
