#pragma once

#include "dict.h"

namespace ff
{
	bool json_parse(std::string_view text, ff::dict& dict, const char** error_pos = nullptr);
	void json_write(const ff::dict& dict, std::ostream& output);
}
