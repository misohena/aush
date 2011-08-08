/**
 * @file
 * @brief コマンドライン解析に関する実装ファイルです。
 * @author AKIYAMA Kouhei
 * @since 2007-08-27
 */
#include "CmdLineSplitter.h"

namespace aush {

/**
 * コマンドライン引数をトークン毎に分割します。
 * @param cmdLineStr コマンドライン文字列です。
 * @return cmdLineStrを分割してできた文字列配列です。
 */
std::vector<NativeString> splitCmdLineArgs(const NativeChar *cmdLineStr)
{
	std::vector<NativeString> args;

	NativeString token;
	bool insideQuoted = false;
	int insideMbcChar = 0;
	int continuousBackSlashCount = 0;
	int continuousDelimiterCount = 1;//最初は区切り文字があるものとして扱う。

	//効率が悪くても1バイトずつ丁寧に処理をしていく。
	for(const NativeChar *p = cmdLineStr; *p != AUSH_NATIVE_L('\0'); p++){
		//ここで文字の種類を完全に判断してしまう。
		const NativeChar ch = *p;
		const bool chIsBackSlash = (ch == AUSH_NATIVE_L('\\') && !insideMbcChar);
		const bool chIsQuote = (ch == AUSH_NATIVE_L('"') && !insideMbcChar);
		const bool chIsSpace = ((ch == AUSH_NATIVE_L(' ') || ch == AUSH_NATIVE_L('\t')) && !insideMbcChar);
		const bool chIsDelimiter = (chIsSpace && !insideQuoted);
		const bool chIsNormalChar = (!chIsBackSlash && !chIsQuote && !chIsDelimiter);//スペースであってもクォートされていれば通常の文字になることに注意。

		//文字種別ごとの処理

		if(chIsDelimiter){
			if(continuousDelimiterCount == 0){
				args.push_back(token);
				token.clear();
			}
		}
		else if(chIsQuote){
			//クォート前のバックスラッシュを展開する。
			token.append(continuousBackSlashCount / 2, AUSH_NATIVE_L('\\'));
			if((continuousBackSlashCount % 2) == 0){
				//バックスラッシュが偶数なら余りのバックスラッシュが無く、クォートは単独で有効。
				//クォートを開始する。
				insideQuoted = !insideQuoted;
			}
			else{
				//バックスラッシュが奇数なら余りのバックスラッシュがあり、クォートはエスケープされる。
				//エスケープされたクォートを受け入れる。
				token += AUSH_NATIVE_L('"');
			}
		}
		else if(chIsNormalChar){
			//バックスラッシュを展開する。
			token.append(continuousBackSlashCount, AUSH_NATIVE_L('\\'));
			//普通の文字を受け入れる。
			token += ch;
		}

		//マルチバイト文字から脱出
		if(insideMbcChar){
			--insideMbcChar;
		}
		//マルチバイト文字の中に入ったか。
		else{
			insideMbcChar = getNativeCharLength(p) - 1;
		}

		//連続するバックスラッシュを数える。
		if(chIsBackSlash){
			continuousBackSlashCount++;
		}
		else{
			continuousBackSlashCount = 0;
		}

		//連続するデリミタを数える。
		if(chIsDelimiter){
			continuousDelimiterCount++;
		}
		else{
			continuousDelimiterCount = 0;
		}
	}

	//未処理のバックスラッシュを処理する。
	token.append(continuousBackSlashCount, AUSH_NATIVE_L('\\'));

	//最後のトークンを追加する。
	if( ! token.empty() || insideQuoted){
		args.push_back(token);
	}

	//結果の返却
	return args;
}


}//namespace aush
