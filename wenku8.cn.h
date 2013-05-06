#pragma once

// STL Header
#include <set>
#include <string>

class Wenku8Cn
{
public:
	bool CheckServer( const std::string& rServer );

	void AnalyzeIndexPage( std::string& rHtmlContent );
};

