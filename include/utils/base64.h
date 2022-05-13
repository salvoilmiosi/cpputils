#ifndef __BASE64_H__
#define __BASE64_H__

#include <vector>
#include <string>
#include <span>

std::string base64_encode(std::span<const std::byte> buf);

std::vector<std::byte> base64_decode(std::string const& encoded_string);

#endif