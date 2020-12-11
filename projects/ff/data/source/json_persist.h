#pragma once

#include "dict.h"

namespace ff
{
	ff::dict json_parse(std::string_view text, const char** error_pos = nullptr);
	void json_write(const ff::dict& dict, std::ostream& output);
}
