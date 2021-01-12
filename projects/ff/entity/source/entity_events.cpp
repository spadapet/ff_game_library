#include "pch.h"
#include "entity_events.h"

using namespace std::string_view_literals;

const size_t ff::entity_events::event_activated = ff::hash<std::string_view>()("ff::entity::event_activated"sv);
const size_t ff::entity_events::event_deactivated = ff::hash<std::string_view>()("ff::entity::event_deactivated"sv);
const size_t ff::entity_events::event_destroying = ff::hash<std::string_view>()("ff::entity::event_destroying"sv);
const size_t ff::entity_events::event_null = ff::hash<std::string_view>()("ff::entity::event_null"sv);
