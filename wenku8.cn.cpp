#include "wenku8.cn.h"
#include "HttpClient.h"

// Boost Header
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/locale.hpp>

using namespace std;
using namespace HttpClientLite;

Wenku8Cn::Wenku8Cn()
{
	m_SiteList = std::set<std::string>( { "www.wenku8.com", "www.wenku8.net" } );

	m_wsAuthorPrefix	= boost::locale::conv::utf_to_utf<wchar_t>( "\xE4\xBD\x9C\xE8\x80\x85\xEF\xBC\x9A" );
	m_TitleTag			= make_pair( L"<div id=\"title\">",					L"</div>" );
	m_AuthorTag			= make_pair( L"<div id=\"info\">",					L"</div>" );
	m_BookTitleTag		= make_pair( L"<td class=\"vcss\" colspan=\"4\">",	L"</td>" );
	m_BookChapterTag	= make_pair( L"<td class=\"ccss\">",				L"</td>" );
	m_ContentTag		= make_pair( L"<div id=\"content\">",				L"\n</div>" );
	m_ImageTag			= make_pair( L"<img src=\"",						L"\" border=\"0\" class=\"imagecontent\">" );

	m_vRemoveString = {
		L"<img src=\"http://img.wenku8.com/banner.jpg\" border=\"0\"/>"
	};
}

bool Wenku8Cn::CheckServer( const std::string& rServer )
{
	if( m_SiteList.find( rServer ) != m_SiteList.end() )
		return true;

	return false;
}

std::wstring Wenku8Cn::FilterHTML(const std::wstring& rHTML)
{
	std::wstring sFilterHTML = rHTML;
	for (auto sFilter : m_vRemoveString)
		boost::algorithm::replace_all(sFilterHTML, sFilter, "");

	return sFilterHTML;
}

std::pair<std::wstring,std::vector<BookIndex>> Wenku8Cn::AnalyzeIndexPage( std::wstring& rHtmlContent )
{
	wstring sTitle, sAuthor;

	// find title
	auto pTitle = HTMLParser::FindContentBetweenTag( rHtmlContent, m_TitleTag );
	if( pTitle.first != wstring::npos )
		sTitle	= pTitle.second;

	// find author
	auto	pAuthor = HTMLParser::FindContentBetweenTag( rHtmlContent, m_AuthorTag );
	sAuthor = pAuthor.second;
	if( sAuthor.length() > 3 && sAuthor.substr( 0, 3 ) == m_wsAuthorPrefix )
		sAuthor = sAuthor.substr( 3 );

	// find all books' title
	vector< std::pair<size_t,wstring> > vBooks;
	size_t uPos = 0;
	while( true )
	{
		auto pBook = HTMLParser::FindContentBetweenTag( rHtmlContent, m_BookTitleTag, uPos );
		if( pBook.first != wstring::npos )
		{
			vBooks.push_back( pBook );
			uPos = pBook.first + 1;
		}
		else
		{
			break;
		}
	}

	// get chapter link of each book
	vector<BookIndex> vBookLinks;
	for( size_t i = 0; i < vBooks.size(); ++ i )
	{
		// make sure the range of this book
		size_t uPos1 = vBooks[i].first, uPos2;
		if( i == vBooks.size() - 1 )
			uPos2 = rHtmlContent.length();
		else
			uPos2 = vBooks[i+1].first;

		// basic book data
		BookIndex mBook;
		mBook.m_sTitle	= sTitle + L" " + vBooks[i].second;
		mBook.m_sAuthor	= sAuthor;
		
		// get all chapter
		while( true )
		{
			auto pLink = HTMLParser::FindContentBetweenTag( rHtmlContent, m_BookChapterTag, uPos1 );
			if( pLink.first == wstring::npos || pLink.first > uPos2 )
			{
				break;
			}
			else
			{
				if( pLink.second == L"&nbsp;" )
					break;

				auto slink = HTMLParser::AnalyzeLink( pLink.second );
				if( slink )
				{
					mBook.m_vChapter.push_back( *slink );
				}
				uPos1 = pLink.first;
			}
		}
		vBookLinks.push_back( mBook );
	}
	return make_pair( sTitle, vBookLinks );
}

wstring Wenku8Cn::GetChapterContent( const wstring& rHtml )
{
	auto pContent = HTMLParser::FindContentBetweenTag( rHtml, m_ContentTag );
	if( pContent.first != wstring::npos )
	{
		boost::algorithm::replace_all( pContent.second, "<ul ", "<!--" );
		boost::algorithm::replace_all( pContent.second, "</ul>", " -->" );
		boost::algorithm::replace_all( pContent.second, "<br />\r\n<br />\r\n", "<BR />\r\n");

		return pContent.second;
	}

	return L"";
}

vector< pair<size_t,wstring> > Wenku8Cn::FindAllImage( const wstring& rHTML, size_t uPos )
{
	vector< pair<size_t,wstring> > vRes;
	size_t uStartPos = uPos;
	while( true )
	{
		auto pImg = HTMLParser::FindContentBetweenTag( rHTML, m_ImageTag, uStartPos );
		if( pImg.first == wstring::npos )
			break;
		else
		{
			vRes.push_back( pImg );
			uStartPos = pImg.first + pImg.second.size() + m_ImageTag.second.size();
		}
	}
	return vRes;
}
