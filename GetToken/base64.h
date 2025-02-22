// Mostly rip-off from a repo below (some modification for C++ concepts)
// https://github.com/tobiaslocker/base64
#pragma once
#ifndef BASE64_H
#define BASE64_H

#include <ranges>
#include <string>

namespace base64
{
    inline constexpr std::string_view base64_chars{ "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                                    "abcdefghijklmnopqrstuvwxyz"
                                                    "0123456789+/" };

    template<typename OutputBuffer, std::ranges::input_range R>
        requires std::same_as<std::ranges::range_value_t<R>, char>
    || std::same_as<std::ranges::range_value_t<R>, unsigned char>
        || std::same_as<std::ranges::range_value_t<R>, std::byte>
        OutputBuffer encode_into(R&& range)
    {
        auto counter = size_t{};
        auto bit_stream = std::uint32_t{};
        auto offset = size_t{};
        auto encoded = OutputBuffer{};
        auto begin = std::cbegin(range);
        auto end = std::cend(range);

        encoded.reserve(static_cast<size_t>(1.5 * static_cast<double>(std::distance(begin, end))));

        while (begin != end)
        {
            auto const num_val = static_cast<unsigned char>(*begin);
            offset = 16 - counter % 3 * 8;
            bit_stream += num_val << offset;

            if (offset == 16)
            {
                encoded.push_back(base64_chars[bit_stream >> 18 & 0x3f]);
            }

            if (offset == 8)
            {
                encoded.push_back(base64_chars[bit_stream >> 12 & 0x3f]);
            }

            if (offset == 0 && counter != 3)
            {
                encoded.push_back(base64_chars[bit_stream >> 6 & 0x3f]);
                encoded.push_back(base64_chars[bit_stream & 0x3f]);
                bit_stream = 0;
            }

            ++counter;
            ++begin;
        }

        if (offset == 16)
        {
            encoded.push_back(base64_chars[bit_stream >> 12 & 0x3f]);
            encoded.push_back('=');
            encoded.push_back('=');
        }

        if (offset == 8)
        {
            encoded.push_back(base64_chars[bit_stream >> 6 & 0x3f]);
            encoded.push_back('=');
        }

        return encoded;
    }

    template<std::ranges::input_range R>
    std::string to_base64(R&& range)
    {
        return encode_into<std::string>(range);
    }

    template<typename OutputBuffer>
        requires std::same_as<typename OutputBuffer::value_type, char>
    || std::same_as<typename OutputBuffer::value_type, unsigned char>
        || std::same_as<typename OutputBuffer::value_type, std::byte>
        OutputBuffer decode_into(std::string_view data)
    {
        using value_type = typename OutputBuffer::value_type;
        auto counter = size_t{};
        auto bit_stream = uint32_t{};
        auto decoded = OutputBuffer{};

        decoded.reserve(std::size(data));

        for (unsigned char c : data)
        {
            auto const num_val = base64_chars.find(c);

            if (num_val != std::string::npos)
            {
                auto const offset = 18 - counter % 4 * 6;
                bit_stream += static_cast<uint32_t>(num_val) << offset;

                if (offset == 12)
                {
                    decoded.push_back(static_cast<value_type>(bit_stream >> 16 & 0xff));
                }

                if (offset == 6)
                {
                    decoded.push_back(static_cast<value_type>(bit_stream >> 8 & 0xff));
                }

                if (offset == 0 && counter != 4)
                {
                    decoded.push_back(static_cast<value_type>(bit_stream & 0xff));
                    bit_stream = 0;
                }
            }
            else if (c != '=')
            {
                throw std::runtime_error{ "Invalid base64 encoded data" };
            }

            counter++;
        }
        return decoded;
    }

    // Convert from Base64
    // Note: "inline" to workaround program-wide ODR
    inline std::string from_base64(std::string_view data)
    {
        return decode_into<std::string>(data);
    }
} // namespace Base64
#endif
