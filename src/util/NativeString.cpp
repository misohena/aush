/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2011-08-05
 */
#include <windows.h>
#include "NativeString.h"

namespace aush {

// --------------------------------------------------------------------------
// 文字操作
// --------------------------------------------------------------------------

/**
 * 1文字の長さを求めます。
 */
int getNativeCharLength(const NativeChar *p)
{
	// 2バイト目が範囲外の場合にどうするか。1バイトとするべき？
	// 最低限、2バイト目のNULL文字をスキップしてしまわないようにする。
	// NULL文字は長さ1。長さ0等とはしない。NULL終端文字列とは限らないので。
#ifdef _MBCS
	return (::IsDBCSLeadByte(p[0]) && p[1] != '\0') ? 2 : 1;
#else //NativeChar = UTF-16
	return (IS_HIGH_SURROGATE(p[0]) && IS_LOW_SURROGATE(p[1])) ? 2 : 1;
#endif
	// ↑p[0]が成立しないとp[1]へアクセスしないことに注意。
}

int getNativeCharLength(const NativeChar *p, const NativeChar *end)
{
	return p >= end ? 0
		: p + 1 >= end ? 1
		: getNativeCharLength(p);
}

}//namespace aush
