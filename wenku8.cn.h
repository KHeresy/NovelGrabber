#pragma once

// STL Header
#include <map>
#include <set>
#include <string>
#include <vector>

// Application header
#include "BookDef.h"

class Wenku8Cn
{
public:
	Wenku8Cn();

	bool CheckServer( const std::string& rServer );

	std::pair<std::wstring,std::vector<BookIndex>> AnalyzeIndexPage( std::wstring& rHtmlContent );

	std::wstring GetChapterContent( const std::wstring& rHTML );

	std::vector< std::pair<size_t,std::wstring> > FindAllImage( const std::wstring& rHTML, size_t uPos = 0 );

protected:
	std::set<std::string>	m_SiteList;

	std::wstring			m_wsAuthorPrefix;
	std::pair<std::wstring,std::wstring>	m_TitleTag;
	std::pair<std::wstring,std::wstring>	m_AuthorTag;
	std::pair<std::wstring,std::wstring>	m_BookTitleTag;
	std::pair<std::wstring,std::wstring>	m_BookChapterTag;
	std::pair<std::wstring,std::wstring>	m_ContentTag;
	std::pair<std::wstring,std::wstring>	m_ImageTag;
};

