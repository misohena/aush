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
	typedef char NativeChar; ///< �v���b�g�t�H�[���ɂ�����W���I�ȕ����^�ł��B
	typedef std::string NativeString; ///< std::basic_string<NativeChar>�ł��B
	typedef wchar_t NonNativeChar;
	typedef std::wstring NonNativeString;
#else
	typedef wchar_t NativeChar;
	typedef std::wstring NativeString;
	typedef char NonNativeChar;
	typedef std::string NonNativeString;
#endif

/*
 * AUSH_NATIVE_L�}�N���́A�������e�����╶���񃊃e������
 * �v���b�g�t�H�[���l�C�e�B�u�ȕ\���֕ϊ����܂��B
 * AUSH_NATIVE_L("Hello")�̂悤�ɂ����ƁAwchar_t���l�C�e�B�u�Ȋ��ł�L"Hello"�ɂȂ�܂��B
 */
#ifdef _MBCS
# define AUSH_NATIVE_L(x) x
#else
# define AUSH_NATIVE_L(x) L##x
#endif

// --------------------------------------------------------------------------
// ��������
// --------------------------------------------------------------------------

int getNativeCharLength(const NativeChar *p);
int getNativeCharLength(const NativeChar *p, const NativeChar *end);

}//namespace aush

#endif
