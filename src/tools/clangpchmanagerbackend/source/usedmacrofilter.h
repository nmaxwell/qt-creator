/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#pragma once

#include "usedmacro.h"
#include "sourceentry.h"

#include <compilermacro.h>

namespace ClangBackEnd {

class UsedMacroFilter
{
public:
    struct Includes
    {
        FilePathIds project;
        FilePathIds system;
    };

    UsedMacroFilter(const SourceEntries &includes, const UsedMacros &usedMacros)
    {
        filterIncludes(includes);
        systemUsedMacros = filterUsedMarcos(usedMacros, systemIncludes);
        projectUsedMacros = filterUsedMarcos(usedMacros, projectIncludes);
    }

    void filterIncludes(const SourceEntries &includes)
    {
        systemIncludes.reserve(includes.size());
        projectIncludes.reserve(includes.size());

        for (SourceEntry include : includes)
            filterInclude(include);
    }

    void filter(const CompilerMacros &compilerMacros)
    {
        CompilerMacros indexedCompilerMacro  = compilerMacros;

        std::sort(indexedCompilerMacro.begin(),
                  indexedCompilerMacro.end(),
                  [](const CompilerMacro &first, const CompilerMacro &second) {
                      return std::tie(first.key, first.value) < std::tie(second.key, second.value);
                  });

        systemCompilerMacros = filtercompilerMacros(indexedCompilerMacro, systemUsedMacros);
        projectCompilerMacros = filtercompilerMacros(indexedCompilerMacro, projectUsedMacros);
    }

private:
    void filterInclude(SourceEntry include)
    {
        switch (include.sourceType) {
        case SourceType::TopSystemInclude:
        case SourceType::SystemInclude:
            systemIncludes.emplace_back(include.sourceId);
            break;
        case SourceType::TopProjectInclude:
        case SourceType::ProjectInclude:
            projectIncludes.emplace_back(include.sourceId);
            break;
        case SourceType::UserInclude:
            break;
        }
    }

    static UsedMacros filterUsedMarcos(const UsedMacros &usedMacros, const FilePathIds &filePathId)
    {
        class BackInserterIterator : public std::back_insert_iterator<UsedMacros>
        {
        public:
            BackInserterIterator(UsedMacros &container)
                : std::back_insert_iterator<UsedMacros>(container)
            {}

            BackInserterIterator &operator=(const UsedMacro &usedMacro)
            {
                container->push_back(usedMacro);

                return *this;
            }

            BackInserterIterator &operator*() { return *this; }
        };

        struct Compare
        {
            bool operator()(const UsedMacro &usedMacro, FilePathId filePathId)
            {
                return usedMacro.filePathId < filePathId;
            }

            bool operator()(FilePathId filePathId, const UsedMacro &usedMacro)
            {
                return filePathId < usedMacro.filePathId;
            }
        };

        UsedMacros filtertedMacros;
        filtertedMacros.reserve(usedMacros.size());

        std::set_intersection(usedMacros.begin(),
                              usedMacros.end(),
                              filePathId.begin(),
                              filePathId.end(),
                              BackInserterIterator(filtertedMacros),
                              Compare{});

        std::sort(filtertedMacros.begin(),
                  filtertedMacros.end(),
                  [](const UsedMacro &first, const UsedMacro &second) {
                      return first.macroName < second.macroName;
                  });

        return filtertedMacros;
    }

    static CompilerMacros filtercompilerMacros(const CompilerMacros &indexedCompilerMacro,
                                               const UsedMacros &usedMacros)
    {
        struct Compare
        {
            bool operator()(const UsedMacro &usedMacro,
                            const CompilerMacro &compileMacro)
            {
                return usedMacro.macroName < compileMacro.key;
            }

            bool operator()(const CompilerMacro &compileMacro,
                            const UsedMacro &usedMacro)
            {
                return compileMacro.key < usedMacro.macroName;
            }
        };

        CompilerMacros filtertedCompilerMacros;
        filtertedCompilerMacros.reserve(indexedCompilerMacro.size());

        std::set_intersection(indexedCompilerMacro.begin(),
                              indexedCompilerMacro.end(),
                              usedMacros.begin(),
                              usedMacros.end(),
                              std::back_inserter(filtertedCompilerMacros),
                              Compare{});

        return filtertedCompilerMacros;
    }

public:
    FilePathIds projectIncludes;
    FilePathIds systemIncludes;
    UsedMacros projectUsedMacros;
    UsedMacros systemUsedMacros;
    CompilerMacros projectCompilerMacros;
    CompilerMacros systemCompilerMacros;
};

} // namespace ClangBackEnd
