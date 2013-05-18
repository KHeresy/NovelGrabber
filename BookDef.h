#pragma once

// STL Header
#include <string>
#include <vector>

class BookIndex
{
public:
	std::wstring	m_sTitle;
	std::wstring	m_sAuthor;

	std::vector< std::pair<std::wstring,std::wstring> > m_vChapter;
};
