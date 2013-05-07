#pragma once

// STL Header
#include <map>
#include <set>
#include <string>
#include <vector>

class BookIndex
{
public:
	std::string	m_sTitle;
	std::string	m_sAuthor;

	std::vector< std::pair<std::string,std::string> > m_vChapter;
};

class Wenku8Cn
{
public:
	Wenku8Cn();

	bool CheckServer( const std::string& rServer );

	std::vector<BookIndex> AnalyzeIndexPage( std::string& rHtmlContent );

protected:
	std::set<std::string> m_SiteList;

	std::pair<std::string,std::string>	m_TitleTag;
	std::pair<std::string,std::string>	m_AuthorTag;
	std::pair<std::string,std::string>	m_BookTitleTag;
	std::pair<std::string,std::string>	m_BookChapterTag;
	std::pair<std::string,std::string>	m_ContentTag;
};

