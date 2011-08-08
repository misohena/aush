/**
 * @file
 * @brief �R�}���h���C����͂Ɋւ���w�b�_�[�t�@�C���ł��B
 * @author AKIYAMA Kouhei
 * @since 2007-08-27
 */
#ifndef AUSH_CMDLINESPLITTER_H_INCLUDED_20070827
#define AUSH_CMDLINESPLITTER_H_INCLUDED_20070827

#include <vector>
#include <string>
#include "NativeString.h"

namespace aush {
    std::vector<NativeString> splitCmdLineArgs(const NativeChar *cmdLineStr);
}//namespace aush

#endif
