/**
 * @file
 * @brief パス文字列操作に関する実装ファイルです。
 * @author AKIYAMA Kouhei
 * @since 2008-07-28
 */

#include <windows.h>
#include "PathString.h"


namespace {
using namespace aush;

// ------------------------------------------------------------------
// MBCS用文字処理関数
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
 * 指定された文字の位置を求めます。
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
			if(++it == str.end()){//1バイト余分に消費
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
 * 最後にある指定された文字の位置を求めます。
 */
template<typename Pred>
std::size_t find_last_char(const NativeString &str, Pred pred)
{
	std::size_t last_pos = NativeString::npos;
	for(NativeString::const_iterator it = str.begin(); it != str.end(); ++it){
#ifdef _MBCS
		if(::IsDBCSLeadByte(*it)){
			if(++it == str.end()){//1バイト余分に消費
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
 * 文字列中の次の文字の位置を返します。
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


}//無名



namespace aush{


/**
 * 最後のファイル名部分の先頭の位置を返します。
 */
PathString::size_type getLastFileNamePos(const PathString &s)
{
	// 完全修飾パス
	// \\Server\a\b => b
	// \\Server\a\ => (empty)
	// \\Server\a => a
	// \\Server\ => (empty)
	// \\Server => \\Server  (*特殊)
	//
	// \\?\C:\a => a
	// \\?\C:\ => (empty)
	// \\?\C: => C:
	// \\?\ => (empty)
	// \\? => \\?  (*特殊)
	//
	// (\\?\\\Server\aみたいなのはあるの？)
	//
	// C:\a\b => b
	// C:\a\ => (empty)
	// C:\a => a
	// C:\ => (empty)
	//
	// 非完全修飾パス
	// \\?\C:a => a (*特殊? 許可されていないっぽいけど)
	//
	// \a => a
	// \ => (empty)
	//
	// C:a\b => b
	// C:a\ => (empty)
	// C:a => a  (*特殊)
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
		return s.size(); //区切り文字で終わっている。または、文字列が空。
	}

	// 特殊な物を処理する。
	// ・ \\Server
	// ・ C:a
	// ・ C:
	if(lastSep == 1 && is_separator()(s[0])){
		return 0; // \\で始まり以降区切りが無い場合、全体をひとまとまりとする。
	}
	if(lastSep == PathString::npos && s.size() >= 3 && s[1] == AUSH_NATIVE_L(':')){ //PRN:とかのことを考えると微妙だけど、splitpathあたりもこれと等しい感じだし(_MAX_DRIVE=3)、まあいいか。
		return 2; //コロン以降のみがファイル名。
	}

	return afterSep;
}

/**
 * パス末尾のファイル名部分を返します。
 * 最後のパス区切り文字以降を返します。
 */
PathString getLastFileName(const PathString &s)
{
	return PathString(s, getLastFileNamePos(s));
}


/**
 * パス末尾のファイル名部分を切り落とした文字列を返します。
 *
 * 最後のパス区切り文字は残します。入力文字列の最後がパス区切り文字で終わっている場合は、入力文字列をそのまま返します。
 */
PathString chopLastFileName(const PathString &s)
{
	return PathString(s, 0, getLastFileNamePos(s));
}


/**
 * 文字列が余分なセパレータで終わっているかどうかを返します。
 */
bool isTerminatedByRedundantSeparator(const PathString &s)
{
	// 完全修飾パス
	// \\Server\a\ => true?
	// \\Server\ => true?
	// \\?\C:\ => false
	// \\?\ => false
	// C:\a\ => true
	// C:\ => false
	//
	// 非完全修飾パス
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
 * パス末尾の余分なセパレータを切り落とした文字列を返します。
 *
 * 切り落とすのは余分なセパレータであり、切り落とすことによって意味が変わってしまう場合は切り落としません。
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
 * ファイルの拡張子(ドットを含む)を返します。
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
 * フルパス名を求めます。
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
 * フルパス名のファイル部分を除いた物を求めます。
 *
 * chopLastFileName(getFullPath(s))とほぼ同じだと思います。
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
		return PathString(buffer); // sがパス区切りで終わっていたり、//serverや//server/shareのようなUNC先頭。
	}
	else{
		return PathString(buffer, filePart); //ファイル部の直前までを返す。
	}
}
#endif


}//namespace aush
