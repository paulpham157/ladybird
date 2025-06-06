/*
 * Copyright (c) 2024-2025, Tim Flynn <trflynn89@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Array.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/QuickSort.h>
#include <LibUnicode/ICU.h>
#include <LibUnicode/TimeZone.h>

#include <unicode/timezone.h>
#include <unicode/ucal.h>

namespace Unicode {

static Optional<String> cached_system_time_zone;

String current_time_zone()
{
    if (cached_system_time_zone.has_value())
        return *cached_system_time_zone;

    UErrorCode status = U_ZERO_ERROR;

    auto time_zone = adopt_own_if_nonnull(icu::TimeZone::createDefault());
    if (!time_zone || *time_zone == icu::TimeZone::getUnknown())
        return "UTC"_string;

    icu::UnicodeString time_zone_id;
    time_zone->getID(time_zone_id);

    icu::UnicodeString time_zone_name;
    time_zone->getCanonicalID(time_zone_id, time_zone_name, status);

    if (icu_failure(status))
        return "UTC"_string;

    cached_system_time_zone = icu_string_to_string(time_zone_name);
    return *cached_system_time_zone;
}

void clear_system_time_zone_cache()
{
    cached_system_time_zone.clear();
}

ErrorOr<void> set_current_time_zone(StringView time_zone)
{
    auto time_zone_data = TimeZoneData::for_time_zone(time_zone);
    if (!time_zone_data.has_value())
        return Error::from_string_literal("Unable to find the provided time zone");

    icu::TimeZone::setDefault(time_zone_data->time_zone());
    clear_system_time_zone_cache();

    return {};
}

// https://github.com/unicode-org/icu/blob/main/icu4c/source/tools/tzcode/icuzones
static constexpr bool is_legacy_non_iana_time_zone(StringView time_zone)
{
    constexpr auto legacy_zones = to_array({
        "ACT"sv,
        "AET"sv,
        "AGT"sv,
        "ART"sv,
        "AST"sv,
        "BET"sv,
        "BST"sv,
        "Canada/East-Saskatchewan"sv,
        "CAT"sv,
        "CNT"sv,
        "CST"sv,
        "CTT"sv,
        "EAT"sv,
        "ECT"sv,
        "IET"sv,
        "IST"sv,
        "JST"sv,
        "MIT"sv,
        "NET"sv,
        "NST"sv,
        "PLT"sv,
        "PNT"sv,
        "PRT"sv,
        "PST"sv,
        "SST"sv,
        "US/Pacific-New"sv,
        "VST"sv,
    });

    if (time_zone.starts_with("SystemV/"sv))
        return true;

    return legacy_zones.contains_slow(time_zone);
}

static Vector<String> icu_available_time_zones(Optional<ByteString> const& region)
{
    UErrorCode status = U_ZERO_ERROR;

    char const* icu_region = region.has_value() ? region->characters() : nullptr;

    auto time_zone_enumerator = adopt_own_if_nonnull(icu::TimeZone::createTimeZoneIDEnumeration(UCAL_ZONE_TYPE_ANY, icu_region, nullptr, status));
    if (icu_failure(status))
        return { "UTC"_string };

    auto time_zones = icu_string_enumeration_to_list(move(time_zone_enumerator), [](char const* zone) {
        return !is_legacy_non_iana_time_zone({ zone, strlen(zone) });
    });

    quick_sort(time_zones);
    return time_zones;
}

Vector<String> const& available_time_zones()
{
    static auto time_zones = icu_available_time_zones({});
    return time_zones;
}

Vector<String> available_time_zones_in_region(StringView region)
{
    return icu_available_time_zones(region);
}

Optional<String> resolve_primary_time_zone(StringView time_zone)
{
    UErrorCode status = U_ZERO_ERROR;

    icu::UnicodeString iana_id;
    icu::TimeZone::getIanaID(icu_string(time_zone), iana_id, status);

    if (icu_failure(status))
        return {};

    return icu_string_to_string(iana_id);
}

Optional<TimeZoneOffset> time_zone_offset(StringView time_zone, UnixDateTime time)
{
    UErrorCode status = U_ZERO_ERROR;

    auto time_zone_data = TimeZoneData::for_time_zone(time_zone);
    if (!time_zone_data.has_value())
        return {};

    i32 raw_offset = 0;
    i32 dst_offset = 0;

    // We must clamp the time we provide to ICU such that the result of converting milliseconds to days fits in an i32.
    // Further, that conversion must still be valid after applying DST offsets to the time we provide.
    static constexpr auto min_time = (static_cast<UDate>(AK::NumericLimits<i32>::min()) + U_MILLIS_PER_DAY) * U_MILLIS_PER_DAY;
    static constexpr auto max_time = (static_cast<UDate>(AK::NumericLimits<i32>::max()) - U_MILLIS_PER_DAY) * U_MILLIS_PER_DAY;
    auto icu_time = clamp(static_cast<UDate>(time.milliseconds_since_epoch()), min_time, max_time);

    time_zone_data->time_zone().getOffset(icu_time, 0, raw_offset, dst_offset, status);
    if (icu_failure(status))
        return {};

    return TimeZoneOffset {
        .offset = AK::Duration::from_milliseconds(raw_offset + dst_offset),
        .in_dst = dst_offset == 0 ? TimeZoneOffset::InDST::No : TimeZoneOffset::InDST::Yes,
    };
}

}
