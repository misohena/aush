/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2011-08-05
 */
#ifndef AUSH_NATIVESTRING_H_INCLUDED
#define AUSH_NATIVESTRING_H_INCLUDED

#include <cstdlib>
#include <string>

namespace aush {
#ifdef _MBCS
	typedef char NativeChar; ///< プラットフォームにおける標準的な文字型です。
	typedef std::string NativeString; ///< std::basic_string<NativeChar>です。
	typedef wchar_t NonNativeChar;
	typedef std::wstring NonNativeString;
#else
	typedef wchar_t NativeChar;
	typedef std::wstring NativeString;
	typedef char NonNativeChar;
	typedef std::string NonNativeString;
#endif

/*
 * AUSH_NATIVE_Lマクロは、文字リテラルや文字列リテラルを
 * プラットフォームネイティブな表現へ変換します。
 * AUSH_NATIVE_L("Hello")のようにつかうと、wchar_tがネイティブな環境ではL"Hello"になります。
 */
#ifdef _MBCS
# define AUSH_NATIVE_L(x) x
#else
# define AUSH_NATIVE_L(x) L##x
#endif

// --------------------------------------------------------------------------
// 文字操作
// --------------------------------------------------------------------------

int getNativeCharLength(const NativeChar *p);
int getNativeCharLength(const NativeChar *p, const NativeChar *end);

}//namespace aush

#endif
