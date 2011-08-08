/**
 * @file
 * @author AKIYAMA Kouhei
 * @since 2011-08-05
 */
#include <windows.h>
#include "NativeString.h"

namespace aush {

// --------------------------------------------------------------------------
// ��������
// --------------------------------------------------------------------------

/**
 * 1�����̒��������߂܂��B
 */
int getNativeCharLength(const NativeChar *p)
{
	// 2�o�C�g�ڂ��͈͊O�̏ꍇ�ɂǂ����邩�B1�o�C�g�Ƃ���ׂ��H
	// �Œ���A2�o�C�g�ڂ�NULL�������X�L�b�v���Ă��܂�Ȃ��悤�ɂ���B
	// NULL�����͒���1�B����0���Ƃ͂��Ȃ��BNULL�I�[������Ƃ͌���Ȃ��̂ŁB
#ifdef _MBCS
	return (::IsDBCSLeadByte(p[0]) && p[1] != '\0') ? 2 : 1;
#else //NativeChar = UTF-16
	return (IS_HIGH_SURROGATE(p[0]) && IS_LOW_SURROGATE(p[1])) ? 2 : 1;
#endif
	// ��p[0]���������Ȃ���p[1]�փA�N�Z�X���Ȃ����Ƃɒ��ӁB
}

int getNativeCharLength(const NativeChar *p, const NativeChar *end)
{
	return p >= end ? 0
		: p + 1 >= end ? 1
		: getNativeCharLength(p);
}

}//namespace aush
